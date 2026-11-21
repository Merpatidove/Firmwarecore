// waveshare_c6_lcd_147/src/main.cpp — board glue for rsvp_core: serial
// ingest, SD book browser (menu), playback, BOOT button. Hardware facts
// live in include/board.h; stream parsing shared (firmware/rsvp_stream),
// frame rendering shared (firmware/rsvp_render.h). This file is only glue.
//
// DEVIATIONS from core/webapp reference (explicit, see also board.h):
// 1. ARDUINO_USB_CDC_ON_BOOT=1 (platformio.ini): Serial routes to the native
//    USB port; default UART0 stalls host writes after 64B.
// 2. Constructor builds PORTRAIT then setRotation(BOARD_LCD_ROT): the GFX
//    library swaps W/H itself for odd rotations — pre-swapped dims in the
//    constructor painted only part of the glass.
// 3. Banner + menu strings use ASCII hyphen: the classic GLCD font has no
//    em-dash glyph.
// 4. Chunking (1 word/chunk) is the companion's job (send_book.py), per
//    core rules #4/#5 — the firmware never splits text itself.
// 5. SD card shares the SPI bus with the LCD (own MISO=5/CS=4); SPI.begin()
//    runs with explicit pins BEFORE gfx->begin() so both libraries agree.
// 6. .rsvp files larger than BOARD_BUF_SIZE play truncated (static buffer);
//    the stream parser sets a truncated flag and playback just proceeds.
// 7. Single-button semantics: classified on RELEASE; presses between
//    SHORT_MS and LONG_MS are ignored (dead zone).
// 8. BUGFIX while factoring the parser: section titles used to point into
//    one shared control-line buffer (every title aliased the last row);
//    rsvp_stream copies them into titles[] now.
// 9. SD-read-on-device path UNVERIFIED: compiled + host-tested only, no
//    board/SD attached yet. Pending: mount, scan, menu, load, SPI arb.
// 10. SD books STREAM via rsvp_file (16KB window + skip map): book size =
//    card size, no 192KB cap. Serial path unchanged (whole-stream rs_feed).
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Arduino_GFX_Library.h>
#include "board.h"
#include "rsvp_core.h"
#include "rsvp_render.h"
#include "rsvp_stream.h"
#include "rsvp_file.h"

static Arduino_DataBus *bus = new Arduino_HWSPI(
    BOARD_PIN_DC, BOARD_PIN_CS, BOARD_PIN_SCK, BOARD_PIN_MOSI);
static Arduino_GFX *gfx = new Arduino_ST7789(
    bus, BOARD_PIN_RST, 0 /* portrait ctor — deviation 2 */, BOARD_PANEL_IPS,
    BOARD_LCD_NATIVE_W, BOARD_LCD_NATIVE_H,
    BOARD_LCD_COL_OFF, BOARD_LCD_ROW_OFF, BOARD_LCD_COL_OFF, BOARD_LCD_ROW_OFF);

static const rsvp_render_cfg RENDER_CFG = {
    BOARD_FONT, BOARD_CHAR_W, BOARD_PIVOT_X, BOARD_LCD_W, BOARD_LCD_H,
    BOARD_STATUS_RGB565};

/* book storage — caller-owned, handed to the stream parser */
static char buf[BOARD_BUF_SIZE];
static rsvp_section sections[BOARD_MAX_SECTIONS];
static char titles[BOARD_MAX_SECTIONS][RSVP_STREAM_TITLE_MAX];
static rsvp_stream stream; /* serial ingest */

/* SD streaming state (deviation 10): 16KB window + skip-map reader */
static char sd_win[BOARD_SD_WIN_SIZE];
static rsvp_section sd_sections[BOARD_MAX_SECTIONS];
static char sd_titles[BOARD_MAX_SECTIONS][RSVP_STREAM_TITLE_MAX];
static rf_reader rd;
static File book;            /* held open across playback refills */
static bool sd_book = false; /* current playback source: false = serial */

static rsvp r;
static rsvp_state_blob saved;
static uint8_t theme_idx = 0; /* night */
static bool playing = false;

/* SD book list */
static char files[BOARD_SD_FILE_MAX][BOARD_SD_NAME_MAX];
static uint16_t n_files = 0;
static bool sd_ok = false;

/* UI state machine */
enum ui_state { UI_MENU, UI_PLAY, UI_ERR_NOSD, UI_ERR_NOFILES };
static ui_state ui = UI_MENU;
static int menu_sel = 0;

/* ---- UI helpers (menu/error screens; playback rendering is shared) -- */

static void draw_centered(const char *msg, uint16_t color)
{
    gfx->fillScreen(RSVP_THEMES[theme_idx].bg.rgb565);
    gfx->setTextSize(2);
    gfx->setTextBound(0, 0, BOARD_LCD_W, BOARD_LCD_H);
    gfx->setTextColor(color);
    /* ASCII hyphen only — deviation 3 */
    gfx->setCursor(8, (BOARD_LCD_H - 16) / 2);
    gfx->print(msg);
}

