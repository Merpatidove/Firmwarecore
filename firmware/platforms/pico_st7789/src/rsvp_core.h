/* firmware/rsvp_core.h — streaming RSVP playback core for MCUs. C99, no allocation.
 * Firmware core, NOT the webapp: shares nothing with index.html. Faithful port
 * of rsvp-core.js's playback math, block mode only. Rule #s cite ../RESEARCH.md. */
#ifndef RSVP_CORE_H
#define RSVP_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h> /* freestanding C99 header, same class as stdint/stddef: bool lives here */

typedef struct rsvp_s rsvp; /* opaque */

/* rule #15: table is precomputed by the companion (JS sections() output). */
typedef struct {
    uint32_t byte_offset; /* byte offset of the section's first chunk */
    const char *title;
} rsvp_section;

/* test_core.c stacks `rsvp r;` on the C stack: it needs the real struct.
 * Boards keep the default opaque typedef; only the test defines this. */
#ifdef RSVP_EXPOSE_STATE
struct rsvp_s {
    const char *stream;
    uint32_t len;
    uint32_t off; /* byte offset of current chunk start; off == len: parked at end */
    uint16_t wpm;
    const rsvp_section *sections;
    uint16_t n_sections;
};
#endif

/* What a board renders per frame. pivot_byte/pivot_cp locate the ORP character
 * (rule #6); words is the chunk's word count; ms the hold time (rule #7). */
typedef struct {
    const char *text;
    uint32_t len;
    uint16_t pivot_byte;
    uint16_t pivot_cp;
    uint16_t words;
    uint32_t ms;
} rsvp_view;

/* feature 5: colors — hexes from index.html #wchips + app defaults. */
typedef struct {
    uint8_t r, g, b;
    uint16_t rgb565;
} rsvp_color;

/* feature 5, extended: named theme = the 4 colors a board paints each frame.
 * Preset table is fixed (flash); boards pick one index at a time. */
typedef struct {
    const char *name;
    rsvp_color bg, fg, accent, guide;
} rsvp_theme;

#define RSVP_THEME(bg, fg, ac, gu) \
    {#bg "/" #fg, {bg}, {fg}, {ac}, {gu}}

#define RSVP_RGB888565(r, g, b) \
    ((uint16_t)((((r)&0xF8) << 8) | (((g)&0xFC) << 3) | (((b)&0xF8) >> 3)))

extern const rsvp_color RSVP_ACCENTS[5]; /* amber white green red blue */
extern const rsvp_color RSVP_BG_DEFAULT;    /* #08090c */
extern const rsvp_color RSVP_FG_DEFAULT;    /* #e9e4dc */
extern const rsvp_color RSVP_GUIDE_DEFAULT; /* #2a2e36 */

/* feature 5: named theme presets (values from the rsvpnano app palette). */
#define RSVP_THEME_PRESETS 4
extern const rsvp_theme RSVP_THEMES[RSVP_THEME_PRESETS];

/* The stream is UTF-8, chunks separated by '\n' bytes (companion owns
 * chunking/sanitizing, rules #4/#5/#14). No copy is made: it must outlive r. */
void rsvp_init(rsvp *r, const char *stream, uint32_t len,
               const rsvp_section *sections, uint16_t n_sections);

bool rsvp_next(rsvp *r); /* advance; false at end (clamps, like the JS seek) */
bool rsvp_prev(rsvp *r); /* regression (rule #8); false at start */

void rsvp_seek_chunk(rsvp *r, uint32_t byte_offset); /* snaps to chunk start */

/* feature 1: clamp >=60 (rule #1), treat 0 as the 300 default. */
void rsvp_set_wpm(rsvp *r, uint16_t wpm);
uint16_t rsvp_wpm(const rsvp *r);

/* Current chunk as a view; empty stream -> text=NULL, len=0. */
rsvp_view rsvp_frame(const rsvp *r);

/* rule #15: section navigation (feature 2). */
uint16_t rsvp_section_count(const rsvp *r);
/* Returns byte_offset, 0xFFFFFFFF on bad index; title via out_title (may be NULL). */
uint32_t rsvp_section_at(const rsvp *r, uint16_t i, const char **out_title);

/* Persistence: serialize/load the small user state (wpm + position + theme).
 * The core never touches storage; the board owns where the blob goes (NVS,
 * EEPROM, file). Blob is endian-safe-by-value little-endian, version-tagged. */
#define RSVP_STATE_MAGIC 0x52535031u /* "RSP1" */
typedef struct {
    uint32_t magic;   /* RSVP_STATE_MAGIC */
    uint16_t version; /* 1 */
    uint16_t wpm;
    uint32_t byte_off; /* chunk-aligned stream offset */
    uint8_t theme_idx;
} rsvp_state_blob; /* 12 bytes, no padding (all naturally aligned, packed order) */

/* Fill blob from r; returns 0. out_blob may not be NULL. */
void rsvp_save(const rsvp *r, uint8_t theme_idx, rsvp_state_blob *out_blob);
/* Apply blob to r: wpm + theme + seek to saved offset (clamped like seek_chunk).
 * Returns false if magic/version mismatch — board then uses defaults. */
bool rsvp_load(rsvp *r, const rsvp_state_blob *blob, uint8_t *out_theme_idx);

#ifdef __cplusplus
}
#endif

#endif /* RSVP_CORE_H */
