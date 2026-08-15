#include "ui.h"
#include <gui/canvas.h>
#include <stdio.h>

void ui_draw(Canvas* canvas, void* ctx) {
    UiState* s = (UiState*)ctx;
    canvas_clear(canvas);

    // Title bar
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "BadUSB Typing Race");

    // Separator
    canvas_draw_line(canvas, 0, 13, 128, 13);

    if(!s->word_sent) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 28, "Press OK to start");
        canvas_draw_str(canvas, 2, 38, "a new word.");
    } else {
        // Show the word large
        canvas_set_font(canvas, FontBigNumbers); // if available; fallback
        // FontBigNumbers may not exist; use FontPrimary for word
        canvas_set_font(canvas, FontPrimary);
        char buf[32];
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