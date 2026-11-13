/* firmware/rsvp_core.c — streaming RSVP core, ported from rsvp-core.js. C99, no heap.
 * The companion app sends pre-chunked '\n'-delimited chunks (rules #4/#5/#14:
 * it owns sanitize/chunk/sections). This core only walks and renders them. */
#ifdef __cplusplus
extern "C" {
#endif
#include "rsvp_core.h"
#ifdef __cplusplus
}
#endif

/* rule #10: ORP is a tunable heuristic — tune this one table, nothing else. */
static const uint16_t ORP_TABLE[5] = {0, 1, 2, 3, 4};
#define RSVP_WPM_DEFAULT 300u
#define RSVP_WPM_MIN 60u
#define RSVP_SENTINEL 0xFFFFFFFFu

#ifdef RSVP_EXPOSE_STATE
/* test builds: struct definition comes from the header. */
#else
struct rsvp_s {
    const char *stream;
    uint32_t len;
    uint32_t off; /* byte offset of current chunk start; off == len: parked at end */
    uint16_t wpm;
    const rsvp_section *sections;
    uint16_t n_sections;
};
#endif

/* feature 5: exact hexes from index.html #wchips + app defaults. */
const rsvp_color RSVP_ACCENTS[5] = {
    {0xff, 0xb3, 0x47, RSVP_RGB888565(0xff, 0xb3, 0x47)}, /* amber */
    {0xff, 0xff, 0xff, RSVP_RGB888565(0xff, 0xff, 0xff)}, /* white */
    {0x7e, 0xe7, 0x87, RSVP_RGB888565(0x7e, 0xe7, 0x87)}, /* green */
    {0xff, 0x7a, 0x6e, RSVP_RGB888565(0xff, 0x7a, 0x6e)}, /* red */
    {0x79, 0xc0, 0xff, RSVP_RGB888565(0x79, 0xc0, 0xff)}, /* blue */
};
const rsvp_color RSVP_BG_DEFAULT = {0x08, 0x09, 0x0c, RSVP_RGB888565(0x08, 0x09, 0x0c)};
const rsvp_color RSVP_FG_DEFAULT = {0xe9, 0xe4, 0xdc, RSVP_RGB888565(0xe9, 0xe4, 0xdc)};
const rsvp_color RSVP_GUIDE_DEFAULT = {0x2a, 0x2e, 0x36, RSVP_RGB888565(0x2a, 0x2e, 0x36)};

/* feature 5: named presets — dark reads best on these panels; solar = e-ink
 * style light theme for daylight. accent pairs with each bg. */
const rsvp_theme RSVP_THEMES[RSVP_THEME_PRESETS] = {
    {"night",  {0x08, 0x09, 0x0c, RSVP_RGB888565(0x08, 0x09, 0x0c)},
               {0xe9, 0xe4, 0xdc, RSVP_RGB888565(0xe9, 0xe4, 0xdc)},
               {0xff, 0xb3, 0x47, RSVP_RGB888565(0xff, 0xb3, 0x47)},
               {0x2a, 0x2e, 0x36, RSVP_RGB888565(0x2a, 0x2e, 0x36)}},
    {"paper",  {0xf4, 0xf1, 0xe8, RSVP_RGB888565(0xf4, 0xf1, 0xe8)},
               {0x1a, 0x1a, 0x1a, RSVP_RGB888565(0x1a, 0x1a, 0x1a)},
               {0xc7, 0x50, 0x0c, RSVP_RGB888565(0xc7, 0x50, 0x0c)},
               {0xd8, 0xd4, 0xc8, RSVP_RGB888565(0xd8, 0xd4, 0xc8)}},
    {"ocean",  {0x0a, 0x14, 0x1e, RSVP_RGB888565(0x0a, 0x14, 0x1e)},
               {0xd7, 0xe3, 0xf4, RSVP_RGB888565(0xd7, 0xe3, 0xf4)},
               {0x79, 0xc0, 0xff, RSVP_RGB888565(0x79, 0xc0, 0xff)},
               {0x1c, 0x2e, 0x44, RSVP_RGB888565(0x1c, 0x2e, 0x44)}},
    {"forest", {0x0c, 0x12, 0x0d, RSVP_RGB888565(0x0c, 0x12, 0x0d)},
               {0xdc, 0xe8, 0xdc, RSVP_RGB888565(0xdc, 0xe8, 0xdc)},
               {0x7e, 0xe7, 0x87, RSVP_RGB888565(0x7e, 0xe7, 0x87)},
               {0x1e, 0x30, 0x22, RSVP_RGB888565(0x1e, 0x30, 0x22)}},
};

