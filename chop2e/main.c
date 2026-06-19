// main.c
#include "chop2e.h"
#include "stdio.h"

pthread_mutex_t triggerLock = PTHREAD_MUTEX_INITIALIZER;

void togglePlay(MetronomeState *state) {
    state->isPlaying = !state->isPlaying;
    if (!state->isPlaying) {
        state->sampleCount = 0;
        state->pulseIndex = 0;
        state->lights.rightlights[9] = GRAY;
    } else {
        state->lights.rightlights[9] = RED;
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

    atomic_store(&state->activeSamplePos[sampleIndex], 0);
    
    pthread_mutex_lock(&triggerLock);
    state->activeSamplePos[sampleIndex] = 0;  // Start playback from beginning
    pthread_mutex_unlock(&triggerLock);
}

void saveStateToFile(MetronomeState *state, const char *path) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        printf("Could not open file %s\n", path);
        return;
    }

    SavedMetronomeState saved = {
        .bpm = state->bpm,
        .isPlaying = state->isPlaying,
        .sampleCount = state->sampleCount,
        .pulseIndex = state->pulseIndex,
        .patternsLen = state->patternsLen,
        .currentInstrument = state->currentInstrument,
    };

    memcpy(saved.patterns, state->patterns, sizeof saved.patterns);
    memcpy(saved.patternSeq, state->patternSeq, sizeof saved.patternSeq);

    fwrite(&saved, sizeof saved, 1, file);
    fclose(file);
}

void loadStateFromFile(MetronomeState *state, const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("Could not open file %s\n", path);
        return;
    }

    SavedMetronomeState saved;
    if (fread(&saved, sizeof saved, 1, file) != 1) {
        printf("Could not read state from %s\n", path);
        fclose(file);
        return;
    }

    fclose(file);

    state->bpm = saved.bpm;
    state->isPlaying = saved.isPlaying;
    state->sampleCount = saved.sampleCount;
    state->pulseIndex = saved.pulseIndex;
    state->patternsLen = saved.patternsLen;
    state->currentInstrument = saved.currentInstrument;

    memcpy(state->patterns, saved.patterns, sizeof saved.patterns);
    memcpy(state->patternSeq, saved.patternSeq, sizeof saved.patternSeq);
}

void switch_pattern(MetronomeState *state) {
    // check if we are onto the next patttern; if we are, switch to the new pattern chain
    // we subtract one because both might be zero if we set it from a pasued state
    int beat_samples = BEAT_SAMPLES(state->bpm);

    for (int i = 0; i < state->patternsLenTemp; ++i) {
        state->patternSeq[i] = state->patternSeqTemp[i];
    }
    state->patternsLen = state->patternsLenTemp;
    state->patternsLenTemp = 0;
    state->writtenOn = -1;

    state->sampleCount %= beat_samples;
}

/*
void deriveLightsFromState(Lights *lights, MetronomeState *state) {
    printf("DERIVING...\n");
    SoundLibrary *soundLibrary = &state->soundLibrary;

    int beat_samples = BEAT_SAMPLES(state->bpm);
    int pattern_samples = PATTERN_SAMPLES(state->bpm);
    int currentBeat = (state->sampleCount / beat_samples) % 16;
    int currentPattern = state->patternSeq[(state->sampleCount / pattern_samples) % state->patternsLen];

    for (int instr = 0; instr < 16; instr++) {
        for (int j = 0; j < 16; ++j) {
            if (state->activeSamplePos[instr] >= 0 || state->patterns[currentPattern][(currentBeat+16-j) % 16] & (1 << instr)) {
                lights->leftlights[instr] = PURPLE;
                printf("instr: %d\n", instr);
            } else {
                lights->leftlights[instr] = GOLD;
            }
        }

    }
}
*/

/*
static inline int beat_samples(const MetronomeState *state) {
    return SAMPLE_RATE * 60 / state->bpm / 2;
}

static inline int pattern_samples(const MetronomeState *state) {
    return beat_samples(state) * 16;
}
*/

static inline int current_beat(const MetronomeState *state) {
    return (state->sampleCount / BEAT_SAMPLES(state->bpm)) % 16;
}

static inline int current_pattern(const MetronomeState *state) {
    return state->patternSeq[(state->sampleCount / PATTERN_SAMPLES(state->bpm)) % state->patternsLen];
}

static inline int previous_pattern(const MetronomeState *state) {
    return state->patternSeq[((state->sampleCount / PATTERN_SAMPLES(state->bpm))-1) % state->patternsLen];
}

