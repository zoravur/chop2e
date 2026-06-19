#include "chop2e.h"
#include <assert.h>

#define ICON_SIZE 16
#define PAD_SMALL_TEXT_SIZE 28
#define PAD_SMALL_TEXT_SCALE 0.5f

void DrawPad(int left, int top, const char chars[], const int keys[], Color borderLight[], const char *labels[]);

void DrawStatusBar(MetronomeState *state);

static Texture2D play_icon;
static Texture2D pause_icon;
static Texture2D stop_icon;
static Texture2D record_icon;
static Font barlow;

void InitUi() {
    static bool initialized = false;
    if (!initialized) {
        barlow = LoadFontEx("fonts/Barlow_Condensed/BarlowCondensed-Medium.ttf", PAD_SMALL_TEXT_SIZE, 0, 0);
        play_icon = LoadTexture("bitmaps/play.png");
        pause_icon = LoadTexture("bitmaps/pause.png");
        stop_icon = LoadTexture("bitmaps/stop.png");
        record_icon = LoadTexture("bitmaps/record.png");
        initialized = true;

    }
};

void DrawUi(MetronomeState *state) {
    Lights *lights = &state->lights;
    InitUi();
    BeginDrawing();
    ClearBackground(ColorLerp(DARKGRAY, BLACK, 0.8));
    DrawPad(PAD_MARGIN_LEFT, PAD_MARGIN_TOP, leftchars, leftkeys, lights->leftlights, leftlabels);
    DrawPad(800-PSGW*4-PAD_MARGIN_LEFT+GUTTER_WIDTH, PAD_MARGIN_TOP, rightchars, rightkeys, lights->rightlights, rightlabels);
    DrawStatusBar(state);
    EndDrawing();
}

void DrawPad(int left, int top, const char chars[], const int keys[], Color borderLight[], const char *labels[]) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            int idx = i*4+j;
            char buf[2] = "\0";
            buf[0] = chars[idx];
            int key = keys[idx];

            DrawRectangle(left+PSGW*j, top+PSGW*i, PAD_SIZE, PAD_SIZE, borderLight[i*4+j]);
            DrawRectangle(left+PSGW*j+INSET_BORDER_WIDTH, top+PSGW*i+INSET_BORDER_WIDTH, 
                    PAD_SIZE-2*INSET_BORDER_WIDTH, PAD_SIZE-2*INSET_BORDER_WIDTH, LIGHTGRAY);
            if (IsKeyPressed(key)) {
                DrawRectangle(left+PSGW*j, top+PSGW*i, PAD_SIZE, PAD_SIZE, MAROON);
                DrawRectangle(left+PSGW*j+INSET_BORDER_WIDTH, top+PSGW*i+INSET_BORDER_WIDTH, 
                        PAD_SIZE-2*INSET_BORDER_WIDTH, PAD_SIZE-2*INSET_BORDER_WIDTH, MAROON);
            }
//            } else if (IsKeyDown(key)) {
//                DrawRectangle(left+PSGW*j, top+PSGW*i, PAD_SIZE, PAD_SIZE, RED);
//                DrawRectangle(left+PSGW*j+INSET_BORDER_WIDTH, top+PSGW*i+INSET_BORDER_WIDTH, 
//                        PAD_SIZE-2*INSET_BORDER_WIDTH, PAD_SIZE-2*INSET_BORDER_WIDTH, MAROON);
//            } else {
//                DrawRectangle(left+PSGW*j, top+PSGW*i, PAD_SIZE, PAD_SIZE, GRAY);
//            }
            DrawText(buf, PAD_TEXT_PADDING_TL+left+PSGW*j, PAD_TEXT_PADDING_TL+top+PSGW*i, PAD_TEXT_SIZE, BLACK);
            DrawTextEx(barlow, labels[idx], (Vector2){PAD_TEXT_PADDING_TL+left+PSGW*j-1, PAD_TEXT_PADDING_TL+top+PSGW*i+40}, PAD_SMALL_TEXT_SIZE*PAD_SMALL_TEXT_SCALE, -0.25, BLACK);
        }
    }
}

