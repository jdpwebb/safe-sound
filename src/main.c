#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include "epoll_timerfd_utilities.h"
#include <applibs/gpio.h>
#include <applibs/log.h>

#include "parson.h" // used to parse Device Twin messages.

// project-specific header files
#include "hw/safe_sound_hardware.h"
#include "common.h"
#include "record_audio.h"
#include "process_audio.h"
#include "azure_iot.h"
#include "event_utilities.h"

// This application uses machine learning to classify audio continuously.

// Forward declaration of functions
static int InitializeApp(void);
static int InitPeripheralsAndHandlers(void);
static void ClosePeripheralsAndHandlers(void);
static void ButtonTimerEventHandler(EventData* eventData);
static void AudioEventHandler(EventData* eventData);
static void AzureTimerEventHandler(EventData* eventData);
static void SimulateEvent(void);
static void HandlePrediction(int prediction, float confidence);
static void TwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char* payload,
	size_t payloadSize, void* userContextCallback);
static int DirectMethodCallback(const char* method_name, const unsigned char* payload,
	size_t size, unsigned char** response, size_t* response_size, void* userContextCallback);

// File descriptors - initialized to invalid value
static int buttonAGpioFd = -1;
static int buttonPollTimerFd = -1;
static int azureTimerFd = -1;
static int epollFd = -1;

// Button state variables
static GPIO_Value_Type buttonState = GPIO_Value_High;

// Audio variables
const float confidenceThresh = 0.95f;
AudioBuffer audioData;
const short debugAudioPeriod = 5;  // print debug info every 5 seconds
const short predictionCooloff = 5;  // only allow a prediction every 5 seconds
static struct timespec lastDebugCheck, lastPredictionTime;
static bool usePrerecorded = false;  // Specifies whether to use prerecorded audio

// General settings variables
static bool isArmed = true;  // Whether a new event should be reported

// Event handler data structures. Only the event handler field needs to be populated.
static EventData buttonEventData = { .eventHandler = &ButtonTimerEventHandler };
static EventData audioEventData = { .eventHandler = &AudioEventHandler };
static EventData azureEventData = { .eventHandler = &AzureTimerEventHandler };

/// <summary>
///     Main entry point for this application.
/// </summary>
int main(int argc, char* argv[])
{
	pthread_t tid;
	Log_Debug("INFO: Application starting.\n");

	if (InitializeApp() < 0) {
		terminationRequired = true;
	}

	// Start audio recording thread
	if (!terminationRequired
		&& pthread_create(&tid, NULL, RecordAudioThread, (void*)&audioData) != 0) {
		Log_Debug("ERROR: Microphone record thread creation failed.");
		terminationRequired = true;
	}

	// Use epoll to wait for events and trigger handlers, until an error or SIGTERM happens
	while (!terminationRequired) {
		terminationRequired = WaitForEventAndCallHandler(epollFd) != 0;
	}

	ClosePeripheralsAndHandlers();
	// Wait for audio recording thread to finish
	pthread_join(tid, NULL);
	Log_Debug("INFO: Application exiting.\n");
	return 0;
}

/// <summary>
///     Signal handler for termination requests. This handler must be async-signal-safe.
/// </summary>
static void TerminationHandler(int signalNumber)
{
	// Don't use Log_Debug here, as it is not guaranteed to be async-signal-safe.
	terminationRequired = true;
}

///	<summary>
///		Initializes audio buffers, prediction models, event history and peripherals.
///	</summary>
///	<returns>0 on success, -1 on failure</returns>
static int InitializeApp(void)
{
	if (!initialize_audio_buffer(&audioData)) {
		Log_Debug("ERROR: Failed to initialize the audio buffer.\n");
		return -1;
	}

	if (!check_predict_setup()) {
		Log_Debug("ERROR: Prediction setup failed.\n");
		return -1;
	}

	initialize_event_history();

	if (InitPeripheralsAndHandlers() != 0) {
		Log_Debug("ERROR: Initialization of peripherals failed.\n");
		return -1;
	}

	return 0;
}

