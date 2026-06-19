#include <AudioToolbox/AudioToolbox.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include "sound_library.h"

#define SAMPLE_RATE 48000
#define BPM_DEFAULT 140
#define BEAT_SAMPLES(bpm) (SAMPLE_RATE * 60 / bpm / 2)
#define PATTERN_SAMPLES(bpm) (BEAT_SAMPLES(bpm) * 16)

const char *keyMapping = ";qjkaoeu',.p1234";  // Custom key-to-sample mapping
                                              //
typedef enum {
    MODE_NORMAL,
    MODE_BPM,
    MODE_WRITE,
    MODE_SELECT_INSTRUMENT
} Mode;

typedef struct {
    SoundLibrary soundLibrary;  // Loaded sample
    UInt32 patterns[16][16];
    int sampleCount;
    int pulseIndex;
    UInt32 patternSeq[128];
    int patternsLen;
    int activeSamplePos[16];
    int currentInstrument;
    int bpm;
    Mode mode;
    int isPlaying;
} MetronomeState;

pthread_mutex_t triggerLock = PTHREAD_MUTEX_INITIALIZER;

// Set terminal to unbuffered mode
void setUnbufferedInput() {
    struct termios newSettings;
    tcgetattr(STDIN_FILENO, &newSettings);
    newSettings.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);

    // Set stdin to non-blocking mode
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

// Restore terminal settings
void restoreBufferedInput() {
    struct termios newSettings;
    tcgetattr(STDIN_FILENO, &newSettings);
    newSettings.c_lflag |= (ICANON | ECHO); // Restore canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);
}

void togglePlay(MetronomeState *state) {
    state->isPlaying = !state->isPlaying;
    if (!state->isPlaying) {
        state->sampleCount = 0;
        state->pulseIndex = 0;
    }
    printf(state->isPlaying ? "Playing...\n" : "Stopped.\n");
}

void playSample(MetronomeState *state, int sampleIndex) {
    if (state->soundLibrary.samples[sampleIndex] == NULL) {
        return;
    }

    if (sampleIndex < 0 || sampleIndex >= state->soundLibrary.n_samples) {
        printf("Invalid sample index: %d\n", sampleIndex);
        return;
    }
    
    pthread_mutex_lock(&triggerLock);
    state->activeSamplePos[sampleIndex] = 0;  // Start playback from beginning
    pthread_mutex_unlock(&triggerLock);
}

// Audio callback
static OSStatus AudioCallback(void *inRefCon,
                              AudioUnitRenderActionFlags *ioActionFlags,
                              const AudioTimeStamp *inTimeStamp,
                              UInt32 inBusNumber,
                              UInt32 inNumberFrames,
                              AudioBufferList *ioData) {
    MetronomeState *state = (MetronomeState *)inRefCon;

    SoundLibrary *soundLibrary = &state->soundLibrary;

    float *buffer = (float *)ioData->mBuffers[0].mData;

    memset(buffer, 0, inNumberFrames * sizeof(float));

    int beat_samples = BEAT_SAMPLES(state->bpm);
    int pattern_samples = PATTERN_SAMPLES(state->bpm);

    for (UInt32 i = 0; i < inNumberFrames; i++) {

        int currentBeat = (state->sampleCount / beat_samples) % 16;
        int currentPattern = state->patternSeq[(state->sampleCount / pattern_samples) % state->patternsLen];

        if (state->isPlaying) {
            if (state->pulseIndex >= beat_samples) {
                state->pulseIndex -= beat_samples;
            }

            for (int instr = 0; instr < 16; ++instr) {
                for (int j = 0; j < 16; ++j) {
                    if (state->patterns[currentPattern][(currentBeat+16-j) % 16] & (1 << instr)) {
                        if (state->pulseIndex+j*beat_samples < soundLibrary->sampleLengths[instr]) {
                            buffer[i] += soundLibrary->samples[instr][state->pulseIndex+j*beat_samples];
                        }
                    }
                }
            }
            state->pulseIndex++;
            state->sampleCount++;
        }

        pthread_mutex_lock(&triggerLock);
        for (int instr = 0; instr < soundLibrary->n_samples; instr++) {
            if (state->activeSamplePos[instr] >= 0) {
                buffer[i] += soundLibrary->samples[instr][state->activeSamplePos[instr]];  // Mix in playing sample
                state->activeSamplePos[instr]++;  // Move to next sample position
                if (state->activeSamplePos[instr] >= soundLibrary->sampleLengths[instr]) {
                    state->activeSamplePos[instr] = -1;  // Stop playback
                }
            }
        }
        pthread_mutex_unlock(&triggerLock);
    }

    return noErr;
}

