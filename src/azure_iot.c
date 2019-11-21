#include "azure_iot.h"
#include <stdio.h>
#include <time.h>

#include <applibs/log.h>
#include <applibs/networking.h>

// Azure IoT SDK
#include <iothub_device_client_ll.h>
#include <iothub_client_options.h>
#include <iothubtransportmqtt.h>
#include <iothub.h>

// Scope ID from Azure IoT Hub
static char scopeId[] = "XXXXXXXXXXX";

const int AzureIoTDefaultPollPeriodSeconds = 5;

// Azure IoT Hub variables
static IOTHUB_DEVICE_CLIENT_LL_HANDLE iothubClientHandle = NULL;
static const int keepalivePeriodSeconds = 20;
static bool iothubAuthenticated = false;

// Azure IoT poll periods
static const int AzureIoTMinReconnectPeriodSeconds = 60;
static const int AzureIoTMaxReconnectPeriodSeconds = 10 * 60;

static int azureIoTPollPeriodSeconds = AzureIoTDefaultPollPeriodSeconds;
static struct timespec lastReconnectTry = { 0, 0 };

/// <summary>
///     Sets the IoT Hub authentication state for the app
///     The SAS Token expires which will set the authentication state
/// </summary>
static void HubConnectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result,
	IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason,
	void* userContextCallback)
{
	iothubAuthenticated = (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED);
	Log_Debug("IoT Hub Authenticated: %s\n", GetReasonString(reason));
}

/// <summary>
///     Sets up the Azure IoT Hub connection (creates the iothubClientHandle)
///     When the SAS Token for a device expires the connection needs to be recreated
///     which is why this is not simply a one time call.
///
///		This function uses an exponential back-off to retry connecting to the IoT Hub.
/// </summary>
/// <param name="twin_callback">Function called when a device twin update is received.</param>
/// <param name="direct_method_callback">Function called when a direct method request is received.</param>
/// <returns>
///		True if setting up or refreshing the SAS Token was successful.
///		False if it isn't time to reconnect, or the reconnection failed.
///	</returns>
bool SetupAzureClient(IOTHUB_CLIENT_DEVICE_TWIN_CALLBACK twin_callback,
	IOTHUB_CLIENT_DEVICE_METHOD_CALLBACK_ASYNC direct_method_callback
	)
{
	// Check if it's time to attempt a reconnect
	struct timespec currentTime;
	clock_gettime(CLOCK_REALTIME, &currentTime);
	if (currentTime.tv_sec - lastReconnectTry.tv_sec < azureIoTPollPeriodSeconds) {
		// It's not time to attempt another reconnect
		return false;
	}
	lastReconnectTry = currentTime;

	if (iothubClientHandle != NULL)
		IoTHubDeviceClient_LL_Destroy(iothubClientHandle);

	AZURE_SPHERE_PROV_RETURN_VALUE provResult =
		IoTHubDeviceClient_LL_CreateWithAzureSphereDeviceAuthProvisioning(scopeId, 10000,
			&iothubClientHandle);
	Log_Debug("IoTHubDeviceClient_LL_CreateWithAzureSphereDeviceAuthProvisioning returned '%s'.\n",
		getAzureSphereProvisioningResultString(provResult));

	if (provResult.result != AZURE_SPHERE_PROV_RESULT_OK) {

		// If we fail to connect, reduce the polling frequency, starting at
		// AzureIoTMinReconnectPeriodSeconds and with a backoff up to
		// AzureIoTMaxReconnectPeriodSeconds
		if (azureIoTPollPeriodSeconds == AzureIoTDefaultPollPeriodSeconds) {
			azureIoTPollPeriodSeconds = AzureIoTMinReconnectPeriodSeconds;
		}
		else {
			azureIoTPollPeriodSeconds *= 2;
			if (azureIoTPollPeriodSeconds > AzureIoTMaxReconnectPeriodSeconds) {
				azureIoTPollPeriodSeconds = AzureIoTMaxReconnectPeriodSeconds;
			}
		}

		Log_Debug("ERROR: failure to create IoTHub Handle - will retry in %i seconds.\n",
			azureIoTPollPeriodSeconds);
		return false;
	}

	// Successfully connected, so make sure the polling frequency is back to the default
	azureIoTPollPeriodSeconds = AzureIoTDefaultPollPeriodSeconds;

	iothubAuthenticated = true;

	if (IoTHubDeviceClient_LL_SetOption(iothubClientHandle, OPTION_KEEP_ALIVE,
		&keepalivePeriodSeconds) != IOTHUB_CLIENT_OK) {
		Log_Debug("ERROR: failure setting option \"%s\"\n", OPTION_KEEP_ALIVE);
		return false;
	}

	IoTHubDeviceClient_LL_SetDeviceTwinCallback(iothubClientHandle, twin_callback, NULL);
	// Tell the system about the callback function to call when we receive a Direct Method message from Azure
	IoTHubDeviceClient_LL_SetDeviceMethodCallback(iothubClientHandle, direct_method_callback, NULL);
	IoTHubDeviceClient_LL_SetConnectionStatusCallback(iothubClientHandle,
		HubConnectionStatusCallback, NULL);
	return true;
}