void rsvp_init(rsvp *r, const char *stream, uint32_t len,
               const rsvp_section *sections, uint16_t n_sections)
{
    r->stream = stream;
    r->len = len;
    r->off = 0;
    r->sections = sections;
    r->n_sections = n_sections;
    rsvp_set_wpm(r, RSVP_WPM_DEFAULT);
}

void rsvp_set_wpm(rsvp *r, uint16_t wpm) /* feature 1 */
{
    if (wpm == 0)
        wpm = RSVP_WPM_DEFAULT;
    if (wpm < RSVP_WPM_MIN) /* rule #1: stay in the normal reading range */
        wpm = RSVP_WPM_MIN;
    r->wpm = wpm;
}

uint16_t rsvp_wpm(const rsvp *r) { return r->wpm; }

uint16_t rsvp_section_count(const rsvp *r) { return r->n_sections; }

uint32_t rsvp_section_at(const rsvp *r, uint16_t i, const char **out_title)
{
    if (i >= r->n_sections)
        return RSVP_SENTINEL;
    if (out_title)
        *out_title = r->sections[i].title;
    return r->sections[i].byte_offset;
}

/* Chunk boundaries around byte offset p: scan out to the enclosing '\n's. */
static void chunk_at(const rsvp *r, uint32_t p, uint32_t *start, uint32_t *end)
{
    uint32_t s = p, e = p;
    while (s > 0 && r->stream[s - 1] != '\n')
        s--;
    while (e < r->len && r->stream[e] != '\n')
        e++;
    *start = s;
    *end = e;
}

bool rsvp_next(rsvp *r)
{
    uint32_t e;
    if (r->off >= r->len)
        return false;
    for (e = r->off; e < r->len && r->stream[e] != '\n'; e++)
        ;
    r->off = (e < r->len) ? e + 1 : r->len;
    return true;
}

bool rsvp_prev(rsvp *r) /* rule #8: regression must always be available */
{
    uint32_t s = r->off;
    if (s == 0)
        return false;
    if (s > r->len)
        s = r->len;
    s--; /* step back over the '\n' (or past the parked-at-end position) */
    while (s > 0 && r->stream[s - 1] != '\n')
        s--;
    r->off = s;
    return true;
}

void rsvp_seek_chunk(rsvp *r, uint32_t byte_offset)
{
    uint32_t start, end;
    if (byte_offset >= r->len) { /* JS seek clamps to end */
        r->off = r->len;
        return;
    }
    chunk_at(r, byte_offset, &start, &end);
    r->off = start;
}

/* Codepoint boundary test: a byte is a lead byte unless it's a continuation. */
static int is_lead(unsigned char b) { return (b & 0xC0) != 0x80; }

/* Byte offset of codepoint `index` in [start,end); end if index is out of range. */
static uint32_t cp_offset(const char *s, uint32_t start, uint32_t end, uint16_t index)
{
    uint16_t seen = 0;
    uint32_t i = start;
    while (i < end) {
        if (is_lead((unsigned char)s[i])) {
            if (seen == index)
                return i;
            seen++;
        }
        i++;
    }
    return end;
}

static uint16_t cp_count(const char *s, uint32_t start, uint32_t end)
{
    uint16_t n = 0;
    uint32_t i;
    for (i = start; i < end; i++)
        if (is_lead((unsigned char)s[i]))
            n++;
    return n;
}

/* feature 3: count words, not bytes — chunks never contain '\n'. */
static uint16_t word_count(const char *s, uint32_t start, uint32_t end)
{
    uint16_t n = 0;
    int in_word = 0;
    uint32_t i;
    for (i = start; i < end; i++) {
        if (s[i] == ' ') {
            in_word = 0;
        } else if (!in_word) {
            n++;
            in_word = 1;
        }
    }
    return n;
}