void *userInputThread(void *arg) {
    MetronomeState *state = (MetronomeState *)arg;
    setUnbufferedInput();

    while (1) {
        char key = getchar();

        if (key == '\x1B') {
            restoreBufferedInput();
            exit(0);
        }

        switch (state->mode) {
            case MODE_BPM:
                if (key == '[') {
                    state->bpm--;
                        printf("BPM: %d\n", state->bpm);
                } else if (key == ']') {
                    state->bpm++;
                        printf("BPM: %d\n", state->bpm);
                } else if (key == 'r') {
                    state->mode = MODE_NORMAL;
                    printf("Exited BPM mode.\n");
                }
                break;

            case MODE_SELECT_INSTRUMENT:
                {
                    char *keyPtr = strchr(keyMapping, key);
                    if (keyPtr) {
                        state->currentInstrument = keyPtr - keyMapping;
                        printf("Selected Instrument: %d\n", state->currentInstrument);
                        state->mode = MODE_NORMAL;
                    }
                }
                break;

            case MODE_WRITE:
                {
                    if (key == 'c') {
                        state->mode = MODE_NORMAL;
                        printf("Write mode deactivated. Press sample pad to play sample.\n");
                    }
                    char *keyPtr = strchr(keyMapping, key);
                    if (keyPtr) {
                        int beat = keyPtr - keyMapping;
                        int currentPattern = state->patternSeq[0];
                        state->patterns[currentPattern][beat] ^= (1 << state->currentInstrument);
                        printf("Toggled instrument %d on beat %d\n", state->currentInstrument, beat);
                    }
                }
                break;

            case MODE_NORMAL:
            default:
                if (key == 'n') {
                    togglePlay(state);
                } else if (key == 'r') {
                    state->mode = MODE_BPM;
                    printf("Entered BPM mode. Use ↑/↓ to adjust.\n");
                } else if (key == 'g') {
                    state->mode = MODE_SELECT_INSTRUMENT;
                    printf("Select an instrument...\n");
                } else if (key == 'c') {
                    state->mode = MODE_WRITE;
                    printf("Write mode active. Toggle beats with sample keys.\n");
                } else {
                    char *keyPtr = strchr(keyMapping, key);
                    if (keyPtr) {
                        int sampleIndex = keyPtr - keyMapping;
                        playSample(state, sampleIndex);
                    }
                }
                break;
        }
    }
}