/// <summary>
///     Set up SIGTERM termination handler, initialize peripherals, and set up event handlers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static int InitPeripheralsAndHandlers(void)
{
	// Register termination handler
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = TerminationHandler;
	sigaction(SIGTERM, &action, NULL);

	epollFd = CreateEpollFd();
	if (epollFd < 0) {
		return -1;
	}

	// Open button GPIO as input, and set up a timer to poll it
	buttonAGpioFd = GPIO_OpenAsInput(BUTTON_A);
	if (buttonAGpioFd < 0) {
		Log_Debug("ERROR: Could not open button GPIO: %s (%d).\n", strerror(errno), errno);
		return -1;
	}
	// Create a timer to check if the button was pressed
	struct timespec buttonPressCheckPeriod = { 0, 1000000 };
	buttonPollTimerFd =
		CreateTimerFdAndAddToEpoll(epollFd, &buttonPressCheckPeriod, &buttonEventData, EPOLLIN);
	if (buttonPollTimerFd < 0) {
		return -1;
	}

	// Register the file descriptor which specifies if there is new audio data to process
	clock_gettime(CLOCK_REALTIME, &lastDebugCheck);
	clock_gettime(CLOCK_REALTIME, &lastPredictionTime);
	int result = RegisterEventHandlerToEpoll(
		epollFd, audioData.dataAvailableFd, &audioEventData, EPOLLIN);
	if (result < 0) {
		return -1;
	}

	// Register Azure IoT Hub update handler which is called periodically
	struct timespec azureProcessPeriod = { IOT_DEFAULT_POLL_PERIOD, 0 };
	azureTimerFd =
		CreateTimerFdAndAddToEpoll(epollFd, &azureProcessPeriod, &azureEventData, EPOLLIN);
	if (azureTimerFd < 0) {
		return -1;
	}

	return 0;
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
	Log_Debug("INFO: Closing file descriptors.\n");
	CloseFdAndPrintError(azureTimerFd, "AzureTimer");
	CloseFdAndPrintError(buttonPollTimerFd, "ButtonPollTimer");
	CloseFdAndPrintError(buttonAGpioFd, "ButtonAGPIO");
	CloseFdAndPrintError(audioData.dataAvailableFd, "AudioDataAvailable");
	CloseFdAndPrintError(epollFd, "Epoll");
}

/// <summary>
///     Handle button timer event: if the button is pressed, change the LED blink rate.
/// </summary>
static void ButtonTimerEventHandler(EventData* eventData)
{
	if (ConsumeTimerFdEvent(buttonPollTimerFd) != 0) {
		terminationRequired = true;
		return;
	}

	// Check for a button press
	GPIO_Value_Type newButtonState;
	int result = GPIO_GetValue(buttonAGpioFd, &newButtonState);
	if (result != 0) {
		Log_Debug("ERROR: Could not read button GPIO: %s (%d).\n", strerror(errno), errno);
		terminationRequired = true;
		return;
	}

	// The button has just been pressed, feed the prerecorded data into the predictor.
	// The button has GPIO_Value_Low when pressed and GPIO_Value_High when released
	if (newButtonState != buttonState) {
		if (newButtonState == GPIO_Value_Low) {
			SimulateEvent();
		}
		buttonState = newButtonState;
	}
}

/// <summary>
///     Handle new audio event: a new audio frame has been recorded so process it.
/// </summary>
static void AudioEventHandler(EventData* eventData)
{
	if (ConsumeTimerFdEvent(audioData.dataAvailableFd) != 0) {
		terminationRequired = true;
		return;
	}

	// check if it's time to print debug info
	struct timespec currentTime;
	clock_gettime(CLOCK_REALTIME, &currentTime);
	if (currentTime.tv_sec - lastDebugCheck.tv_sec > debugAudioPeriod) {
		if (audioData.dropped_frames > 0) {
			Log_Debug("WARNING: Dropped %d frames in last %d seconds.\n",
				audioData.dropped_frames, currentTime.tv_sec - lastDebugCheck.tv_sec);
			audioData.dropped_frames = 0;
		}
		lastDebugCheck = currentTime;
	}

	// Read the next frame of data
	float featurizer_input[AUDIO_FRAME_SIZE];
	bool readResult = read_audio_buffer(&audioData, featurizer_input, AUDIO_FRAME_SIZE);
	if (usePrerecorded) {
		usePrerecorded = prepare_prerecorded(featurizer_input);
		if (!usePrerecorded) {
			predict_reset();
		}
	}
	else if (!readResult) {
		// no data to read
		return;
	}

	int prediction;  // prediction category (0 - num_categories)
	float confidence;  // confidence in prediction (0.0 - 1.0)
	predict_single_frame(featurizer_input, &prediction, &confidence);
	float overall_confidence = smooth_prediction(prediction, confidence);
	if (overall_confidence > confidenceThresh) {
		// call prediction handler
		HandlePrediction(prediction, overall_confidence);
	}
}

/// <summary>
///		Azure timer event:  Check connection status and send telemetry
/// </summary>
static void AzureTimerEventHandler(EventData* eventData)
{
	if (ConsumeTimerFdEvent(azureTimerFd) != 0) {
		terminationRequired = true;
		return;
	}

	iot_hub_update(TwinCallback, DirectMethodCallback);
}


