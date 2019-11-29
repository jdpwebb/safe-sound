#pragma once

#include <stdbool.h>
#include <iothub_client_core_common.h>
#include <azure_sphere_provisioning.h>

#define SCOPE_ID_LENGTH 20

extern const int IOT_DEFAULT_POLL_PERIOD;

/// <summary>
///		Sets the scope ID for the IoT Hub client.
/// </summary>
/// <param name="scope_id">Scope ID unique to your IoT Hub.</param>
void initialize_hub_client(const char* scope_id);

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
);

/// <summary>
///		Handles all the functionality necessary to ensuring communication with the IoT Hub
///		stays active, including refreshing authentication when necessary, calling callbacks
///		when there is an update	from the hub.
///
///		It should be called at least IOT_DEFAULT_POLL_PERIOD frequently.
/// </summary>
/// <param name="twin_callback">Function called when a device twin update is received.</param>
/// <param name="direct_method_callback">Function called when a direct method request is received.</param>
void iot_hub_update(IOTHUB_CLIENT_DEVICE_TWIN_CALLBACK twin_callback,
	IOTHUB_CLIENT_DEVICE_METHOD_CALLBACK_ASYNC direct_method_callback);

/// <summary>
///     Sends telemetry to IoT Hub
/// </summary>
/// <param name="data">Telemetry string to send.</param>
void send_telemetry(const char* data);

/// <summary>
///		Creates and enqueues an update to the Device Twin as a series of key-value properties
///     stored in the new_state string.
///		The report is not sent immediately, but it is sent on the next invocation of
///     iot_hub_update().
///	</summary>
/// <param name="new_state">The state updates as a string of a key-value pair.</param>
///	<returns>True if the operation is successful, false otherwise.</returns>
bool update_device_twin(unsigned char* new_state);

/// <summary>
///     Sends an update to the device twin.
/// </summary>
/// <param name="propertyName">The IoT Hub Device Twin property name.</param>
/// <param name="propertyValue">The IoT Hub Device Twin property value.</param>
void update_device_twin_bool(const char* propertyName, bool propertyValue);

/// <summary>
///     Sends an update to the device twin.
/// </summary>
/// <param name="propertyName">the IoT Hub Device Twin property name</param>
/// <param name="propertyValue">the IoT Hub Device Twin property value</param>
void update_device_twin_int(const char* propertyName, int propertyValue);

/// <summary>
///		Allows querying for the current authentication status.
/// </summary>
/// <returns>True if the IoT Hub has been successfully authenticated, false otherwise.</returns>
bool is_hub_authenticated(void);