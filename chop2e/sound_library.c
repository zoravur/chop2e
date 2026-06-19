#include <stdio.h>
#include <stdlib.h>
#include "sound_library.h"

float *loadWavFile(const char *filepath, UInt32 *outSampleLength) {
    AudioFileID audioFile;
    OSStatus status = AudioFileOpenURL(CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)filepath, strlen(filepath), false), kAudioFileReadPermission, 0, &audioFile);
    if (status != noErr) {
        printf("Error opening file\n");
        return NULL;
    }

    UInt64 dataSize = 0;
    UInt32 size = sizeof(dataSize);
    AudioFileGetProperty(audioFile, kAudioFilePropertyAudioDataByteCount, &size, &dataSize);

    UInt32 numSamples = dataSize / sizeof(int16_t);  // Correct calculation for 16-bit PCM
    *outSampleLength = numSamples;

    int16_t *intBuffer = (int16_t *)malloc(dataSize);
    AudioFileReadBytes(audioFile, false, 0, (UInt32 *)&dataSize, intBuffer);
    AudioFileClose(audioFile);

    // Convert from 16-bit PCM to float (-1.0 to 1.0)
    float *buffer = (float *)malloc(numSamples * sizeof(float));

    for (UInt32 i = 0; i < numSamples; i++) {
        buffer[i] = intBuffer[i] / 32768.0f;
    }

    free(intBuffer);
    return buffer;
}

void loadSounds(SoundLibrary *soundLibrary, const char *sample_dir) {
    if (strlen(sample_dir) >= 256) {
        fprintf(stderr, "sample directory path must be 255 characters or fewer");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for sample arrays
    soundLibrary->samples = (float **)malloc(NUM_SAMPLES * sizeof(float *));
    soundLibrary->sampleLengths = (UInt32 *)malloc(NUM_SAMPLES * sizeof(UInt32));
    soundLibrary->n_samples = NUM_SAMPLES;

    if (!soundLibrary->samples || !soundLibrary->sampleLengths) {
        printf("Memory allocation failed for SoundLibrary\n");
        return;
    }

    char fpath[512] = {};
    for (UInt32 i = 0; i < NUM_SAMPLES; ++i) {
        snprintf(fpath, sizeof(fpath), "%s/%d.wav", sample_dir, i+1);

        soundLibrary->samples[i] = loadWavFile(fpath, &soundLibrary->sampleLengths[i]);

        if (!soundLibrary->samples[i]) {
            fprintf(stderr, "Failed to load sample: %s\n", fpath);
        }
    }
}