/* rule #7: ms = (60000/wpm) * words, then sentence 18/10 or clause 13/10.
 * JS: /[.!?…]["')\]]?$/ -> 1.8, else /[,;:]["')\]]?$/ -> 1.3 — strip the one
 * optional closer, then test the last byte. One floor at the very end —
 * (60000 × words × mult) / (wpm × 10) — stays within 1ms of the JS floats. */
static uint32_t hold_ms(const rsvp *r, uint32_t start, uint32_t end, uint16_t words)
{
    const unsigned char *b = (const unsigned char *)r->stream;
    uint32_t n = end - start;
    uint32_t mult = 10u;
    unsigned char c;
    const uint32_t base = (60000u * words) / r->wpm;
    if (n == 0)
        return base;
    c = b[end - 1];
    if (c == '"' || c == '\'' || c == ')' || c == ']') {
        end--;
        if (start == end)
            return base; /* degenerate closer-only chunk */
        c = b[end - 1];
    }
    if (end - start >= 3 && b[end - 3] == 0xE2 && b[end - 2] == 0x80 &&
        b[end - 1] == 0xA6)
        mult = 18u; /* '…' */
    else if (c == '.' || c == '!' || c == '?')
        mult = 18u;
    else if (c == ',' || c == ';' || c == ':')
        mult = 13u;
    return (60000u * words * mult) / (r->wpm * 10u);
}

rsvp_view rsvp_frame(const rsvp *r)
{
    rsvp_view v = {0, 0, 0, 0, 0, 0};
    uint32_t start, end;
    uint16_t total_cp, pcp;

    if (!r->stream || r->len == 0 || r->off >= r->len)
        return v; /* empty stream -> text NULL, len 0 */

    chunk_at(r, r->off, &start, &end);
    if (start == end)
        return v; /* degenerate empty chunk: nothing to render */
    v.text = r->stream + start;
    v.len = end - start;
    v.words = word_count(r->stream, start, end);
    v.ms = hold_ms(r, start, end, v.words);

    /* rule #6 pivot. JS s.length is UTF-16 units; chunks are BMP text, so we
     * count codepoints (feature 3: byte counts break CJK/Arabic/everything). */
    total_cp = cp_count(r->stream, start, end);
    pcp = (total_cp <= 1)   ? ORP_TABLE[0]
          : (total_cp <= 5)   ? ORP_TABLE[1]
          : (total_cp <= 9)   ? ORP_TABLE[2]
          : (total_cp <= 13)  ? ORP_TABLE[3]
                              : ORP_TABLE[4];
    if (total_cp == 0)
        return v;
    if (pcp > (uint16_t)(total_cp - 1)) /* JS: min(pivotIndex(s), len-1) */
        pcp = (uint16_t)(total_cp - 1);
    while (pcp < (uint16_t)(total_cp - 1) && /* JS split(): never pivot on space */
           r->stream[cp_offset(r->stream, start, end, pcp)] == ' ')
        pcp++;
    v.pivot_byte = (uint16_t)(cp_offset(r->stream, start, end, pcp) - start);
    v.pivot_cp = pcp;
    return v;
}

void rsvp_save(const rsvp *r, uint8_t theme_idx, rsvp_state_blob *out_blob)
{
    out_blob->magic = RSVP_STATE_MAGIC;
    out_blob->version = 1;
    out_blob->wpm = r->wpm;
    out_blob->byte_off = r->off;
    out_blob->theme_idx =
        (theme_idx < RSVP_THEME_PRESETS) ? theme_idx : (uint8_t)(RSVP_THEME_PRESETS - 1);
}

bool rsvp_load(rsvp *r, const rsvp_state_blob *blob, uint8_t *out_theme_idx)
{
    if (blob->magic != RSVP_STATE_MAGIC || blob->version != 1)
        return false;
    rsvp_set_wpm(r, blob->wpm);
    rsvp_seek_chunk(r, blob->byte_off);
    if (out_theme_idx) {
        *out_theme_idx = (blob->theme_idx < RSVP_THEME_PRESETS)
                             ? blob->theme_idx
                             : 0;
    }
    return true;
}
