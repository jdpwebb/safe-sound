#include "process_audio.h"
#include <stddef.h>
#include <stdlib.h>

#include <applibs/log.h>
#include <time.h>

#include "common.h"

#define MODEL_WRAPPER_DEFINED
#include "classifier.h"
#define MFCC_WRAPPER_DEFINED
#include "featurizer.h"
#include "window_break.h"

const char* const categories[] = {
	"background_noise",
	"gunshot",
	"window_break",
};

// Prediction variables
const float CONFIDENCE_THRESHOLD = 0.85f;
const int CONSECUTIVE_PREDICTION_THRESHOLD = 7;
float overall_inverse_confidence = 1;
int last_prediction = 0;
unsigned short num_same_prediction = 0;
int vad_signal = 0;
int prepared_recording_index = 0;
const int prepared_recording_rows = sizeof(sample_wav_data) / (AUDIO_FRAME_SIZE * sizeof(short));

bool check_predict_setup()
{
    Log_Debug("Prerecorded sample contains %d rows of 16-bit PCM data\n",
		prepared_recording_rows);

    int input_size = mfcc_GetInputSize(0);
    if (input_size != AUDIO_FRAME_SIZE)
    {
        Log_Debug("Expecting featurizer to take %d samples\n", AUDIO_FRAME_SIZE);
        return false;
    }
    int output_size = mfcc_GetOutputSize(0);
    Log_Debug("Featurizer input %d and output %d.\n", input_size, output_size);

    input_size = model_GetInputSize(0);
    if (input_size != output_size) {
        Log_Debug("Classifier input %d does not match featurizer output %d.\n",
			input_size, output_size);
        return false;
    }
    output_size = model_GetOutputSize(0);
    Log_Debug("Classifier input %d and output %d.\n", input_size, output_size);

    return true;
}

static int argmax(float* buffer, int len)
{
    int max = 0;
    float value = 0;
    for (int j = 0; j < len; j++)
    {
        float v = buffer[j];
        if (v > value) {
            value = v;
            max = j;
        }
    }
    return max;
}

float smooth_prediction(int prediction, float confidence) {
	if (confidence >= CONFIDENCE_THRESHOLD && prediction == last_prediction) {
		++num_same_prediction;
		overall_inverse_confidence *= (1.0f - confidence);
	}
	else {
		num_same_prediction = 0;
		overall_inverse_confidence = 1.0f;
	}
	last_prediction = prediction;
	if (num_same_prediction > CONSECUTIVE_PREDICTION_THRESHOLD) {
		// got a valid prediction
		float overall_confidence = 1.0f - overall_inverse_confidence;
		num_same_prediction = 0;
		overall_inverse_confidence = 1.0f;
		predict_reset();
		return overall_confidence;
	}
	return 0;
}

void predict_single_frame(float* inputData, int* prediction, float* confidence) {
	float classifier_input_buffer[FEATURES_SIZE];
	float classifier_output[NUM_CATEGORIES];
	mfcc_Filter(NULL, inputData, classifier_input_buffer);
	model_Predict(NULL, classifier_input_buffer, classifier_output);
	*prediction = argmax(classifier_output, NUM_CATEGORIES);
	*confidence = classifier_output[*prediction];
}

bool prepare_prerecorded(float* featurizer_input_buffer) {
	for (int j = 0; j < AUDIO_FRAME_SIZE; j++)
	{
		featurizer_input_buffer[j] = (float)sample_wav_data[prepared_recording_index][j] / 32768.0f;
	}
	++prepared_recording_index;
	// if there is still data to process, return true
	return prepared_recording_index < prepared_recording_rows;
}

void predict_prerecorded()
{
    predict_reset();
	int prediction;
	float confidence;
	float featurizer_input_buffer[AUDIO_FRAME_SIZE];
    // run the predictor on our static sample PCM data...
    int input_size = mfcc_GetInputSize(0);
    int rows = sizeof(sample_wav_data) / (AUDIO_FRAME_SIZE * sizeof(short));
    float best_confidence = 0;
    int best_prediction = 0;
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < input_size; j++)
        {
            featurizer_input_buffer[j] = (float)sample_wav_data[i][j] / 32768.0f;
        }
		predict_single_frame(featurizer_input_buffer, &prediction, &confidence);
        if (confidence > best_confidence)
        {
            best_confidence = confidence;
            best_prediction = prediction;
        }
    }

    Log_Debug("INFO: prediction is '%s' with confidence %f\n", categories[best_prediction], best_confidence);
}

void predict_reset()
{
	num_same_prediction = 0;
	overall_inverse_confidence = 1.0;
    mfcc_Reset();
    model_Reset();
}

void prerecorded_reset()
{
	prepared_recording_index = 0;
}