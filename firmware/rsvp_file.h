/* firmware/rsvp_file.h — windowed SD book reader. Streams a .rsvp file
 * through a small RAM window so book size = card size (no 192KB cap).
 *
 * Why a skip map: a .rsvp file is NOT the raw stream — SOH control lines
 * (0x01 S/T/E) are interleaved between chunk bytes. The reader scans the
 * file once (pass A, reusing rsvp_stream's parser verbatim) to build:
 *   - the section table (fully in RAM, 128 x 48B)
 *   - a skip map: file ranges absent from the logical stream
 * Playback (pass B) keeps a window of stream bytes in RAM around the read
 * position; refill = SD seek + segmented read that compacts around skips.
 *
 * DEVIATIONS (explicit):
 * - rf_attach/rf_sync write rsvp struct fields directly (RSVP_EXPOSE_STATE
 *   required) — the core keeps its contiguous-(stream,len) contract; the
 *   board just re-points it into the window every frame.
 * - RF_SKIP_MAX overflow (book with >~130 control rows) refuses the load
 *   instead of silently truncating.
 * - A single chunk line longer than the window cannot be served; impossible
 *   per companion format (1 word/line, <=63B), guarded by check_rfile.py.
 */
#ifndef RSVP_FILE_H
#define RSVP_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rsvp_core.h"
#include "rsvp_stream.h"

#define RF_MARGIN 256          /* >= 2x longest line (63B): safe scan slack */
#define RF_SKIP_MAX 160        /* 128 S-rows + T + E + slack */
#define RF_DEFAULT_WIN (16 * 1024)

/* Injected storage: read 'len' bytes at absolute FILE offset 'off' into
 * dst; set *got to bytes actually read. false = IO error. */
typedef struct {
    void *ud;
    bool (*read_at)(void *ud, uint32_t off, char *dst, uint32_t len,
                    uint32_t *got);
} rf_io;

typedef struct {
    uint32_t start; /* absolute FILE offset of the skip's first byte */
    uint32_t len;   /* bytes skipped */
    uint32_t stream_start; /* logical stream offset this skip occupies */
} rf_skip;

typedef struct {
    rf_io io;

    char *win;              /* caller-owned window */
    uint32_t win_cap;
    rsvp_section *sections; /* caller-owned, fully in RAM */
    uint16_t max_sections, n_sections;
    char (*titles)[RSVP_STREAM_TITLE_MAX];

    rf_skip skips[RF_SKIP_MAX];
    uint16_t n_skips;
    uint32_t file_len;   /* total file bytes (pass A) */
    uint32_t stream_len; /* logical stream bytes (pass A) */

    uint32_t win_start;  /* logical offset of win[0] */
    uint32_t win_len;    /* logical bytes in win */
    uint32_t pos;        /* reader's logical offset (folded via rf_sync) */

    bool end_seen;
    int theme_req;
    bool overflow;       /* skip map or section table overflowed */
} rf_reader;

void rf_init(rf_reader *f, const rf_io *io, char *win, uint32_t win_cap,
             rsvp_section *sections, uint16_t max_sections,
             char (*titles)[RSVP_STREAM_TITLE_MAX]);

/* Pass A: sequential walk. Builds sections + skip map + lengths.
 * Reuses the serial parser (rs_feed) so behavior cannot diverge.
 * Returns false on IO error or table overflow. */
bool rf_scan(rf_reader *f);

/* Logical stream offset -> absolute file offset (skip-map walk). */
uint32_t rf_file_off(const rf_reader *f, uint32_t logical);

/* Pass B: make sure the window covers logical_pos +- RF_MARGIN; refill
 * (seek + segmented compacting read) if not. Returns false on IO error or
 * when a single line cannot fit the window. */
bool rf_prime(rf_reader *f, uint32_t logical_pos);

/* Board loop, around each core call (need RSVP_EXPOSE_STATE):
 *   rf_attach(f, &r);  v = rsvp_frame(&r);  ...;  rsvp_next(&r);  rf_sync(f, &r);
 * rf_attach refills + re-points r into the window; rf_sync folds the
 * window-relative offset back into f->pos (persist f->pos for resume). */
#ifdef RSVP_EXPOSE_STATE
void rf_attach(rf_reader *f, rsvp *r);
void rf_sync(rf_reader *f, rsvp *r);
#endif

#ifdef __cplusplus
}
#endif

#endif /* RSVP_FILE_H */