void DrawPlayIcon(int x, int y, Color color) {
    Vector2 p1 = { x + ICON_SIZE * 0.2f, y + ICON_SIZE * 0.2f };
    Vector2 p2 = { x + ICON_SIZE * 0.2f, y + ICON_SIZE * 0.8f };
    Vector2 p3 = { x + ICON_SIZE * 0.8f, y + ICON_SIZE * 0.5f };
    DrawTriangle(p1, p2, p3, color);
}

void DrawPauseIcon(int x, int y, Color color) {
    float barWidth = ICON_SIZE * 0.2f;
    float barHeight = ICON_SIZE * 0.75f;
    float spacing = ICON_SIZE * 0.15f;

    DrawRectangle(x + ICON_SIZE * 0.2f, y + ICON_SIZE * 0.125f, barWidth, barHeight, color);
    DrawRectangle(x + ICON_SIZE * 0.6f, y + ICON_SIZE * 0.125f, barWidth, barHeight, color);
}

void DrawStopIcon(int x, int y, Color color) {
    float pad = ICON_SIZE * 0.15f;
    DrawRectangle(x + pad, y + 1.225*pad, ICON_SIZE - 2*pad, ICON_SIZE - 2*pad, color);
}

void DrawRecordIcon(int x, int y, Color color) {
    DrawCircle(x + ICON_SIZE / 2, y + ICON_SIZE / 2, ICON_SIZE * 0.375f, color);
}


// #define MAX_MODE_STRING 256

const char* mode_to_string(Mode mode) {
    switch (mode) {
        case MODE_NORMAL: return "NORMAL";
        case MODE_BPM: return "BPM";
        case MODE_WRITE: return "WRITE";
        case MODE_SELECT_INSTRUMENT: return "INSTR";
        case MODE_SELECT_PATTERN: return "PATTERN";
        case MODE_INVALID: return "INVALID";
        default: return "UNKNOWN";
    }
}

void build_mode_string(const Mode *modes,
                       int count,
                       char *out,
                       size_t out_sz)
{
    size_t off = 0;                     // next write position
    if (out_sz == 0) return;
    out[0] = '\0';

    for (int i = 0; i < count; ++i) {
        const char *name = mode_to_string(modes[i]);

        /* print "<NAME>" or "<NAME> | " */
        int wrote = snprintf(out + off, out_sz - off,
                             "%s%s",
                             name,
                             (i < count - 1) ? " | " : "");

        if (wrote < 0)               break;                 // encoding error
        if ((size_t)wrote >= out_sz - off) {                // truncated
            out[out_sz - 1] = '\0';     // guarantee NUL
            break;                      // stop before overflow
        }
        off += (size_t)wrote;           // advance
    }
}

void DrawStatusBar(MetronomeState *state) {
    int ox = PAD_MARGIN_LEFT;
    int oy = PSGW*4 + PAD_MARGIN_TOP + GUTTER_WIDTH;
    int spacing = ICON_SIZE+4;

    char buf[1024];
    snprintf(buf, sizeof(buf), "bpm: %d", state->bpm);


    DrawText(buf, GetScreenWidth()-ox-MeasureText(buf, 16), oy, 16, RAYWHITE);

    build_mode_string(state->modestack, state->numActiveModes, buf, sizeof(buf));
    //assert(state->numActiveModes >= 0 &&
    //   state->numActiveModes <= 15);

    DrawText(buf, ox, oy, 16, RAYWHITE);

    //DrawPlayIcon(ox+0*spacing, oy, BLACK);
    //DrawPauseIcon(ox+1*spacing, oy, BLACK);
    //DrawStopIcon(ox+2*spacing, oy, BLACK);
    //DrawRecordIcon(ox+3*spacing, oy, MAROON);
}
