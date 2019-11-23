#pragma once

/// <summary>
///     Runs an infinite loop which records audio from the microphone in socket 1 and processes it
///     it to create features that can be input into the classifier.
/// </summary>
/// <param name="vargp">Used to pass the AudioBuffer struct.</param>
/// <returns>NULL</returns>
void* RecordAudioThread(void* vargp);