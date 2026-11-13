// esp32_st7789/src/main.cpp — board glue for rsvp_core: serial ingest +
// playback + BOOT button. Wire format (firmware/README.md): UTF-8 chunks
// '\n'-delimited; control lines start with SOH (0x01):
//   0x01 S <byte_offset> <tab> <title>   — one section-table row (repeatable)
//   0x01 E                                — end of transmission, start reading
// Plain text that isn't a control line = chunk bytes (pushed verbatim into buf).
#include <Arduino.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include "rsvp_core.h"

#ifndef RSVP_SERIAL_BAUD
#define RSVP_SERIAL_BAUD 115200
#endif

#ifdef ARDUINO_ARCH_RP2040
// arduino-pico: Arduino_HWSPI takes a SPIClass* + pin config, not raw SCK/MOSI
static Arduino_DataBus *bus_factory()
{
    SPI.setSCK(RSVP_PIN_SCK);
    SPI.setTX(RSVP_PIN_MOSI);
    SPI.setCS(RSVP_PIN_CS);
    SPI.begin();
    return new Arduino_HWSPI(RSVP_PIN_DC, RSVP_PIN_CS);
}
#else
static Arduino_DataBus *bus_factory()
{
    return new Arduino_HWSPI(RSVP_PIN_DC, RSVP_PIN_CS, RSVP_PIN_SCK, RSVP_PIN_MOSI);
}
#endif

static Arduino_DataBus *bus = bus_factory();
static Arduino_GFX *gfx = new Arduino_ST7789(
    bus, RSVP_PIN_RST, 0 /* rot */, false /* IPS */,
    RSVP_LCD_W, RSVP_LCD_H, RSVP_LCD_OFF_X, RSVP_LCD_OFF_Y, RSVP_LCD_OFF_X,
    RSVP_LCD_OFF_Y);

// book buffer: static, no heap. ~192KB fits C6 heap easily.
static char buf[192 * 1024];
static uint32_t buf_len = 0;
static rsvp_section sections[128];
static uint16_t n_sections = 0;
static rsvp r;
static rsvp_state_blob saved;
static uint8_t theme_idx = 0;
static bool playing = false;

#define THEME_COUNT RSVP_THEME_PRESETS

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

    // copy chunk out (not NUL-terminated in the stream) and clamp to screen
    char line[128];
    uint32_t n = v.len < sizeof(line) - 1 ? v.len : sizeof(line) - 1;
    memcpy(line, v.text, n);
    line[n] = 0;

    gfx->setTextBound(0, 0, RSVP_LCD_W, RSVP_LCD_H);
    gfx->setTextSize(2);
    int16_t x = 4, y = (RSVP_LCD_H - 16 * 2) / 2;
    gfx->setCursor(x, y);

    const uint16_t cp_before = v.pivot_cp;      // chars left of pivot
    const uint16_t cp_at = 1;                    // the pivot char itself
    // print in 3 passes so the pivot glyph gets the accent color
    uint16_t cp = 0;
    uint32_t i = 0;
    // pass 1: [0, pivot_byte)
    uint32_t seg = v.pivot_byte < n ? v.pivot_byte : n;
    gfx->setTextColor(t.fg.rgb565);
    for (; i < seg; i++) { gfx->write(line[i]); }
    // pass 2: pivot char (skip spaces — core guarantees non-space, but clamp)
    gfx->setTextColor(t.accent.rgb565);
    if (i < n) { gfx->write(line[i]); i++; }
    // pass 3: rest
    gfx->setTextColor(t.fg.rgb565);
    for (; i < n; i++) { gfx->write(line[i]); }
    (void)cp; (void)cp_before; (void)cp_at;

    // status line: wpm + theme
    gfx->setTextSize(1);
    gfx->setCursor(4, RSVP_LCD_H - 12);
    gfx->setTextColor(t.guide.rgb565);
    gfx->printf("%u wpm  %s  ch%u/%u", rsvp_wpm(&r), RSVP_THEMES[theme_idx].name,
                n_sections ? 1 : 0, n_sections);
}

// --- serial ingest: SOH control lines + raw chunk lines -----------------
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
        // read one control line
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
            rsvp_load(&r, &saved, &theme_idx); // resume if blob valid
            apply_theme();
            playing = true;
            Serial.printf("[rsvp] len=%u sections=%u wpm=%u\n",
                          (unsigned)buf_len, n_sections, rsvp_wpm(&r));
        } else if (ctl[0] == 'T') { // T <idx> — set theme
            int idx = atoi(ctl + 1);
            if (idx >= 0 && idx < THEME_COUNT) {
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
    pinMode(RSVP_PIN_BL, OUTPUT);
    digitalWrite(RSVP_PIN_BL, HIGH);
    apply_theme();

    gfx->setTextSize(2);
    gfx->setCursor(4, 40);
    gfx->setTextColor(RSVP_THEMES[theme_idx].fg.rgb565);
    gfx->print("RSVP — send text");
    Serial.println("[rsvp] ready (SOH S/E protocol, plain lines = chunks)");
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
        // persist on every chunk: cheap 12-byte struct, board writes NVS lazily
        rsvp_save(&r, theme_idx, &saved);
        // TODO(board): debounce-write `saved` into NVS every ~5s or on pause
    }
}
