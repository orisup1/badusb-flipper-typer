#pragma once

#include <gui/canvas.h>

typedef struct {
    const char* word;
    bool word_sent;
    uint16_t wpm;
    bool wpm_valid;
} UiState;

void ui_draw(Canvas* canvas, void* ctx);