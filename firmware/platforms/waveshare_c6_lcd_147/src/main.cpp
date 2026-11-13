// waveshare_c6_lcd_147/src/main.cpp — board glue for rsvp_core, dedicated to
// the Waveshare ESP32-C6-LCD-1.47 NON-TOUCH board (verified pinmap: SCK=7
// MOSI=6 CS=14 DC=15 RST=21 BL=22, ST7789 IPS 172x320 offset 34, BOOT=9).
// Serial ingest: plain UTF-8 lines = chunks; SOH (0x01) control lines:
//   0x01 S <byte_offset> <tab> <title>   — section-table row (repeatable)
//   0x01 E                               — end of transmission, start reading
//   0x01 T <idx>                         — switch theme preset
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "rsvp_core.h"

#ifndef RSVP_SERIAL_BAUD
#define RSVP_SERIAL_BAUD 115200
#endif

// C6 USB CDC (Serial) vs UART bridge: this board exposes native USB-CDC.
// Hosts must NOT need DTR tricks — plain writes work once CDC is up.
static Arduino_DataBus *bus =
    new Arduino_HWSPI(15 /* DC */, 14 /* CS */, 7 /* SCK */, 6 /* MOSI */);
static Arduino_GFX *gfx = new Arduino_ST7789(
    bus, 21 /* RST */, 0 /* rot */, true /* IPS panel */,
    172, 320, 34, 0, 34, 0 /* col/row offsets — required, shifts otherwise */);

#define RSVP_PIN_BTN 9 /* BOOT button */
// C6 app DRAM ~320KB: 192KB book buffer fits, leaves headroom for CDC+GFX.
static char buf[192 * 1024];
static uint32_t buf_len = 0;
static rsvp_section sections[128];
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

static void draw_frame()
{
    const rsvp_view v = rsvp_frame(&r);
    if (!v.text || v.len == 0) {
        playing = false; /* end of stream: park */
        return;
    }
    const rsvp_theme &t = RSVP_THEMES[theme_idx];
    gfx->fillScreen(t.bg.rgb565);

    char line[128]; /* chunk copy — stream bytes are not NUL-terminated */
    uint32_t n = v.len < sizeof(line) - 1 ? v.len : sizeof(line) - 1;
    memcpy(line, v.text, n);
    line[n] = 0;

    // ASCII-only banner strings; the classic GLCD font has no em-dash glyph.
    gfx->setTextBound(0, 0, 172, 320);
    gfx->setTextSize(2);
    gfx->setCursor(4, (320 - 32) / 2);

    // 3 passes: fg, accent pivot char, fg (rule #6 ORP highlight)
    uint32_t i = 0;
    uint32_t seg = v.pivot_byte < n ? v.pivot_byte : n;
    gfx->setTextColor(t.fg.rgb565);
    for (; i < seg; i++) gfx->write(line[i]);
    gfx->setTextColor(t.accent.rgb565);
    if (i < n) gfx->write(line[i++]);
    gfx->setTextColor(t.fg.rgb565);
    for (; i < n; i++) gfx->write(line[i]);

    gfx->setTextSize(1);
    gfx->setCursor(4, 320 - 12);
    gfx->setTextColor(t.guide.rgb565);
    gfx->printf("%u wpm  %s", rsvp_wpm(&r), RSVP_THEMES[theme_idx].name);
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
        if (ctl[0] == 'S' && ctl[1] == ' ' && n_sections < 128) {
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
    Serial.begin(RSVP_SERIAL_BAUD);
    pinMode(RSVP_PIN_BTN, INPUT_PULLUP);
    gfx->begin();
    pinMode(22, OUTPUT); /* BL */
    digitalWrite(22, HIGH);
    apply_theme();

    gfx->setTextSize(2);
    gfx->setCursor(4, 40);
    gfx->setTextColor(RSVP_THEMES[theme_idx].fg.rgb565);
    gfx->print("RSVP - send text"); /* ASCII hyphen: font lacks em-dash */
    Serial.println("[rsvp] ready (SOH S/E protocol)");
}

void loop()
{
    ingest_serial();

    static uint32_t last_btn = 0;
    if (digitalRead(RSVP_PIN_BTN) == LOW && millis() - last_btn > 250) {
        last_btn = millis();
        playing = !playing;
    }

    static uint32_t next_at = 0;
    if (playing && (int32_t)(millis() - next_at) >= 0) {
        draw_frame();
        next_at = millis() + rsvp_frame(&r).ms;
        rsvp_next(&r);
        rsvp_save(&r, theme_idx, &saved);
        // TODO(board): debounce-write `saved` into NVS every ~5s or on pause
    }
}
