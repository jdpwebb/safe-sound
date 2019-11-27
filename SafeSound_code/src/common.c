#include "common.h"

#include <sys/eventfd.h>
#include <string.h>

// Termination state
volatile sig_atomic_t terminationRequired = false;

bool initialize_audio_buffer(AudioBuffer* buf)
{
	buf->read_index = MAX_BUFFERS - 1;
	buf->write_index = 0;
	buf->buffer_size = AUDIO_FRAME_SIZE;
	buf->dropped_frames = 0;
	buf->dataAvailableFd = eventfd(0, EFD_SEMAPHORE);
	return buf->dataAvailableFd >= 0;
}

bool write_audio_buffer(AudioBuffer* buf, float* srcData, unsigned short srcSize)
{
	if (srcSize > buf->buffer_size) {
		return false;
	}
	if (buf->read_index == buf->write_index) {
		// no free buffer to write to
		return false;
	}
	// everything is good, copy the data
	memcpy(buf->buffers[buf->write_index], srcData, srcSize * sizeof(float));
	buf->write_index = (short)((buf->write_index + 1) % MAX_BUFFERS);
	return true;
}

bool read_audio_buffer(AudioBuffer* buf, float* destBuf, unsigned short destSize)
{
	if (destBuf == NULL || destSize < buf->buffer_size) {
		return false;
	}
	short next_index = (short)((buf->read_index + 1) % MAX_BUFFERS);
	if (next_index == buf->write_index) {
		// no new data to read
		return false;
	}
	memcpy(destBuf, buf->buffers[buf->read_index], destSize * sizeof(float));
	buf->read_index = next_index;
	return true;
}