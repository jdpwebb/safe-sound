#pragma once

#include <stdbool.h>
#include <signal.h>

#define AUDIO_FRAME_SIZE 512
#define AUDIO_SAMPLE_RATE 16000  // samples/sec

#define MAX_BUFFERS 10

// signal which controls when the application ends
extern volatile sig_atomic_t terminationRequired;

/// <summary>
/// Contains up to MAX_BUFFERS audio data chunks of AUDIO_FRAME_SIZE.
/// Use read_audio_buffer and write_audio_buffer functions to manipulate these structs.
/// </summary>
typedef struct AudioBuffer {
	float buffers[MAX_BUFFERS][AUDIO_FRAME_SIZE];
	short read_index;
	short write_index;
	short buffer_size;
	int dataAvailableFd;
	unsigned int dropped_frames;
} AudioBuffer;

/// <summary>
///     Sets read_index to MAX_BUFFERS - 1, write_index to 0,
///     buffer_size to AUDIO_FRAME_SIZE, dropped_frames to 0,
///     and initializes dataAvailableFd.
/// </summary>
/// <param name="buf">AudioBuffer to initialize.</param>
/// <returns>True if successful, false otherwise.</returns>
bool initialize_audio_buffer(AudioBuffer* buf);

/// <summary>
///     Copies the data in srcData into the write buffer in buf.
/// </summary>
/// <param name="buf">AudioBuffer to use.</param>
/// <param name="srcData">Data to copy into the write buffer.</param>
/// <param name="srcSize">Length of srcData.</param>
/// <returns>True if successful, false otherwise.</returns>
bool write_audio_buffer(AudioBuffer* buf, float* srcData, unsigned short srcSize);

/// <summary>
///     Increments read_index and copies the next data frame into destBuf.
/// </summary>
/// <param name="buf">AudioBuffer to use.</param>
///	<param name="destBuf">Plain float array to write the next frame into.</param>
/// <param name="destSize">Size of destBuf in bytes.</param>
/// <returns>Pointer to read buffer.</returns>
bool read_audio_buffer(AudioBuffer* buf, float* destBuf, unsigned short destSize);