/// <summary>
///		Simulates an event by feeding the prerecorded audio into the classifier.
/// </summary>
static void SimulateEvent(void)
{
	prerecorded_reset();
	predict_reset();
	usePrerecorded = true;
}

/// <summary>
///		Processes new predictions by adding each event to the event history
///		and notifying the IoT Hub.
/// </summary>
/// <param name="prediction">Prediction index</param>
/// <param name="confidence">Confidence value (0 - 1)</param>
static void HandlePrediction(int prediction, float confidence)
{
	// check if the cooloff period has ended
	struct timespec currentTime;
	clock_gettime(CLOCK_REALTIME, &currentTime);
	// Only process a new prediction at most every predictionCooloff seconds
	if (currentTime.tv_sec - lastPredictionTime.tv_sec > predictionCooloff && prediction != 0) {
		Log_Debug("INFO: Prediction: %s with confidence %.2f\n", categories[prediction], confidence);
		if (isArmed) {
			char event_string[EVENT_STRING_SIZE] = { 0 };
			// Create event string (stringified JSON object)
			bool success = construct_event_message(event_string,
				sizeof(event_string), categories[prediction], confidence);
			if (success) {
				// Send event to the IoT Hub
				send_telemetry(event_string);
				save_event(event_string);
				// Update event history in the device twin
				char history_string[EVENT_HISTORY_BYTE_SIZE];
				construct_history_message(history_string, sizeof(history_string));
				if (!update_device_twin((unsigned char*)history_string)) {
					Log_Debug("ERROR: Failed to set reported state for eventHistory.\n");
				}
				else {
					Log_Debug("INFO: Reported state for eventHistory accepted by IoTHubClient.\n");
				}
			}
		}
		lastPredictionTime = currentTime;
	}
}

/// <summary>
///     Callback invoked when a Device Twin update is received from IoT Hub.
///     Updates local state for 'showEvents' (bool).
/// </summary>
/// <param name="payload">Contains the Device Twin JSON document (desired and reported).</param>
/// <param name="payloadSize">Size of the Device Twin JSON document.</param>
static void TwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char* payload,
	size_t payloadSize, void* userContextCallback)
{
	size_t nullTerminatedJsonSize = payloadSize + 1;
	char* nullTerminatedJsonString = (char*)malloc(nullTerminatedJsonSize);
	if (nullTerminatedJsonString == NULL) {
		Log_Debug("ERROR: Could not allocate buffer for twin update payload.\n");
		abort();
	}

	// Copy the provided buffer to a null terminated buffer.
	memcpy(nullTerminatedJsonString, payload, payloadSize);
	// Add the null terminator at the end.
	nullTerminatedJsonString[nullTerminatedJsonSize - 1] = 0;
	Log_Debug(nullTerminatedJsonString);
	JSON_Value* rootProperties = NULL;
	rootProperties = json_parse_string(nullTerminatedJsonString);
	if (rootProperties == NULL) {
		Log_Debug("WARNING: Cannot parse the string as JSON content.\n");
		goto cleanup;
	}

	JSON_Object* rootObject = json_value_get_object(rootProperties);
	JSON_Object* desiredProperties = json_object_dotget_object(rootObject, "desired");
	if (desiredProperties == NULL) {
		desiredProperties = rootObject;
	}

	// Handle the Device Twin Desired Properties here.
	int armedState = json_object_get_boolean(desiredProperties, "armed");
	if (armedState != -1) {
		isArmed = (bool)armedState;
		if (armedState) {
			Log_Debug("INFO: Arming the security system.\n");
		}
		else {
			Log_Debug("INFO: Disarming the security system.\n");
		}
		update_device_twin_bool("armed", isArmed);
	}

cleanup:
	// Release the allocated memory.
	json_value_free(rootProperties);
	free(nullTerminatedJsonString);
}

/// <summary>
///     Callback when direct method is called.
/// </summary>
static int DirectMethodCallback(const char* method_name, const unsigned char* payload,
	size_t size, unsigned char** response, size_t* response_size, void* userContextCallback)
{
	(void)userContextCallback;
	(void)payload;
	(void)size;

	int result;

	if (strcmp("simulateEvent", method_name) == 0)
	{
		const char deviceMethodResponse[] = "{ \"Response\": \"Simulating window break event\" }";
		*response_size = sizeof(deviceMethodResponse) - 1;
		*response = malloc(*response_size);
		(void)memcpy(*response, deviceMethodResponse, *response_size);
		result = 200;
		SimulateEvent();
	}
	else
	{
		// All other entries are ignored.
		const char deviceMethodResponse[] = "{ }";
		*response_size = sizeof(deviceMethodResponse) - 1;
		*response = malloc(*response_size);
		(void)memcpy(*response, deviceMethodResponse, *response_size);
		result = -1;
	}

	return result;
}
