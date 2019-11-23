#include "event_utilities.h"
#include <string.h>
#include <time.h>
#include <applibs/log.h>

static char event_history[EVENT_HISTORY_SIZE][EVENT_STRING_SIZE];
static size_t event_history_index = 0;

void initialize_event_history(void) {
	for (size_t i = 0; i < EVENT_HISTORY_SIZE; ++i) {
		strcpy(event_history[i], "");
	}
	event_history_index = 0;
}

void save_event(const char* event_string) {
	strncpy(event_history[event_history_index], event_string, EVENT_STRING_SIZE);
	event_history_index = (event_history_index + 1) % EVENT_HISTORY_SIZE;
}

bool construct_event_message(
	char* buffer, size_t buf_size, const char* event_type, float confidence
) {
	const char* EventMsgTemplate = "{\"eventType\":\"%s\",\"confidence\":%1.2f,\"eventTime\":%d}";
	struct timespec currentTime;
	clock_gettime(CLOCK_REALTIME, &currentTime);
	int len = snprintf(buffer, buf_size, EventMsgTemplate, event_type, confidence, currentTime.tv_sec);
	return len > 0;
}

bool construct_history_message(char* buffer, size_t buf_size) {
	// Ensure the given buffer is big enough.
	// Add enough size for each string plus comma (',') and key number ("0":)
	if (buf_size < (EVENT_STRING_SIZE + 5) * EVENT_HISTORY_SIZE
		+ sizeof(HISTORY_FORMAT_BEGIN) + sizeof(HISTORY_FORMAT_END)) {
		Log_Debug("ERROR: buffer is not big enough.\n");
		return false;
	}
	strcpy(buffer, HISTORY_FORMAT_BEGIN);
	size_t string_index = sizeof(HISTORY_FORMAT_BEGIN) - 1;
	// Copy each string from the event_history into the buffer
	// Start with the most recent event and then add older events
	size_t i = (event_history_index - 1 + EVENT_HISTORY_SIZE) % EVENT_HISTORY_SIZE;
	size_t event_key = 0;
	size_t count_processed = 0;
	while (count_processed < EVENT_HISTORY_SIZE) {
		++count_processed;
		if (strcmp(event_history[i], "") == 0) {
			// empty event so skip this one
			i = (i - 1 + EVENT_HISTORY_SIZE) % EVENT_HISTORY_SIZE;
			continue;
		}
		int len = snprintf(buffer + string_index, 5, "\"%d\":", event_key);
		if (len < 0) {
			Log_Debug("ERROR: snprintf in construct_history_message.\n");
			return false;
		}
		string_index += 4;
		strcpy(buffer + string_index, event_history[i]);
		string_index += strlen(event_history[i]);
		buffer[string_index++] = ',';  // append comma
		++event_key;
		i = (i - 1 + EVENT_HISTORY_SIZE) % EVENT_HISTORY_SIZE;
	}
	--string_index;  // remove last comma
	strcpy(buffer + string_index, HISTORY_FORMAT_END);
	return true;
}