// Audio callback
static OSStatus ChopAudioCallback(void *inRefCon,
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

    float interp_factor[16] = { 0. };

    for (UInt32 i = 0; i < inNumberFrames; i++) {

        int currentBeat = current_beat(state);
        int currentPattern = current_pattern(state);

        if (state->writtenOn / PATTERN_SAMPLES(state->bpm) < state->sampleCount / PATTERN_SAMPLES(state->bpm)) {
//            state->lights.leftlights[currentPattern] = PURPLE;
//            state->lights.leftlights[previous_pattern(state)] = GRAY;

            if (state->writtenOn >= 0) {
                switch_pattern(state);
            }
        }


        if (state->isPlaying) {
            if (state->pulseIndex >= beat_samples) {
                state->pulseIndex -= beat_samples;
            }

            for (int instr = 0; instr < 16; ++instr) {
//                state->lights.leftlights[instr] = GRAY;
                /*
                for (int j = 0; j < 16; ++j) {
                    if (state->patterns[currentPattern][(currentBeat+16-j) % 16] & (1 << instr)) {
                        if (state->pulseIndex+j*beat_samples < soundLibrary->sampleLengths[instr]) {
                            buffer[i] += soundLibrary->samples[instr][state->pulseIndex+j*beat_samples];
                            interp_factor[instr] = 1 - (float)(state->pulseIndex+j*beat_samples) / (float)soundLibrary->sampleLengths[instr]; 
                        }
                    }
                }
                */

                for (int j = 0; j < 16; ++j) {
                    if (state->patterns[currentPattern][(currentBeat+16-j) % 16] & (1 << instr)) {
                        if (state->pulseIndex+j*beat_samples < soundLibrary->sampleLengths[instr]) {
                            buffer[i] += soundLibrary->samples[instr][state->pulseIndex+j*beat_samples];
                            interp_factor[instr] = 1 - (float)(state->pulseIndex+j*beat_samples) / (float)soundLibrary->sampleLengths[instr]; 
                            break;
                        }
                    }
                }
            }
            state->pulseIndex++;
            state->sampleCount++;
        }


        for (int instr = 0; instr < soundLibrary->n_samples; instr++) {
            int pos = atomic_load(&state->activeSamplePos[instr]);

            if (pos >= 0) {
                buffer[i] += soundLibrary->samples[instr][pos];
                pos++;
                if (pos >= soundLibrary->sampleLengths[instr]) {
                    atomic_store(&state->activeSamplePos[instr], -1);
                } else {
                    atomic_store(&state->activeSamplePos[instr], pos);
                    interp_factor[instr] = 1.0f - (float)pos / (float)soundLibrary->sampleLengths[instr];
                }
            }
            /*
            if (state->activeSamplePos[instr] >= 0) {
                buffer[i] += soundLibrary->samples[instr][state->activeSamplePos[instr]];  // Mix in playing sample
                state->activeSamplePos[instr]++;  // Move to next sample position
                if (state->activeSamplePos[instr] >= soundLibrary->sampleLengths[instr]) {
                    state->activeSamplePos[instr] = -1;  // Stop playback
                } else {
                    interp_factor[instr] = 1 - (float)state->activeSamplePos[instr] / (float)soundLibrary->sampleLengths[instr];
                }
            }
            */
        }
        buffer[i] /= 6.;

    }

    // determine light colors from state
    for (int instr = 0; instr < 16; ++instr) {
        Color defaultColor = GRAY;

        if (current_beat(state) == instr && state->isPlaying) {
            defaultColor = ORANGE;
        }

        for (int i = 0; i < state->numActiveModes; ++i) {
            switch (state->modestack[i]) {
                case MODE_NORMAL: break;
                case MODE_BPM: break;
                case MODE_WRITE: if (state->patterns[current_pattern(state)][instr] & (1<<state->currentInstrument)) { defaultColor = SKYBLUE; } break;
                case MODE_SELECT_INSTRUMENT: if (instr == state->currentInstrument) { defaultColor = GOLD; } break;
                case MODE_SELECT_PATTERN: if (instr == current_pattern(state)) { defaultColor = PURPLE; } break;
                default: break;
            }
        }
        state->lights.leftlights[instr] = ColorLerp(defaultColor, RED, interp_factor[instr]);
    }
    

    return noErr;
}

