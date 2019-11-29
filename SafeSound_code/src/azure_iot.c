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
static char scopeId[SCOPE_ID_LENGTH];

const int IOT_DEFAULT_POLL_PERIOD = 5;

// Azure IoT Hub variables
static IOTHUB_DEVICE_CLIENT_LL_HANDLE iothubClientHandle = NULL;
static const int keepalivePeriodSeconds = 20;
static bool iothubAuthenticated = false;

// Azure IoT check setup periods
static const int AzureIoTMinReconnectPeriodSeconds = 60;
static const int AzureIoTMaxReconnectPeriodSeconds = 10 * 60;

static int azureIoTPollPeriodSeconds = IOT_DEFAULT_POLL_PERIOD;
static struct timespec lastReconnectTry = { 0, 0 };

static const char* get_reason_string(IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason);
static const char* get_azure_sphere_provisioning_result_string(
	AZURE_SPHERE_PROV_RETURN_VALUE provisioningResult
);

/// <summary>
///     Sets the IoT Hub authentication state for the app
///     The SAS Token expires which will set the authentication state
/// </summary>
static void hub_connection_status_callback(IOTHUB_CLIENT_CONNECTION_STATUS result,
	IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason,
	void* userContextCallback)
{
	iothubAuthenticated = (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED);
	Log_Debug("INFO: IoT Hub Authenticated: %s\n", get_reason_string(reason));
}

/// <summary>
///		Sets the scope ID for the IoT Hub client.
/// </summary>
/// <param name="scope_id">Scope ID unique to your IoT Hub.</param>
void initialize_hub_client(const char* scope_id)
{
	strncpy(scopeId, scope_id, SCOPE_ID_LENGTH);
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
bool setup_hub_client(IOTHUB_CLIENT_DEVICE_TWIN_CALLBACK twin_callback,
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
	Log_Debug("INFO: Device provisioning returned '%s'.\n",
		get_azure_sphere_provisioning_result_string(provResult));

	if (provResult.result != AZURE_SPHERE_PROV_RESULT_OK) {

		// If we fail to connect, reduce the polling frequency, starting at
		// AzureIoTMinReconnectPeriodSeconds and with a backoff up to
		// AzureIoTMaxReconnectPeriodSeconds
		if (azureIoTPollPeriodSeconds == IOT_DEFAULT_POLL_PERIOD) {
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
	azureIoTPollPeriodSeconds = IOT_DEFAULT_POLL_PERIOD;

	iothubAuthenticated = true;

	if (IoTHubDeviceClient_LL_SetOption(iothubClientHandle, OPTION_KEEP_ALIVE,
		&keepalivePeriodSeconds) != IOTHUB_CLIENT_OK) {
		Log_Debug("ERROR: Failure setting option \"%s\"\n", OPTION_KEEP_ALIVE);
		return false;
	}

	IoTHubDeviceClient_LL_SetDeviceTwinCallback(iothubClientHandle, twin_callback, NULL);
	// Tell the system about the callback function to call when we receive a Direct Method message from Azure
	IoTHubDeviceClient_LL_SetDeviceMethodCallback(iothubClientHandle, direct_method_callback, NULL);
	IoTHubDeviceClient_LL_SetConnectionStatusCallback(iothubClientHandle,
		hub_connection_status_callback, NULL);
	return true;
}

void iot_hub_update(IOTHUB_CLIENT_DEVICE_TWIN_CALLBACK twin_callback,
	IOTHUB_CLIENT_DEVICE_METHOD_CALLBACK_ASYNC direct_method_callback)
{
	bool isNetworkReady = false;
	if (Networking_IsNetworkingReady(&isNetworkReady) != -1) {
		if (isNetworkReady && !iothubAuthenticated) {
			setup_hub_client(twin_callback, direct_method_callback);
		}
	}
	else {
		Log_Debug("ERROR: Failed to get Network state\n");
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
void send_telemetry(const char* eventBuffer)
{
	Log_Debug("INFO: Sending IoT Hub message.\n", eventBuffer);

	IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromString(eventBuffer);

	if (messageHandle == 0) {
		Log_Debug("WARNING: unable to create a new IoTHubMessage\n");
		return;
	}

	if (IoTHubDeviceClient_LL_SendEventAsync(iothubClientHandle, messageHandle, NULL,
		/*&callback_param*/ 0) != IOTHUB_CLIENT_OK) {
		Log_Debug("WARNING: failed to hand over the message to IoTHubClient.\n");
	}

	IoTHubMessage_Destroy(messageHandle);
}

bool update_device_twin(unsigned char* new_state)
{
	if (!is_hub_authenticated()) {
		Log_Debug("ERROR: Client not authenticated.\n");
		return false;
	}
	return IoTHubDeviceClient_LL_SendReportedState(
		iothubClientHandle, new_state,
		strlen(new_state), NULL, 0) == IOTHUB_CLIENT_OK;
}

/// <summary>
///     Sends an update to the device twin.
/// </summary>
/// <param name="propertyName">the IoT Hub Device Twin property name</param>
/// <param name="propertyValue">the IoT Hub Device Twin property value</param>
void update_device_twin_bool(const char* propertyName, bool propertyValue)
{
	static char reportedPropertiesString[30] = { 0 };
	int len = snprintf(reportedPropertiesString, 30, "{\"%s\":%s}", propertyName,
		(propertyValue == true ? "true" : "false"));
	if (len < 0) {
		Log_Debug("ERROR: Couldn't create string for update_device_twin_bool.\n");
		return;
	}

	if (!update_device_twin((unsigned char*)reportedPropertiesString)) {
		Log_Debug("ERROR: failed to set reported state for '%s'.\n", propertyName);
	}
	else {
		Log_Debug("INFO: Reported state for '%s' set to '%s'.\n", propertyName,
			(propertyValue == true ? "true" : "false"));
	}
}

/// <summary>
///     Sends an update to the device twin.
/// </summary>
/// <param name="propertyName">the IoT Hub Device Twin property name</param>
/// <param name="propertyValue">the IoT Hub Device Twin property value</param>
void update_device_twin_int(const char* propertyName, int propertyValue)
{
	static char reportedPropertiesString[30] = { 0 };
	int len = snprintf(reportedPropertiesString, 30, "{\"%s\":%d}", propertyName,
		propertyValue);
	if (len < 0) {
		Log_Debug("ERROR: Couldn't create string for update_device_twin_int.\n");
		return;
	}

	if (!update_device_twin((unsigned char*)reportedPropertiesString)) {
		Log_Debug("ERROR: failed to set reported state for '%s'.\n", propertyName);
	}
	else {
		Log_Debug("INFO: Reported state for '%s' set to '%d'.\n", propertyName,
			propertyValue);
	}
}

/// <summary>
///     Converts the IoT Hub connection status reason to a string.
/// </summary>
static const char* get_reason_string(IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason)
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
static const char* get_azure_sphere_provisioning_result_string(
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
bool is_hub_authenticated()
{
	return iothubAuthenticated;
}