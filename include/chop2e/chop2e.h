#pragma once

#include <AudioToolbox/AudioToolbox.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include "sound_library.h"
#include <stdatomic.h>

#include "raylib.h"

/// CONSTANTS AND FUNCTIONS FOR main.c ///
#define SAMPLE_RATE 48000
#define BPM_DEFAULT 70
#define BEAT_SAMPLES(bpm) (SAMPLE_RATE * 60 / bpm / 4)
#define PATTERN_SAMPLES(bpm) (BEAT_SAMPLES(bpm) * 16)

typedef enum {
    MODE_NORMAL,
    MODE_BPM,
    MODE_WRITE,
    MODE_SELECT_INSTRUMENT,
    MODE_SELECT_PATTERN,
    MODE_INVALID,
} Mode;

typedef struct Lights {
    Color leftlights[16];
    Color rightlights[16];
} Lights;

typedef struct {
    SoundLibrary soundLibrary;  // Loaded sample
    float sampleVolume[16];                            
    float masterVol;
    UInt32 patterns[16][16];
    int sampleCount;
    int pulseIndex;
    UInt32 patternSeq[128];
    int patternsLen;

    UInt32 patternSeqTemp[128];
    int patternsLenTemp;
    int writtenOn;

    _Atomic int activeSamplePos[16];
    int currentInstrument;
    int bpm;
    Mode modestack[16];
    int numActiveModes;
    int isPlaying;
    Lights lights;
} MetronomeState;

typedef struct SavedMetronomeState {
    int bpm;
    int isPlaying;
    int sampleCount;
    int pulseIndex;

    int patterns[16][16];
    int patternSeq[16];
    int patternsLen;

    int currentInstrument;
} SavedMetronomeState;

/// CONSTANTS AND FUNCTIONS FOR ui.c ///
#define PAD_SIZE 70
#define PAD_TEXT_SIZE 30
#define PAD_TEXT_PADDING_TL 10
#define GUTTER_WIDTH 10
#define PAD_MARGIN_LEFT 60
#define PAD_MARGIN_TOP 60
#define PSGW (PAD_SIZE+GUTTER_WIDTH)
#define INSET_BORDER_WIDTH 5

extern const char *leftchars;
extern const int leftkeys[];

extern const char *rightchars;
extern const int rightkeys[];

extern const char *leftlabels[];
extern const char *rightlabels[];

void DrawUi(MetronomeState *state);

// internal
// void DrawPad(int left, int top, const char chars[], const int keys[]);

void deriveLightsFromState(Lights *lights, MetronomeState *state);

