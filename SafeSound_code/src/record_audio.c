#include "record_audio.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <applibs/adc.h>
#include <applibs/log.h>

#include "epoll_timerfd_utilities.h"
#include "common.h"
#include "process_audio.h"

#include "hw/safe_sound_hardware.h"

static float rawAudioBuffer[AUDIO_FRAME_SIZE];
static short audioBufferIndex = 0;
static AudioBuffer* audioBuf = NULL;
static int threadEpollFd = -1;
static int adcControllerFd = -1;
static int microphonePollTimerFd = -1;

// ADC constants
// The size of a sample in bits
static int adcBitCount = -1;

/// <summary>
///     Takes a single reading from ADC channel1 every 62.5 microseconds,
///     and sends a notification that there is a new frame when it completes.
/// </summary>
static void MicrophoneRecordEventHandler(EventData* eventData)
{
	if (ConsumeTimerFdEvent(microphonePollTimerFd) != 0) {
		terminationRequired = true;
		return;
	}

	uint32_t value;
	int result = ADC_Poll(adcControllerFd, MICROPHONE, &value);
	if (result < -1) {
		Log_Debug("ERROR: ADC_Poll failed with error: %s (%d)\n", strerror(errno), errno);
		terminationRequired = true;
		return;
	}

	// scale adc reading from -1 to 1
	float sample = ((float)value * 2) / (float)((1 << adcBitCount) - 1) - 1.0f;

	rawAudioBuffer[audioBufferIndex] = sample;
	if (++audioBufferIndex == AUDIO_FRAME_SIZE) {
		audioBufferIndex = 0;
		// copy full raw buffer into audio buffers
		if (!write_audio_buffer(audioBuf, rawAudioBuffer, AUDIO_FRAME_SIZE)) {
			audioBuf->dropped_frames += 1;
		}
		else {
			// notify main loop that there is new data
			uint64_t increment_one = 1UL;
			if (write(audioBuf->dataAvailableFd, &increment_one, sizeof(increment_one)) < 0) {
				Log_Debug("ERROR: dataAvailableFd write failed.\n");
			}
		}
	}
}

struct EventData adcPollingEventData = { .eventHandler = &MicrophoneRecordEventHandler };

/// <summary>
///     Opens and initializes ADC channel 1 (which has the microphone attached),
///     and creates an event handler to measure the ADC value periodically.
/// </summary>
static int InitMicrophone(void)
{
	// create a separate epoll to avoid waking up main thread
	threadEpollFd = CreateEpollFd();
	if (threadEpollFd < 0) {
		Log_Debug("ERROR: Thread epoll error.\n");
		return -1;
	}

	Log_Debug("INFO: Opening ADC Controller.\n");
	adcControllerFd = ADC_Open(MICROPHONE_CONTROLLER);
	if (adcControllerFd < 0) {
		Log_Debug("ERROR: ADC_Open failed with error: %s (%d)\n", strerror(errno), errno);
		return -1;
	}

	adcBitCount = ADC_GetSampleBitCount(adcControllerFd, MICROPHONE);
	if (adcBitCount == -1) {
		Log_Debug("ERROR: ADC_GetSampleBitCount failed with error : %s (%d)\n", strerror(errno), errno);
		return -1;
	}
	if (adcBitCount == 0) {
		Log_Debug("ERROR: ADC_GetSampleBitCount returned sample size of 0 bits.\n");
		return -1;
	}
	Log_Debug("INFO: ADC sample bit count: %d.\n", adcBitCount);

	// record an audio sample every 1sec/AUDIO_SAMPLE_RATE = 1000000000ns/AUDIO_SAMPLE_RATE
	struct timespec adcCheckPeriod = { .tv_nsec = 1000000000 / AUDIO_SAMPLE_RATE};
	microphonePollTimerFd =
		CreateTimerFdAndAddToEpoll(threadEpollFd, &adcCheckPeriod, &adcPollingEventData, EPOLLIN);
	if (microphonePollTimerFd < 0) {
		Log_Debug("Bad ADC timer file descriptor.\n");
		return -1;
	}

	return 0;
}

/// <summary>
///    Closes all file descriptors opened in this thread
/// </summary>
static void CloseFileDescriptors(void)
{
	Log_Debug("INFO: Closing record audio thread file descriptors.\n");
	CloseFdAndPrintError(microphonePollTimerFd, "ADCTimer");
	CloseFdAndPrintError(adcControllerFd, "ADC");
	CloseFdAndPrintError(threadEpollFd, "ThreadEpoll");
}

/// <summary>
///     Runs an infinite loop which records audio from the microphone in socket 1 and processes it
///     it to create features that can be input into the classifier.
/// </summary>
/// <param name="vargp">Variable arguments; currently unused</param>
/// <returns>NULL</returns>
void* RecordAudioThread(void* vargp)
{
	Log_Debug("INFO: Starting record audio thread.\n");

	audioBuf = (AudioBuffer*)vargp;

	if (InitMicrophone() != 0) {
		terminationRequired = true;
	}

	while (!terminationRequired) {
		if (WaitForEventAndCallHandler(threadEpollFd) != 0) {
			terminationRequired = true;
		}
	}

	CloseFileDescriptors();
	return NULL;
}