#pragma once

#include <stdbool.h>
#include <iothub_client_core_common.h>
#include <azure_sphere_provisioning.h>

extern const int AzureIoTDefaultPollPeriodSeconds;

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
);

/// <summary>
///		Handles all the functionality necessary to ensuring communication with the IoT Hub stays active,
///		including refreshing authentication when necessary, calling callbacks when there is an update
///		from the hub.
///
///		It should be called at least AzureIoTDefaultPollPeriodSeconds frequently.
/// </summary>
/// <param name="twin_callback">Function called when a device twin update is received.</param>
/// <param name="direct_method_callback">Function called when a direct method request is received.</param>
void IoTHubUpdate(IOTHUB_CLIENT_DEVICE_TWIN_CALLBACK twin_callback,
	IOTHUB_CLIENT_DEVICE_METHOD_CALLBACK_ASYNC direct_method_callback);

/// <summary>
///     Sends telemetry to IoT Hub
/// </summary>
/// <param name="key">The telemetry item to update</param>
/// <param name="value">new telemetry value</param>
void SendTelemetry(const char* data);

/// <summary>
///		Creates and enqueues a report containing the name and value pair of a Device Twin reported
///     property. The report is not sent immediately, but it is sent on the next invocation of
///     IoTHubDeviceClient_LL_DoWork().
///	</summary>
/// <param name="new_state">The state updates as a string of a key-value pair.</param>
///	<returns>True if the operation is successful, false otherwise.</returns>
bool UpdateDeviceTwin(unsigned char* new_state);

/// <summary>
///     Converts the IoT Hub connection status reason to a string.
/// </summary>
const char* GetReasonString(IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason);

///     Converts AZURE_SPHERE_PROV_RETURN_VALUE to a string.
/// </summary>
const char* getAzureSphereProvisioningResultString(
	AZURE_SPHERE_PROV_RETURN_VALUE provisioningResult
);

/// <summary>
///		Allows querying for the current authentication status.
/// </summary>
/// <returns>True if the IoT Hub has been successfully authenticated, false otherwise.</returns>
bool isHubAuthenticated(void);