#pragma once

#include <stdbool.h>

#define FEATURES_SIZE 80
#define NUM_CATEGORIES 3

// categories for audio classification
extern const char* const categories[];

/// <summary>
///     Smooths predictions by ensuring that the same prediction occurs
///     over multiple frames with a confidence exceeding the threshold.
/// </summary>
/// <param name="prediction">Integer representing current prediction.</param>
/// <param name="confidence">Current confidence in prediction.</param>
/// <returns>Overall confidence.</returns>
float smooth_prediction(int prediction, float confidence);

/// <summary>
///     Checks that everything is setup correction for prediction.
/// </summary>
/// <returns>true if successful, false for error.</returns>
bool check_predict_setup(void);

/// <summary>
///     Smooths predictions by ensuring that the same prediction occurs
///     over multiple frames with a confidence exceeding the threshold.
/// </summary>
/// <param name="prediction">Integer representing current prediction.</param>
/// <param name="confidence">Current confidence in prediction.</param>
/// <returns>Overall confidence.</returns>
void predict_single_frame(float* inputData, int* prediction, float* confidence);

/// <summary>
///     Loads the next frame of the prerecorded sample into the featurizer_input_buffer.
/// </summary>
/// <param name="featurizer_input_buffer">Buffer for storing next data frame.</param>
/// <returns>True if there is additional data to process, false otherwise.</returns>
bool prepare_prerecorded(float* featurizer_input_buffer);

void predict_prerecorded(void);

/// <summary>
///     Resets the classifier.
/// </summary>
void predict_reset(void);

/// <summary>
///     Resets the prerecorded data index.
///     Should be called before using prepare_prerecorded() the first time.
/// </summary>
void prerecorded_reset(void);

