// esp32_generic/src/main.cpp — board glue for rsvp_core on classic ESP32
// DevKit + external ST7789 SPI panel. Wiring is whatever you choose; the
// defaults below are the common VSPI hookup (override via build_flags).
// Serial ingest: plain UTF-8 lines = chunks; SOH (0x01) control lines:
//   0x01 S <byte_offset> <tab> <title>   — section-table row (repeatable)
//   0x01 E                               — end of transmission, start reading
//   0x01 T <idx>                         — switch theme preset
#ifndef RSVP_PIN_SCK
#define RSVP_PIN_SCK 18
#endif
#ifndef RSVP_PIN_MOSI
#define RSVP_PIN_MOSI 23
#endif
#ifndef RSVP_PIN_CS
#define RSVP_PIN_CS 5
#endif
#ifndef RSVP_PIN_DC
#define RSVP_PIN_DC 2
#endif
#ifndef RSVP_PIN_RST
#define RSVP_PIN_RST 4
#endif
#ifndef RSVP_PIN_BL
#define RSVP_PIN_BL 15
#endif
#ifndef RSVP_PIN_BTN
#define RSVP_PIN_BTN 0 /* BOOT */
#endif
#ifndef RSVP_SERIAL_BAUD
#define RSVP_SERIAL_BAUD 115200
#endif
// classic ESP32 DRAM is tight (~160KB free after Arduino core): default 96KB
#ifndef RSVP_BUF_SIZE
#define RSVP_BUF_SIZE (96 * 1024)
#endif
// most cheap ST7789 breakout panels are IPS; set RSVP_PANEL_TN for TN panels
#ifndef RSVP_PANEL_IPS
#define RSVP_PANEL_IPS true
#endif

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "rsvp_core.h"

static Arduino_DataBus *bus =
    new Arduino_HWSPI(RSVP_PIN_DC, RSVP_PIN_CS, RSVP_PIN_SCK, RSVP_PIN_MOSI);
static Arduino_GFX *gfx = new Arduino_ST7789(
    bus, RSVP_PIN_RST, 0 /* rot */, RSVP_PANEL_IPS,
    240, 320, 0, 0, 0, 0);

static char buf[RSVP_BUF_SIZE];
static uint32_t buf_len = 0;
static rsvp_section sections[64];
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

    gfx->setTextBound(0, 0, 240, 320);
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
        if (ctl[0] == 'S' && ctl[1] == ' ' && n_sections < 64) {
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
    pinMode(RSVP_PIN_BL, OUTPUT);
    digitalWrite(RSVP_PIN_BL, HIGH);
    apply_theme();

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
