// pico_st7789/src/main.cpp — board glue for rsvp_core on Raspberry Pi Pico
// (RP2040) / Pico 2 (RP2350) + external ST7789. Hardware facts live in
// include/board.h; frame rendering is shared (firmware/rsvp_render.h).
// This file is only glue.
//
// DEVIATIONS from core/webapp reference (explicit, see also board.h):
// 1. arduino-pico: Arduino_HWSPI takes a SPIClass* + pin config (SPI.setSCK
//    etc. before begin), not raw SCK/MOSI ctor args like ESP32.
// 2. Banner uses ASCII hyphen: the classic GLCD font has no em-dash glyph.
// 3. Chunking (1 word/chunk) is the companion's job (send_book.py), per
//    core rules #4/#5 — the firmware never splits text itself.
#include <Arduino.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include "board.h"
#include "rsvp_core.h"
#include "rsvp_render.h"

static Arduino_DataBus *bus_factory()
{
    SPI.setSCK(BOARD_PIN_SCK);
    SPI.setTX(BOARD_PIN_MOSI);
    SPI.setCS(BOARD_PIN_CS);
    SPI.begin();
    return new Arduino_HWSPI(BOARD_PIN_DC, BOARD_PIN_CS);
}

static Arduino_DataBus *bus = bus_factory();
static Arduino_GFX *gfx = new Arduino_ST7789(
    bus, BOARD_PIN_RST, 0 /* portrait ctor */, BOARD_PANEL_IPS,
    BOARD_LCD_NATIVE_W, BOARD_LCD_NATIVE_H,
    BOARD_LCD_COL_OFF, BOARD_LCD_ROW_OFF, BOARD_LCD_COL_OFF, BOARD_LCD_ROW_OFF);

static const rsvp_render_cfg RENDER_CFG = {
    BOARD_FONT, BOARD_CHAR_W, BOARD_PIVOT_X, BOARD_LCD_W, BOARD_LCD_H,
    BOARD_STATUS_RGB565};

static char buf[BOARD_BUF_SIZE];
static uint32_t buf_len = 0;
static rsvp_section sections[BOARD_MAX_SECTIONS];
static uint16_t n_sections = 0;
static rsvp r;
static rsvp_state_blob saved;
static uint8_t theme_idx = 0; /* night */
static bool playing = false;

static void apply_theme()
{
    const rsvp_theme &t = RSVP_THEMES[theme_idx];
    gfx->fillScreen(t.bg.rgb565);
}

static void ingest_serial()
{
    while (Serial.available()) {
        static char ctl[96];
        int c = Serial.read();
        if (c != 0x01) { // plain chunk byte
            if (buf_len + 1 < sizeof(buf))
                buf[buf_len++] = (char)c;
            continue;
        }
        size_t l = 0;
        while (Serial.available() && l < sizeof(ctl) - 1) {
            int ch = Serial.read();
            if (ch == '\n')
                break;
            ctl[l++] = (char)ch;
        }
        ctl[l] = 0;
        if (ctl[0] == 'S' && ctl[1] == ' ' && n_sections < BOARD_MAX_SECTIONS) {
            char *tab = strchr(ctl + 2, '\t');
            if (tab) {
                *tab = 0;
                sections[n_sections].byte_offset = (uint32_t)atol(ctl + 2);
                sections[n_sections].title = tab + 1;
                n_sections++;
            }
        } else if (ctl[0] == 'E') {
            rsvp_init(&r, buf, buf_len, sections, n_sections);
            rsvp_load(&r, &saved, &theme_idx);
            apply_theme();
            playing = true;
            Serial.printf("[rsvp] len=%u sections=%u wpm=%u\n",
                          (unsigned)buf_len, n_sections, rsvp_wpm(&r));
        } else if (ctl[0] == 'T') {
            int idx = atoi(ctl + 1);
            if (idx >= 0 && idx < RSVP_THEME_PRESETS) {
                theme_idx = (uint8_t)idx;
                apply_theme();
            }
        }
    }
}

void setup()
{
    Serial.begin(BOARD_SERIAL_BAUD);
    pinMode(BOARD_PIN_BTN, INPUT_PULLUP);
    gfx->begin();
    gfx->setRotation(BOARD_LCD_ROT);
    pinMode(BOARD_PIN_BL, OUTPUT);
    digitalWrite(BOARD_PIN_BL, HIGH);
    apply_theme();

    /* ASCII hyphen only — deviation 2 */
    gfx->setTextSize(2);
    gfx->setCursor(4, 40);
    gfx->setTextColor(RSVP_THEMES[theme_idx].fg.rgb565);
    gfx->print("RSVP - send text");
    Serial.println("[rsvp] ready (SOH S/E protocol)");
}

void loop()
{
    ingest_serial();

    static uint32_t last_btn = 0;
    if (digitalRead(BOARD_PIN_BTN) == LOW && millis() - last_btn > 250) {
        last_btn = millis();
        playing = !playing;
    }

    static uint32_t next_at = 0;
    if (playing && (int32_t)(millis() - next_at) >= 0) {
        const rsvp_view v = rsvp_frame(&r);
        if (!v.text || v.len == 0) {
            playing = false; /* end of stream: park */
            return;
        }
        rsvp_render_frame(gfx, v, RSVP_THEMES[theme_idx], RENDER_CFG);
        rsvp_render_status(gfx, RENDER_CFG, RSVP_THEMES[theme_idx],
                           rsvp_wpm(&r));
        next_at = millis() + v.ms;
        rsvp_next(&r);
        rsvp_save(&r, theme_idx, &saved);
        // TODO(board): debounce-write `saved` into EEPROM every ~5s or on pause
    }
}