int main(void) {
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
    MetronomeState state = { .bpm = BPM_DEFAULT, .isPlaying = 0 };

    loadSounds(&state.soundLibrary, "samples/processed");

    //state.sampleCount = 0;
    //state.pulseIndex = 0; // Prevent playback until first beat
    //
    //
    // STATE STRUCT INITIALIZATION
    memset(state.patterns, 0, sizeof(state.patterns));
    for (int i = 0; i < 16; ++i) {
//        state.lights.leftlights[i] = GRAY;
        state.lights.rightlights[i] = GRAY;
    }

    state.patternsLen = 0;

    // pattern b t t b k t t b t t bu t ku t t t
    state.patterns[0][0]  = 0x1109;
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
    state.patternSeq[state.patternsLen++] = 0;
    

    state.patterns[1][0]  = 0x2209;
    state.patterns[1][1]  = 0x8;
    state.patterns[1][2]  = 0x8;
    state.patterns[1][3]  = 0x8 | 0x1 | 0x0;
    state.patterns[1][4]  = 0x8 | 0x4;
    state.patterns[1][5]  = 0x8;
    state.patterns[1][6]  = 0x8;
    state.patterns[1][7]  = 0x8 | 0x1;
    state.patterns[1][8]  = 0x8;
    state.patterns[1][9]  = 0x8;
    state.patterns[1][10] = 0x8 | 0x1 | 0x0;
    state.patterns[1][11] = 0x8;
    state.patterns[1][12] = 0x8 | 0x4;
    state.patterns[1][13] = 0x8;
    state.patterns[1][14] = 0x8;
    state.patterns[1][15] = 0x8;
    state.patternSeq[state.patternsLen++] = 1;

    state.patterns[2][0]  = 0x4109;
    state.patterns[2][1]  = 0x8;
    state.patterns[2][2]  = 0x8;
    state.patterns[2][3]  = 0x8 | 0x1 | 0x0;
    state.patterns[2][4]  = 0x8 | 0x4;
    state.patterns[2][5]  = 0x8;
    state.patterns[2][6]  = 0x8;
    state.patterns[2][7]  = 0x8 | 0x1;
    state.patterns[2][8]  = 0x8;
    state.patterns[2][9]  = 0x8;
    state.patterns[2][10] = 0x8 | 0x1 | 0x0;
    state.patterns[2][11] = 0x8;
    state.patterns[2][12] = 0x8 | 0x4;
    state.patterns[2][13] = 0x8;
    state.patterns[2][14] = 0x8;
    state.patterns[2][15] = 0x8;
    state.patternSeq[state.patternsLen++] = 2;
    

    state.patterns[3][0]  = 0x8209;
    state.patterns[3][1]  = 0x8;
    state.patterns[3][2]  = 0x8;
    state.patterns[3][3]  = 0x8 | 0x1 | 0x0;
    state.patterns[3][4]  = 0x8 | 0x4;
    state.patterns[3][5]  = 0x8;
    state.patterns[3][6]  = 0x8;
    state.patterns[3][7]  = 0x8 | 0x1;
    state.patterns[3][8]  = 0x8;
    state.patterns[3][9]  = 0x8;
    state.patterns[3][10] = 0x8 | 0x1 | 0x0;
    state.patterns[3][11] = 0x8;
    state.patterns[3][12] = 0x8 | 0x4;
    state.patterns[3][13] = 0x8;
    state.patterns[3][14] = 0x8;
    state.patterns[3][15] = 0x8;
    state.patternSeq[state.patternsLen++] = 3;
    state.patternsLenTemp = 0;

    memset(state.activeSamplePos, -1, sizeof(state.activeSamplePos));
    // STATE STRUCT INITIALIZATION END

    // Set callback
    AURenderCallbackStruct callback = { .inputProc = ChopAudioCallback, .inputProcRefCon = &state };
    AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Global, 0, &callback, sizeof(callback));

    // Start audio
    AudioUnitInitialize(audioUnit);
    AudioOutputUnitStart(audioUnit);

    InitWindow(800, 450, "SimpleSamplePad");
    SetTargetFPS(60);

    bool writing = false;

    state.numActiveModes = 0;
    state.modestack[state.numActiveModes++] = MODE_NORMAL;
    state.writtenOn = -1;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_K)) {
            togglePlay(&state);
        }

        if (IsKeyPressed(KEY_J)) {
            if (state.modestack[state.numActiveModes-1] == MODE_WRITE) {
                state.modestack[state.numActiveModes--] = MODE_INVALID;
                state.lights.rightlights[8] = GRAY;
            } else {
                state.modestack[state.numActiveModes++] = MODE_WRITE;
                state.lights.rightlights[8] = SKYBLUE;
            }
        }

        if (IsKeyPressed(KEY_L)) {
            state.lights.rightlights[10] = PURPLE;
            state.modestack[state.numActiveModes++] = MODE_SELECT_PATTERN;
        }
        if (IsKeyReleased(KEY_L)) {
            if (state.patternsLenTemp > 0) {
                if (state.isPlaying) {
                    state.writtenOn = state.sampleCount;
                } else {
                    switch_pattern(&state);
                }
            }


            state.lights.rightlights[10] = GRAY;
            state.modestack[state.numActiveModes--] = MODE_INVALID;
        }

        if (IsKeyPressed(KEY_N)) {
            state.lights.rightlights[12] = GOLD;
            // current instrument select
            state.modestack[state.numActiveModes++] = MODE_SELECT_INSTRUMENT;
        }
        if (IsKeyReleased(KEY_N)) {
            state.modestack[state.numActiveModes--] = MODE_INVALID;
            state.lights.rightlights[12] = GRAY;
        }

        if (IsKeyPressed(KEY_SEMICOLON)) {
            state.modestack[state.numActiveModes++] = MODE_BPM;
            state.lights.rightlights[11] = ORANGE;
        }
        if (IsKeyReleased(KEY_SEMICOLON)) {
            state.modestack[state.numActiveModes--] = MODE_INVALID;
            state.lights.rightlights[11] = GRAY;
        }

        if (IsKeyPressed(KEY_U) || IsKeyPressedRepeat(KEY_U)) {
            if (state.modestack[state.numActiveModes-1] == MODE_BPM) {
                state.bpm--;
            }
        }
        if (IsKeyPressed(KEY_I) || IsKeyPressedRepeat(KEY_I)) {
            if (state.modestack[state.numActiveModes-1] == MODE_BPM) {
                state.bpm++;
            }
        }

        const char *save_file = "./demo.bin";

        if (IsKeyPressed(KEY_O)) {
            loadStateFromFile(&state, save_file);
            state.lights.rightlights[6] = WHITE;
        }
        if (IsKeyReleased(KEY_O)) {
            state.lights.rightlights[6] = GRAY;
        }

        if (IsKeyPressed(KEY_P)) {
            saveStateToFile(&state, save_file);
            state.lights.rightlights[7] = WHITE;
        }
        if (IsKeyReleased(KEY_P)) {
            state.lights.rightlights[7] = GRAY;
        }


        for (int instr = 0; instr < 16; ++instr) {
            if (state.modestack[state.numActiveModes-1] == MODE_NORMAL) {
                if (IsKeyPressed(leftkeys[instr])) {
                    playSample(&state, instr);
                }
            } else if (state.modestack[state.numActiveModes-1] == MODE_SELECT_PATTERN) {
                if (IsKeyPressed(leftkeys[instr])) {
                    state.patternSeqTemp[state.patternsLenTemp++] = instr;
                }
            } else if (state.modestack[state.numActiveModes-1] == MODE_SELECT_INSTRUMENT) {
                if (IsKeyPressed(leftkeys[instr])) {
                    state.currentInstrument = instr;
                }
            } else if (state.modestack[state.numActiveModes-1] == MODE_WRITE) {
                if (IsKeyPressed(leftkeys[instr])) {
                    state.patterns[current_pattern(&state)][instr] ^= 1 << state.currentInstrument;
                }
                // See TODO
            } else if (state.modestack[state.numActiveModes-1] == MODE_INVALID) {
                printf("INVALID MODE\n");
                exit(1);
            }
        }

        DrawUi(&state);
    }

    CloseWindow();

    /*
    pthread_t inputThread;
    pthread_create(&inputThread, NULL, uiThread, &state);
    //printf("Press keys [%s] to play samples, ESC to quit.\n");
    printf("Press ESC to quit. 'n' to start/stop, 'r' for BPM mode, 'g' to select instrument, 'c' for write mode.\n");

    pthread_join(inputThread, NULL);
    */

    // Cleanup
    //free(state.sampleData);
    AudioOutputUnitStop(audioUnit);
    AudioUnitUninitialize(audioUnit);
    AudioComponentInstanceDispose(audioUnit);

    return 0;
}

