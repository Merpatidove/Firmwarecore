/* firmware/rsvp_stream.h — shared SOH stream parser: turns a byte stream
 * (serial ingest OR a .rsvp SD file — they are byte-identical) into the
 * book buffer + section table. Pure C99, freestanding (no libc): host-
 * testable via test_stream.c, same pattern as rsvp_core.h.
 *
 * Wire format: plain '\n'-terminated lines = chunk bytes (stored verbatim,
 * one word per line); control lines start with SOH (0x01):
 *   0x01 S <byte_offset>\t<title>   section row (repeatable)
 *   0x01 E                          end of transmission -> start playback
 *   0x01 T <idx>                    theme preset request
 *
 * DEVIATION (bugfix while factoring): section titles are copied into a
 * caller-owned titles[] table — the old parser pointed titles[] at a single
 * shared control-line buffer, so every title aliased the LAST line seen. */
#ifndef RSVP_STREAM_H
#define RSVP_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rsvp_core.h"

#define RSVP_STREAM_TITLE_MAX 48
#define RSVP_STREAM_CTL_MAX 96

typedef struct {
    /* book buffer (caller-owned storage) */
    char *buf;
    uint32_t buf_cap;
    uint32_t buf_len;
    bool truncated; /* buffer full: further chunk bytes dropped */

    /* section table */
    rsvp_section *sections;
    uint16_t max_sections;
    uint16_t n_sections;
    char (*titles)[RSVP_STREAM_TITLE_MAX]; /* stable storage for .title */

    /* control-line assembly */
    char ctl[RSVP_STREAM_CTL_MAX];
    uint8_t ctl_len;
    bool in_ctl;

    /* protocol results */
    bool end_seen;  /* 0x01 E received */
    int theme_req;  /* 0x01 T <idx>, -1 = none seen */
} rsvp_stream;

void rs_init(rsvp_stream *s, char *buf, uint32_t buf_cap,
             rsvp_section *sections, uint16_t max_sections,
             char (*titles)[RSVP_STREAM_TITLE_MAX]);
void rs_feed(rsvp_stream *s, char c);

#ifdef __cplusplus
}
#endif

#endif /* RSVP_STREAM_H */