static void menu_error(const char *msg)
{
    draw_centered(msg, RSVP_THEMES[theme_idx].accent.rgb565);
}

static void menu_draw()
{
    const rsvp_theme &t = RSVP_THEMES[theme_idx];
    gfx->fillScreen(t.bg.rgb565);
    gfx->setTextBound(0, 0, BOARD_LCD_W, BOARD_LCD_H);

    /* scrolled window so menu_sel stays visible (16 rows, size-1 font) */
    int first = 0;
    if (n_files > BOARD_MENU_ROWS)
        first = menu_sel - BOARD_MENU_ROWS / 2;
    if (first < 0)
        first = 0;
    if (first + BOARD_MENU_ROWS > n_files)
        first = n_files - BOARD_MENU_ROWS < 0 ? 0 : n_files - BOARD_MENU_ROWS;

    gfx->setTextSize(1);
    int16_t y = 4;
    for (int i = first; i < n_files && y < BOARD_LCD_H - 14; i++, y += 10) {
        gfx->setTextColor(i == menu_sel ? t.accent.rgb565 : t.fg.rgb565);
        gfx->setCursor(i == menu_sel ? 2 : 6, y);
        gfx->print(files[i]);
    }

    gfx->setTextSize(1);
    gfx->setCursor(4, BOARD_LCD_H - 10);
    gfx->setTextColor(BOARD_STATUS_RGB565);
    gfx->printf("%d/%d  short=next  long=open", menu_sel + 1, n_files);
}

/* ---- SD (deviation 9: UNVERIFIED on device) ------------------------- */

static bool sd_mount()
{
    /* Shared bus: SPI was begun with explicit pins pre-gfx (deviation 5).
     * SD.begin takes control of its CS only. */
    return SD.begin(BOARD_PIN_SD_CS, SPI, 25000000u);
}

static bool sd_scan()
{
    n_files = 0;
    File root = SD.open("/");
    if (!root)
        return false;
    File f = root.openNextFile();
    while (f && n_files < BOARD_SD_FILE_MAX) {
        if (!f.isDirectory()) {
            const char *name = f.name();
            size_t len = strlen(name);
            bool rsvp_ext = len > 5 && strncmp(name + len - 5, ".rsvp", 5) == 0;
            if (rsvp_ext) {
                const char *base = name;
                /* some cores return "/name" — strip the slash */
                if (base[0] == '/')
                    base++;
                snprintf(files[n_files], BOARD_SD_NAME_MAX, "%s", base);
                n_files++;
            }
        }
        f = root.openNextFile();
    }
    root.close();
    return n_files > 0;
}

/* Reset the shared parser state; also clears stale section rows so a
 * reload cannot leave titles pointing at previous book memory. */
static void stream_reset()
{
    rs_init(&stream, buf, sizeof(buf), sections, BOARD_MAX_SECTIONS, titles);
}

/* ---- SD streaming (deviation 10) ----------------------------------- */

static bool sd_read_at(void *ud, uint32_t off, char *dst, uint32_t len,
                       uint32_t *got)
{
    File *fp = (File *)ud;
    if (!*fp)
        return false;
    /* one seek per request; read may return less only at EOF */
    if (!fp->seek(off))
        return false;
    int n = fp->read((uint8_t *)dst, len);
    if (n < 0)
        return false;
    *got = (uint32_t)n;
    return true;
}

/* rf_scan walks the whole file sequentially through the real parser —
 * takes seconds on big books; the caller shows "loading..." first. */
static bool sd_open_book(const char *name)
{
    book.close();
    book = SD.open(name);
    if (!book)
        return false;
    rf_io io = {&book, sd_read_at};
    rf_init(&rd, &io, sd_win, sizeof(sd_win), sd_sections,
            BOARD_MAX_SECTIONS, sd_titles);
    if (!rf_scan(&rd)) {
        book.close();
        return false;
    }
    if (!rf_prime(&rd, 0)) {
        book.close();
        return false;
    }
    return true;
}

static void begin_playback()
{
    if (sd_book) {
        /* resume: translate saved byte_off (logical) through the window */
        rsvp_init(&r, sd_win, rd.win_len, sd_sections, rd.n_sections);
        r.off = rd.pos - rd.win_start;
    } else {
        rsvp_init(&r, buf, stream.buf_len, sections, stream.n_sections);
        rsvp_load(&r, &saved, &theme_idx);
    }
    if (sd_book ? rd.theme_req >= 0 : stream.theme_req >= 0) {
        int t = sd_book ? rd.theme_req : stream.theme_req;
        if (t >= 0 && t < RSVP_THEME_PRESETS)
            theme_idx = (uint8_t)t;
    }
    if (!sd_book && stream.truncated)
        Serial.println("[rsvp] file truncated to buffer");
    Serial.printf("[rsvp] %s len=%u sections=%u wpm=%u\n",
                  sd_book ? "sd" : "serial",
                  sd_book ? (unsigned)rd.stream_len
                          : (unsigned)stream.buf_len,
                  sd_book ? rd.n_sections : stream.n_sections,
                  rsvp_wpm(&r));
    playing = true;
    ui = UI_PLAY;
}