void IoTHubUpdate(IOTHUB_CLIENT_DEVICE_TWIN_CALLBACK twin_callback,
	IOTHUB_CLIENT_DEVICE_METHOD_CALLBACK_ASYNC direct_method_callback) {
	bool isNetworkReady = false;
	if (Networking_IsNetworkingReady(&isNetworkReady) != -1) {
		if (isNetworkReady && !iothubAuthenticated) {
			SetupAzureClient(twin_callback, direct_method_callback);
		}
	}
	else {
		Log_Debug("Failed to get Network state\n");
	}

	if (iothubAuthenticated) {
		IoTHubDeviceClient_LL_DoWork(iothubClientHandle);
	}
}

/// <summary>
///     Sends telemetry to IoT Hub
/// </summary>
/// <param name="key">The telemetry item to update</param>
/// <param name="value">new telemetry value</param>
void SendTelemetry(const char* eventBuffer)
{
	Log_Debug("Sending IoT Hub Message: %s\n", eventBuffer);

	IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromString(eventBuffer);

	if (messageHandle == 0) {
		Log_Debug("WARNING: unable to create a new IoTHubMessage\n");
		return;
	}

	if (IoTHubDeviceClient_LL_SendEventAsync(iothubClientHandle, messageHandle, NULL,
		/*&callback_param*/ 0) != IOTHUB_CLIENT_OK) {
		Log_Debug("WARNING: failed to hand over the message to IoTHubClient\n");
	}
	else {
		Log_Debug("INFO: IoTHubClient accepted the message for delivery\n");
	}

	IoTHubMessage_Destroy(messageHandle);
}

bool UpdateDeviceTwin(unsigned char* new_state) {
	if (!isHubAuthenticated()) {
		Log_Debug("Client not authenticated.\n");
		return false;
	}
	return IoTHubDeviceClient_LL_SendReportedState(
		iothubClientHandle, new_state,
		strlen(new_state), NULL, 0) == IOTHUB_CLIENT_OK;
}

/// <summary>
///     Converts the IoT Hub connection status reason to a string.
/// </summary>
const char* GetReasonString(IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason)
{
	static char* reasonString = "unknown reason";
	switch (reason) {
	case IOTHUB_CLIENT_CONNECTION_EXPIRED_SAS_TOKEN:
		reasonString = "IOTHUB_CLIENT_CONNECTION_EXPIRED_SAS_TOKEN";
		break;
	case IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED:
		reasonString = "IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED";
		break;
	case IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL:
		reasonString = "IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL";
		break;
	case IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED:
		reasonString = "IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED";
		break;
	case IOTHUB_CLIENT_CONNECTION_NO_NETWORK:
		reasonString = "IOTHUB_CLIENT_CONNECTION_NO_NETWORK";
		break;
	case IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR:
		reasonString = "IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR";
		break;
	case IOTHUB_CLIENT_CONNECTION_OK:
		reasonString = "IOTHUB_CLIENT_CONNECTION_OK";
		break;
	}
	return reasonString;
}

/// <summary>
///     Converts AZURE_SPHERE_PROV_RETURN_VALUE to a string.
/// </summary>
const char* getAzureSphereProvisioningResultString(
	AZURE_SPHERE_PROV_RETURN_VALUE provisioningResult)
{
	switch (provisioningResult.result) {
	case AZURE_SPHERE_PROV_RESULT_OK:
		return "AZURE_SPHERE_PROV_RESULT_OK";
	case AZURE_SPHERE_PROV_RESULT_INVALID_PARAM:
		return "AZURE_SPHERE_PROV_RESULT_INVALID_PARAM";
	case AZURE_SPHERE_PROV_RESULT_NETWORK_NOT_READY:
		return "AZURE_SPHERE_PROV_RESULT_NETWORK_NOT_READY";
	case AZURE_SPHERE_PROV_RESULT_DEVICEAUTH_NOT_READY:
		return "AZURE_SPHERE_PROV_RESULT_DEVICEAUTH_NOT_READY";
	case AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR:
		return "AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR";
	case AZURE_SPHERE_PROV_RESULT_GENERIC_ERROR:
		return "AZURE_SPHERE_PROV_RESULT_GENERIC_ERROR";
	default:
		return "UNKNOWN_RETURN_VALUE";
	}
}

/// <summary>
///		Allows querying for the current authentication status.
/// </summary>
/// <returns>True if the IoT Hub has been successfully authenticated, false otherwise.</returns>
bool isHubAuthenticated() {
	return iothubAuthenticated;
}