#include "ui.h"
#include <gui/canvas.h>
#include <stdio.h>

static const char* diff_str(Difficulty d) {
    switch(d) {
        case DIFF_EASY: return "Easy";
        case DIFF_MEDIUM: return "Medium";
        case DIFF_HARD: return "Hard";
    }
    return "?";
}

void ui_draw(Canvas* canvas, void* ctx) {
    UiState* s = (UiState*)ctx;
    canvas_clear(canvas);

    // Title bar
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "BadUSB Typing Race");

    // Separator
    canvas_draw_line(canvas, 0, 13, 128, 13);

    // High score top right
    char buf[32];
    snprintf(buf, sizeof(buf), "Best: %u", s->high_score);
    canvas_set_font(canvas, FontSecondary);
    int w = canvas_str_width(canvas, buf);
    canvas_draw_str(canvas, 128 - w - 2, 10, buf);

    if(!s->word_sent) {
        // Difficulty selector
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 28, "Difficulty:");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 38, diff_str(s->diff));
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 48, "< Left / Right >");

        canvas_draw_str(canvas, 2, 62, "Press OK to start");
    } else {
        // Show the word
        canvas_set_font(canvas, FontPrimary);
        snprintf(buf, sizeof(buf), "Word: %s", s->word);
        canvas_draw_str(canvas, 2, 28, buf);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 42, "Type it on host PC");
        canvas_draw_str(canvas, 2, 52, "Press UP to retry");

        if(s->wpm_valid) {
            snprintf(buf, sizeof(buf), "WPM: %u", s->wpm);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, 62, buf);
        } else {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 62, "Waiting for host...");
        }
    }

    // Bottom hint
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 118, "BACK - exit");
}