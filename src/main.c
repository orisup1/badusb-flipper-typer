#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <usb/hid/usb_hid.h>
#include <usb/cdc/usb_cdc.h>
#include <storage/storage.h>
#include <furi_hal_speaker.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include "ui.h"

static const char* WORDS_EASY[] = {
    "flipper", "zero", "badusb", "key", "type", "race", "speed", "test"
};
static const char* WORDS_MEDIUM[] = {
    "keyboard", "typing", "challenge", "practice", "hardware", "firmware", "usbhid", "keystroke"
};
static const char* WORDS_HARD[] = {
    "implementation", "optimization", "development", "configuration", "microcontroller", "programming", "communication", "integration"
};

typedef enum { DIFF_EASY=0, DIFF_MEDIUM=1, DIFF_HARD=2 } Difficulty;

static const char** WORDS_LISTS[] = { WORDS_EASY, WORDS_MEDIUM, WORDS_HARD };
static const uint32_t WORDS_COUNTS[] = {
    sizeof(WORDS_EASY)/sizeof(WORDS_EASY[0]),
    sizeof(WORDS_MEDIUM)/sizeof(WORDS_MEDIUM[0]),
    sizeof(WORDS_HARD)/sizeof(WORDS_HARD[0])
};

static const char* HIGH_SCORE_FILE = "badusb_typing_highscore";

static uint16_t load_high_score(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    uint16_t hs = 0;
    if(storage_file_open(file, HIGH_SCORE_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buf[2];
        if(storage_file_read(file, buf, 2) == 2) {
            hs = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return hs;
}

static void save_high_score(uint16_t hs) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, HIGH_SCORE_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint8_t buf[2] = { (uint8_t)(hs & 0xFF), (uint8_t)(hs >> 8) };
        storage_file_write(file, buf, 2);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void beep_ok(void) { furi_hal_speaker_play_tone(2000, 80); furi_delay_ms(100); furi_hal_speaker_stop(); }
static void beep_done(void) { furi_hal_speaker_play_tone(1500, 120); furi_delay_ms(150); furi_hal_speaker_stop(); }

typedef struct {
    const char* current_word;
    uint32_t start_tick;
    bool word_sent;
    uint16_t wpm;            // received from host
    bool wpm_valid;
    Difficulty diff;
    uint16_t high_score;
} AppState;

/* ---- USB HID keyboard helper ---- */
static void hid_send_key(uint8_t mod, uint8_t key) {
    uint8_t report[8] = {mod, 0, key, 0,0,0,0,0};
    usb_hid_keyboard_send_report(report);
}
static void hid_release_all(void) {
    uint8_t report[8] = {0};
    usb_hid_keyboard_send_report(report);
}
static void send_word_usb_hid(const char* word) {
    for(size_t i = 0; i < strlen(word); ++i) {
        char c = word[i];
        uint8_t mod = 0, key = 0;
        if(c >= 'a' && c <= 'z') {
            key = 4 + (c - 'a');               // HID usage IDs 0x04‑0x1D
        } else if(c >= 'A' && c <= 'Z') {
            mod = 0x02;                         // Left Shift
            key = 4 + (c - 'A');
        } else if(c >= '1' && c <= '9') {
            key = 0x1E + (c - '1');             // 1‑9
        } else if(c == '0') {
            key = 0x27;                         // 0
        } else if(c == ' ') {
            key = 0x2C;                         // Space
        } else if(c == '-') {
            key = 0x2D;
        } else if(c == '=') {
            key = 0x2E;
        } else if(c == '[') {
            key = 0x2F;
        } else if(c == ']') {
            key = 0x30;
        } else if(c == '\\') {
            key = 0x31;
        } else if(c == ';') {
            key = 0x33;
        } else if(c == '\'') {
            key = 0x34;
        } else if(c == '`') {
            key = 0x35;
        } else if(c == ',') {
            key = 0x36;
        } else if(c == '.') {
            key = 0x37;
        } else if(c == '/') {
            key = 0x38;
        }
        if(key) {
            hid_send_key(mod, key);
            furi_delay_ms(4);
            hid_release_all();
            furi_delay_ms(4);
        }
    }
}

/* ---- CDC serial helper ---- */
static void cdc_send_line(const char* line) {
    usb_cdc_send((uint8_t*)line, strlen(line), FuriWaitForever);
    usb_cdc_send((uint8_t*)"\n", 1, FuriWaitForever);
}
static bool cdc_read_line(char* buf, size_t len, uint32_t timeout_ms) {
    size_t pos = 0;
    uint32_t start = furi_get_tick();
    while(furi_get_tick() - start < timeout_ms) {
        uint8_t ch;
        if(usb_cdc_receive(&ch, 1, 10) == FuriStatusOk) {
            if(ch == '\n' || ch == '\r') {
                if(pos) { buf[pos] = 0; return true; }
            } else if(pos < len - 1) {
                buf[pos++] = (char)ch;
            }
        }
    }
    return false;
}

/* ---- Input callback ---- */
static void app_callback(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

/* ---- Main entry ---- */
int32_t badusb_typing_race(void* p) {
    UNUSED(p);
    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();
    UiState ui_state = {0};
    view_port_draw_callback_set(view_port, ui_draw, &ui_state);
    view_port_input_callback_set(view_port, app_callback, queue);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    usb_hid_keyboard_init();
    usb_cdc_init();

    AppState state = {0};
    state.diff = DIFF_EASY;
    state.high_score = load_high_score();
    UiState ui_state = {0};
    srand((unsigned)time(NULL));

    InputEvent event;
    char cdc_buf[64];

    while(1) {
        /* ----- handle host -> Flipper CDC (WPM) ----- */
        if(cdc_read_line(cdc_buf, sizeof(cdc_buf), 5)) {
            // expect "WPM:<number>"
            if(strncmp(cdc_buf, "WPM:", 4) == 0) {
                state.wpm = (uint16_t)atoi(cdc_buf + 4);
                state.wpm_valid = true;
                if(state.wpm > state.high_score) {
                    state.high_score = state.wpm;
                    save_high_score(state.high_score);
                }
                beep_done();
            }
        }

        /* ----- handle UI events ----- */
        if(furi_message_queue_get(queue, &event, 50) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) break;

            if(event.type == InputTypeShort && event.key == InputKeyOk && !state.word_sent) {
                const char** list = WORDS_LISTS[state.diff];
                uint32_t count = WORDS_COUNTS[state.diff];
                state.current_word = list[rand() % count];
                state.start_tick = furi_get_tick();
                state.word_sent = true;
                state.wpm_valid = false;

                // send word over HID
                send_word_usb_hid(state.current_word);
                // notify host script of the target word
                cdc_send_line(state.current_word);
                beep_ok();
            }

            if(event.type == InputTypeShort && event.key == InputKeyUp) {
                // allow retry
                state.word_sent = false;
            }

            if(!state.word_sent) {
                if(event.type == InputTypeShort && event.key == InputKeyLeft) {
                    if(state.diff > DIFF_EASY) state.diff--;
                }
                if(event.type == InputTypeShort && event.key == InputKeyRight) {
                    if(state.diff < DIFF_HARD) state.diff++;
                }
            }
        }

        // sync UI state
        ui_state.word = state.current_word;
        ui_state.word_sent = state.word_sent;
        ui_state.wpm = state.wpm;
        ui_state.wpm_valid = state.wpm_valid;
        ui_state.diff = state.diff;
        ui_state.high_score = state.high_score;

        view_port_update(view_port);
        furi_delay_ms(20);
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(queue);
    furi_record_close(RECORD_GUI);
    return 0;
}