/* ---- button: falling edge + debounce, classify on release (dev 7) --- */

enum btn_evt { BTN_NONE, BTN_SHORT, BTN_LONG };

static btn_evt button_poll()
{
    static bool down = false;
    static uint32_t down_at = 0;
    bool now = digitalRead(BOARD_PIN_BTN) == LOW;
    if (now && !down) {
        down = true;
        down_at = millis();
    } else if (!now && down) {
        down = false;
        if (millis() - down_at < BOARD_BTN_DEBOUNCE_MS)
            return BTN_NONE; /* bounce noise */
        uint32_t held = millis() - down_at;
        if (held < BOARD_BTN_SHORT_MS)
            return BTN_SHORT;
        if (held >= BOARD_BTN_LONG_MS)
            return BTN_LONG;
        /* dead zone between thresholds: ignored */
    }
    return BTN_NONE;
}

/* ---- serial ingest: same parser as SD files ------------------------- */

static void ingest_serial()
{
    while (Serial.available())
        rs_feed(&stream, (char)Serial.read());
    if (stream.end_seen && ui != UI_PLAY)
        begin_playback();
}

void setup()
{
    Serial.begin(BOARD_SERIAL_BAUD);
    /* Shared SPI: explicit pins FIRST (deviation 5) — SCK/MOSI/CS owned by
     * this begin; SD.begin re-uses the bus with its own CS later. */
    SPI.begin(BOARD_PIN_SCK, BOARD_PIN_SD_MISO, BOARD_PIN_MOSI, -1);
    pinMode(BOARD_PIN_BTN, INPUT_PULLUP);
    gfx->begin();
    gfx->setRotation(BOARD_LCD_ROT); /* deviation 2 */
    pinMode(BOARD_PIN_BL, OUTPUT);
    digitalWrite(BOARD_PIN_BL, HIGH);
    stream_reset();

    /* boot straight into the menu; serial books still work any time */
    sd_ok = sd_mount() && sd_scan();
    if (sd_ok) {
        menu_sel = 0;
        menu_draw();
    } else {
        ui = UI_ERR_NOSD;
        menu_error("no SD card");
    }

    /* ASCII hyphen only — deviation 3 */
    gfx->setTextSize(2);
    gfx->setCursor(4, 40);
    gfx->setTextColor(RSVP_THEMES[theme_idx].fg.rgb565);
    gfx->print("RSVP - send text");
    Serial.println("[rsvp] ready (menu / SOH S/E protocol)");
}

void loop()
{
    ingest_serial();

    btn_evt ev = button_poll();

    switch (ui) {
    case UI_PLAY: {
        if (ev == BTN_SHORT) {
            playing = !playing;
            break;
        }
        if (ev == BTN_LONG) { /* back to menu: rescan, keep list fresh */
            sd_book = false;
            book.close();
            sd_ok = sd_mount() && sd_scan();
            if (sd_ok) {
                menu_sel = 0;
                menu_draw();
                ui = UI_MENU;
            } else {
                ui = UI_ERR_NOSD;
                menu_error("no SD card");
            }
            break;
        }

        static uint32_t next_at = 0;
        if (playing && (int32_t)(millis() - next_at) >= 0) {
            if (sd_book)
                rf_attach(&rd, &r); /* refill window if near its edge */
            const rsvp_view v = rsvp_frame(&r);
            if (!v.text || v.len == 0) {
                playing = false; /* end of stream: park */
                break;
            }
            rsvp_render_frame(gfx, v, RSVP_THEMES[theme_idx], RENDER_CFG);
            rsvp_render_status(gfx, RENDER_CFG, RSVP_THEMES[theme_idx],
                               rsvp_wpm(&r));
            next_at = millis() + v.ms;
            rsvp_next(&r);
            if (sd_book) {
                rf_sync(&rd, &r);  /* window offset -> logical pos */
                saved.byte_off = rd.pos;
            } else {
                rsvp_save(&r, theme_idx, &saved);
            }
            // TODO(board): debounce-write `saved` into NVS every ~5s/pause
        }
        break;
    }

    case UI_MENU:
        if (ev == BTN_SHORT && n_files > 0) {
            menu_sel = (menu_sel + 1) % n_files;
            menu_draw();
        } else if (ev == BTN_LONG && n_files > 0) {
            draw_centered("loading...", BOARD_STATUS_RGB565);
            sd_book = true;
            if (sd_open_book(files[menu_sel]))
                begin_playback();
            else {
                sd_book = false;
                menu_error("load failed");
                delay(800);
                menu_draw();
            }
        }
        break;

    case UI_ERR_NOSD:
    case UI_ERR_NOFILES:
        if (ev == BTN_SHORT || ev == BTN_LONG) {
            sd_ok = sd_mount() && sd_scan();
            if (sd_ok) {
                menu_sel = 0;
                menu_draw();
                ui = UI_MENU;
            } else {
                menu_error("no SD card"); /* redraw, stay in error */
            }
        }
        break;
    }
}
