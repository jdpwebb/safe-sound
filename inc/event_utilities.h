#pragma once

#include <stdbool.h>
#include <stdio.h>

static const char HISTORY_FORMAT_BEGIN[] = "{\"eventHistory\":{";
static const char HISTORY_FORMAT_END[] = "}}";

#define EVENT_HISTORY_SIZE 3  // number of events to keep
// Size of buffer needed for the event string
#define EVENT_STRING_SIZE 85
// Size of buffer needed for the event history string
// Add 5 to each event row to account for key index (i.e. "0":) and comma at end of line
#define EVENT_HISTORY_BYTE_SIZE (EVENT_STRING_SIZE + 5) * EVENT_HISTORY_SIZE \
								+ sizeof(HISTORY_FORMAT_BEGIN) + sizeof(HISTORY_FORMAT_END)

///	<summary>
///		Initializes the event history arrays.
///		This function should be called once before running construct_history_message.
/// </summary>
void initialize_event_history(void);

/// <summary>
///		Add an event to the event history buffer.
///	</summary>
///	<param name="event_string">
///		String representing an event.
///		The string should be formatted with construct_event_message().
///	</param>
void save_event(const char* event_string);

/// <summary>
///		Creates a formatted string representing the event as a JSON object.
///		The generated JSON object consists of three key-value properties:
///			"eventType": specifies a string with the event category
///			"confidence": a value from 0 - 1 representing the prediction confidence
///			"eventTime": the time the event occurred represented using seconds since epoch
///		The stringified JSON is then stored in buffer.
///	</summary>
/// <param name="buffer">Array that the event string is stored in.</param>
/// <param name="buf_size">
///		Byte size of the buffer parameter.
///		This should be at least EVENT_STRING_SIZE large.
///	</param>
/// <param name="event_type">String of event category.</param>
///	<param name="confidence">Confidence in event prediction (0 - 1).</param>
/// <returns>True on success, false on failure.</returns>
bool construct_event_message(
	char* buffer, size_t buf_size, const char* event_type, float confidence
);

/// <summary>
///		Creates a formatted string representing the history buffer as a JSON object.
///		Since Azure IoT Hub does not allow arrays, each event row is represented as
///		a separate object with its index (in string form) as its key, i.e. "0".
///		Each event row should have been formatted with construct_event_message().
///		See that function for details on the event information.
///	</summary>
///	<param name="buffer">Array that the formatted history string is put in.</param>
///	<param name="buf_size">
///		Byte size of the buffer parameter.
///		This should be at least EVENT_HISTORY_BYTE_SIZE large.
///	</param>
///	<returns>True on success, false on failure.</returns>
bool construct_history_message(char* buffer, size_t buf_size);