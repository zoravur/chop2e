#ifndef SOUND_LIBRARY_H
#define SOUND_LIBRARY_H

#include <AudioToolbox/AudioToolbox.h>

#define NUM_SAMPLES 16

typedef struct {
    UInt32 n_samples;
    UInt32 *sampleLengths;
    float **samples;
} SoundLibrary;

void loadSounds(SoundLibrary *soundLibrary, const char *sample_dir);

#endif