int main() {
    AudioComponentDescription desc = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple
    };

    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    AudioUnit audioUnit;
    AudioComponentInstanceNew(comp, &audioUnit);

    // Configure format
    AudioStreamBasicDescription format = {0};
    format.mSampleRate = SAMPLE_RATE;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mFramesPerPacket = 1;
    format.mChannelsPerFrame = 1;
    format.mBitsPerChannel = 32;
    format.mBytesPerFrame = 4;
    format.mBytesPerPacket = 4;

    AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(format));

    // Load sample
    MetronomeState state = { .bpm = BPM_DEFAULT, .mode = MODE_NORMAL, .isPlaying = 0 };

    loadSounds(&state.soundLibrary, "samples");

    //state.sampleCount = 0;
    //state.pulseIndex = 0; // Prevent playback until first beat
    memset(state.patterns, 0, sizeof(state.patterns));

    // pattern b t t b k t t b t t bu t ku t t t
    state.patterns[0][0]  = 0x109;
    state.patterns[0][1]  = 0x8;
    state.patterns[0][2]  = 0x8;
    state.patterns[0][3]  = 0x8 | 0x1 | 0x0;
    state.patterns[0][4]  = 0x8 | 0x4;
    state.patterns[0][5]  = 0x8;
    state.patterns[0][6]  = 0x8;
    state.patterns[0][7]  = 0x8 | 0x1;
    state.patterns[0][8]  = 0x8;
    state.patterns[0][9]  = 0x8;
    state.patterns[0][10] = 0x8 | 0x1 | 0x0;
    state.patterns[0][11] = 0x8;
    state.patterns[0][12] = 0x8 | 0x4;
    state.patterns[0][13] = 0x8;
    state.patterns[0][14] = 0x8;
    state.patterns[0][15] = 0x8;

    // pattern b t t b k t t b t t bu t ku t t t
    state.patterns[1][0]  = 0x8 | 0x1;
    state.patterns[1][1]  = 0x8;
    state.patterns[1][2]  = 0x8;
    state.patterns[1][3]  = 0x8 | 0x1;
    state.patterns[1][4]  = 0x8 | 0x4;
    state.patterns[1][5]  = 0x8;
    state.patterns[1][6]  = 0x8;
    state.patterns[1][7]  = 0x8 | 0x1;
    state.patterns[1][8]  = 0x8;
    state.patterns[1][9]  = 0x8;
    state.patterns[1][10] = 0x8 | 0x1;
    state.patterns[1][11] = 0x8;
    state.patterns[1][12] = 0x8 | 0x4;
    state.patterns[1][13] = 0x8;
    state.patterns[1][14] = 0x8;
    state.patterns[1][15] = 0x8;

    state.patterns[2][0]  = 0x0 | 0x1;
    state.patterns[2][1]  = 0x0;
    state.patterns[2][2]  = 0x0;
    state.patterns[2][3]  = 0x0 | 0x1;
    state.patterns[2][4]  = 0x0 | 0x4;
    state.patterns[2][5]  = 0x0;
    state.patterns[2][6]  = 0x0;
    state.patterns[2][7]  = 0x0 | 0x1;
    state.patterns[2][8]  = 0x0;
    state.patterns[2][9]  = 0x0;
    state.patterns[2][10] = 0x0 | 0x1;
    state.patterns[2][11] = 0x0;
    state.patterns[2][12] = 0x0 | 0x4;
    state.patterns[2][13] = 0x0;
    state.patterns[2][14] = 0x0;
    state.patterns[2][15] = 0x0;

    // pattern b t t b k t t b t t bu t ku t t t
    state.patterns[3][0]  = 0x8 | 0x2 | 0x4 | 0x1;
    state.patterns[3][1]  = 0x8;
    state.patterns[3][2]  = 0x8;
    state.patterns[3][3]  = 0x8 | 0x2 | 0x4 | 0x1;
    state.patterns[3][4]  = 0x8;
    state.patterns[3][5]  = 0x8;
    state.patterns[3][6]  = 0x8 | 0x2 | 0x4 | 0x1;
    state.patterns[3][7]  = 0x8;
    state.patterns[3][8]  = 0x8;
    state.patterns[3][9]  = 0x8 | 0x2 | 0x4 | 0x1;
    state.patterns[3][10] = 0x8;
    state.patterns[3][11] = 0x8;
    state.patterns[3][12] = 0x8 | 0x2 | 0x4 | 0x1;
    state.patterns[3][13] = 0x8;
    state.patterns[3][14] = 0x8 | 0x2 | 0x4 | 0x1;
    state.patterns[3][15] = 0x8;

    state.patternSeq[0] = 0;
    state.patternSeq[1] = 2;
    state.patternSeq[2] = 2;
    state.patternSeq[3] = 2;
    state.patternSeq[4] = 1;
    state.patternSeq[5] = 1;
    state.patternSeq[6] = 1;
    state.patternSeq[7] = 3;
    state.patternSeq[8] = 0;
    state.patternSeq[9] = 0;
    state.patternSeq[10] = 0;
    state.patternSeq[11] = 0;
    state.patternsLen = 1;

    memset(state.activeSamplePos, -1, sizeof(state.activeSamplePos));
    //state.patternSeq[12] = 0;

    // Set callback
    AURenderCallbackStruct callback = { .inputProc = AudioCallback, .inputProcRefCon = &state };
    AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Global, 0, &callback, sizeof(callback));

    // Start audio
    AudioUnitInitialize(audioUnit);
    AudioOutputUnitStart(audioUnit);

    pthread_t inputThread;
    pthread_create(&inputThread, NULL, userInputThread, &state);
    //printf("Press keys [%s] to play samples, ESC to quit.\n");
    printf("Press ESC to quit. 'n' to start/stop, 'r' for BPM mode, 'g' to select instrument, 'c' for write mode.\n");

    pthread_join(inputThread, NULL);

    // Cleanup
    //free(state.sampleData);
    AudioOutputUnitStop(audioUnit);
    AudioUnitUninitialize(audioUnit);
    AudioComponentInstanceDispose(audioUnit);

    return 0;
}

