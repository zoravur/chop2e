#include "chop2e.h"

const char *leftchars = "1234qwerasdfzxcv";
const int leftkeys[] = {
    KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR,
    KEY_Q, KEY_W, KEY_E, KEY_R,
    KEY_A, KEY_S, KEY_D, KEY_F,
    KEY_Z, KEY_X, KEY_C, KEY_V
};
const char *leftlabels[] = {
    "", // 8
    "", // 9
    "", // 0
    "", // -
    "", // u
    "", // i
    "", // o
    "", // p
    "", // j
    "", // k
    "", // l
    "", // s
    "", // n
    "", // m
    "", // ,
    "" // .
};

const char *rightchars = "890-uiopjkl;nm,.";
const int rightkeys[] = {
    KEY_EIGHT, KEY_NINE, KEY_ZERO, KEY_MINUS,
    KEY_U, KEY_I, KEY_O, KEY_P,
    KEY_J, KEY_K, KEY_L,
    KEY_SEMICOLON, KEY_N,
    KEY_M, KEY_COMMA, KEY_PERIOD
};
const char *rightlabels[] = {
    "", // 8
    "", // 9
    "", // 0
    "", // -
    "DOWN", // u
    "UP", // i
    "LOAD", // o
    "SAVE", // p
    "WRITE", // j
    "PAUSE / PLAY", // k
    "PATTERN", // l
    "BPM", // s
    "INSTRUMENT", // n
    "", // m
    "", // ,
    "" // .

};
