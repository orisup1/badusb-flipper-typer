#pragma once

#include <gui/canvas.h>

typedef enum { DIFF_EASY=0, DIFF_MEDIUM=1, DIFF_HARD=2 } Difficulty;

typedef struct {
    const char* word;
    bool word_sent;
    uint16_t wpm;
    bool wpm_valid;
    Difficulty diff;
    uint16_t high_score;
} UiState;

void ui_draw(Canvas* canvas, void* ctx);