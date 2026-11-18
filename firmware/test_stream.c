/* firmware/test_stream.c — assert self-check for the SOH stream parser
 * (rsvp_stream: shared by serial ingest and .rsvp SD files). Build:
 *   cc -std=c99 -Wall -Wextra -o test_stream test_stream.c rsvp_stream.c && ./test_stream
 * Or cross-compile-only (no host C toolchain needed to verify it compiles):
 *   riscv32-esp-elf-gcc -std=c99 -Wall -Wextra -Wpedantic --freestanding \
 *     -DRSVP_TEST -c test_stream.c rsvp_stream.c
 * (RSVP_TEST swaps assert for a no-op so freestanding builds don't need libc.) */
#if defined(RSVP_TEST)
#undef assert
#define assert(c) ((void)0)
#else
#include <assert.h>
#endif
#include <stdio.h>
#include <string.h>
#include "rsvp_stream.h"

#define CAP 512
#define MAX_SEC 8

static char buf[CAP];
static rsvp_section secs[MAX_SEC];
static char titles[MAX_SEC][RSVP_STREAM_TITLE_MAX];

static void feed_str(rsvp_stream *s, const char *str)
{
    while (*str)
        rs_feed(s, *str++);
}

int main(void)
{
    /* ---- basic: sections + chunks + E ------------------------------ */
    {
        rsvp_stream s;
        rs_init(&s, buf, CAP, secs, MAX_SEC, titles);
        feed_str(&s,
                 "\x01S 0\tBAB 1\n"
                 "alpha beta gamma,\n"
                 "\x01S 18\tBAB 2\n"
                 "delta epsilon zeta.\n"
                 "\x01E\n");
        assert(s.n_sections == 2);
        assert(s.buf_len == (uint32_t)strlen("alpha beta gamma,\ndelta epsilon zeta.\n"));
        assert(memcmp(buf, "alpha beta gamma,\ndelta epsilon zeta.\n", s.buf_len) == 0);
        assert(s.sections[0].byte_offset == 0);
        assert(strcmp(s.sections[0].title, "BAB 1") == 0);
        assert(s.sections[1].byte_offset == 18);
        assert(strcmp(s.sections[1].title, "BAB 2") == 0);
        assert(s.end_seen);
        assert(!s.truncated);
        printf("basic OK\n");
    }

    /* ---- deviation-8 regression: two S rows = two DISTINCT titles -- */
    {
        rsvp_stream s;
        rs_init(&s, buf, CAP, secs, MAX_SEC, titles);
        feed_str(&s, "\x01S 0\tPendahuluan\n\x01S 40\tTujuan\n\x01E\n");
        assert(s.n_sections == 2);
        assert(strcmp(s.sections[0].title, "Pendahuluan") == 0);
        assert(strcmp(s.sections[1].title, "Tujuan") == 0);
        assert(s.sections[0].title != s.sections[1].title);
        printf("title aliasing OK\n");
    }

    /* ---- theme request --------------------------------------------- */
    {
        rsvp_stream s;
        rs_init(&s, buf, CAP, secs, MAX_SEC, titles);
        feed_str(&s, "\x01T 2\n\x01E\n");
        assert(s.theme_req == 2);
        assert(s.end_seen);
        assert(s.buf_len == 0);
        printf("theme OK\n");
    }

    /* ---- truncation: over-cap chunk bytes set the flag ------------- */
    {
        rsvp_stream s;
        rs_init(&s, buf, 16, secs, MAX_SEC, titles); /* tiny cap */
        for (int i = 0; i < 64; i++)
            rs_feed(&s, 'x');
        assert(s.truncated);
        assert(s.buf_len == 15); /* cap-1: the last byte is kept for NUL */
        printf("truncation OK\n");
    }

    /* ---- section overflow: clamp at max_sections ------------------- */
    {
        rsvp_stream s;
        rs_init(&s, buf, CAP, secs, 2 /* only 2 slots */, titles);
        feed_str(&s, "\x01S 0\ta\n\x01S 1\tb\n\x01S 2\tc\n\x01E\n");
        assert(s.n_sections == 2); /* third row dropped, no overrun */
        assert(s.end_seen);
        printf("section clamp OK\n");
    }

    /* ---- bytes after E are ignored ---------------------------------- */
    {
        rsvp_stream s;
        rs_init(&s, buf, CAP, secs, MAX_SEC, titles);
        feed_str(&s, "\x01E\ntrailing garbage\n");
        assert(s.end_seen);
        assert(s.buf_len == 0);
        printf("post-E ignore OK\n");
    }

    /* ---- rsvp_core integration: parsed buffer plays ---------------- */
    {
        rsvp_stream s;
        rs_init(&s, buf, CAP, secs, MAX_SEC, titles);
        feed_str(&s, "alpha beta gamma,\ndelta epsilon zeta.\n\x01E\n");
#ifdef RSVP_EXPOSE_STATE
        rsvp r;
        rsvp_init(&r, buf, s.buf_len, secs, s.n_sections);
        rsvp_view v = rsvp_frame(&r);
        assert(v.words == 3 && v.ms == 780u); /* ',' clause: 600*3*13/10 */
#endif
        printf("core integration OK\n");
    }

    printf("ALL STREAM TESTS PASSED\n");
    return 0;
}
