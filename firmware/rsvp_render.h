/* firmware/rsvp_render.h — shared board-side frame renderer. Header-only,
 * C++ (Arduino_GFX types). One source for all boards: main.cpp passes the
 * view + theme + this board's render config; no per-board draw_frame copies.
 *
 * DEVIATIONS from core/webapp reference (explicit):
 * 1. ORP parking: the pivot glyph is parked at a fixed column (cfg.pivot_x).
 *    The core only reports pivot_byte/pivot_cp; parking the eye position is
 *    a board render choice (Spritz-style). The webapp does the same in JS.
 * 2. Chunk bytes are copied to a stack line[] and clamped to 63 chars —
 *    one-word chunks (companion, MAX_WORDS=1) always fit.
 * 3. Status bar convention: "wpm  theme" bottom-left, size 1, FIXED medium
 *    grey (cfg.status_rgb565) — the night/ocean/forest theme guide colors
 *    are near-invisible on their own dark bg; core palettes stay untouched. */
#ifndef RSVP_RENDER_H
#define RSVP_RENDER_H

#include <Arduino_GFX_Library.h>
#include "rsvp_core.h"

typedef struct {
    uint16_t font;   /* GLCD textsize multiplier; 1 char = 6x8 px * font */
    uint16_t char_w; /* = 6 * font */
    uint16_t pivot_x; /* ORP column (px) the pivot glyph is parked at */
    uint16_t w, h;   /* logical (post-rotation) screen dims */
    uint16_t status_rgb565; /* status-bar text color (see deviation 3) */
} rsvp_render_cfg;

/* Draw one chunk centered vertically, pivot parked at cfg.pivot_x. */
static inline void rsvp_render_frame(Arduino_GFX *gfx, const rsvp_view &v,
                                     const rsvp_theme &t,
                                     const rsvp_render_cfg &cfg)
{
    gfx->fillScreen(t.bg.rgb565);
    if (!v.text || v.len == 0)
        return;

    char line[64]; /* stream bytes are not NUL-terminated */
    uint32_t n = v.len < sizeof(line) - 1 ? v.len : sizeof(line) - 1;
    memcpy(line, v.text, n);
    line[n] = 0;

    gfx->setTextBound(0, 0, cfg.w, cfg.h);
    gfx->setTextSize(cfg.font);

    /* ORP parking (deviation 1): shift left so the pivot lands on pivot_x */
    int16_t x = (int16_t)cfg.pivot_x - (int16_t)(v.pivot_byte * cfg.char_w);
    if (x < 0)
        x = 0;
    int16_t y = ((int16_t)cfg.h - 8 * (int16_t)cfg.font) / 2;
    gfx->setCursor(x, y);

    /* 3 passes: fg, accent pivot glyph, fg */
    uint32_t i = 0;
    uint32_t seg = v.pivot_byte < n ? v.pivot_byte : n;
    gfx->setTextColor(t.fg.rgb565);
    for (; i < seg; i++) gfx->write(line[i]);
    gfx->setTextColor(t.accent.rgb565);
    if (i < n) gfx->write(line[i++]);
    gfx->setTextColor(t.fg.rgb565);
    for (; i < n; i++) gfx->write(line[i]);
}

/* Status line: "<wpm> wpm  <theme>" bottom-left, size 1, fixed grey. */
static inline void rsvp_render_status(Arduino_GFX *gfx,
                                      const rsvp_render_cfg &cfg,
                                      const rsvp_theme &t, uint16_t wpm)
{
    gfx->setTextSize(1);
    gfx->setCursor(4, cfg.h - 10);
    gfx->setTextColor(cfg.status_rgb565);
    gfx->printf("%u wpm  %s", (unsigned)wpm, t.name);
}

#endif /* RSVP_RENDER_H */
