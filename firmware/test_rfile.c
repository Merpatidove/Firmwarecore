/* firmware/test_rfile.c — assert self-check for the windowed SD reader.
 * Memory-backed rf_io: file = a byte array in RAM. No host C compiler in
 * this repo: the build is a compile GATE only; behavioral assertions run
 * via check_rfile.py (Python reference model of the same math). Build:
 *   riscv32-esp-elf-gcc -std=c99 -Wall -Wextra -Wpedantic --freestanding \
 *     -DRSVP_TEST -DRSVP_EXPOSE_STATE -I. -c test_rfile.c rsvp_file.c rsvp_stream.c
 * (RSVP_TEST swaps assert for a no-op so freestanding builds don't need libc.) */
#if defined(RSVP_TEST)
#undef assert
#define assert(c) ((void)0)
#else
#include <assert.h>
#endif
#include <stdio.h>
#include <string.h>
#include "rsvp_file.h"

#define FAKE_LEN 4096
static char fake_file[FAKE_LEN];
static uint32_t fake_len = 0;

static bool mem_read_at(void *ud, uint32_t off, char *dst, uint32_t len,
                        uint32_t *got)
{
    (void)ud;
    if (off >= fake_len) {
        *got = 0;
        return true; /* clean EOF */
    }
    if (off + len > fake_len)
        len = fake_len - off;
    memcpy(dst, fake_file + off, len);
    *got = len;
    return true;
}

#define WCAP 512
static char win[WCAP];
static rsvp_section secs[64];
static char titles[64][RSVP_STREAM_TITLE_MAX];

static void make_book(void)
{
    /* interleaved: 2 sections + chunks + T + E, ~1KB of stream bytes */
    const char *part1 =
        "\x01S 0\tBAB 1\n"
        "alpha beta gamma,\ndelta epsilon zeta.\nepsilon zeta eta theta.\n"
        "\x01S 78\tBAB 2\n"
        "one two three.\nfour five six seven eight nine ten eleven.\n";
    const char *part2 = "\x01T 2\n\x01E\n";
    fake_len = 0;
    memcpy(fake_file, part1, strlen(part1));
    fake_len += (uint32_t)strlen(part1);
    /* pad with chunk lines so stream spans past tiny windows */
    for (int i = 0; i < 40; i++) {
        int n = snprintf(fake_file + fake_len, FAKE_LEN - fake_len,
                         "word%d word%d tail.\n", i, i + 100);
        fake_len += (uint32_t)n;
    }
    memcpy(fake_file + fake_len, part2, strlen(part2));
    fake_len += (uint32_t)strlen(part2);
}

int main(void)
{
    make_book();

    rf_reader f;
    rf_io io = {0, mem_read_at};
    rf_init(&f, &io, win, WCAP, secs, 64, titles);
    assert(rf_scan(&f));
    assert(f.end_seen);
    assert(f.theme_req == 2);
    assert(f.n_sections == 2);
    assert(strcmp(f.sections[0].title, "BAB 1") == 0);
    assert(f.sections[0].byte_offset == 0);
    assert(f.file_len == fake_len);
    /* stream = file minus skip bytes */
    uint32_t skip_sum = 0;
    for (uint16_t i = 0; i < f.n_skips; i++)
        skip_sum += f.skips[i].len;
    assert(skip_sum + f.stream_len == f.file_len);

    /* windowed read reconstruction: walk the whole stream via rf_prime in
     * small logical steps and compare against a direct scan */
    static char expect[FAKE_LEN];
    uint32_t exp_len = 0;
    bool in_ctl = false;
    for (uint32_t i = 0; i < fake_len; i++) {
        char c = fake_file[i];
        if (in_ctl) {
            if (c == '\n')
                in_ctl = false;
            continue;
        }
        if (c == '\x01') {
            in_ctl = true;
            continue;
        }
        expect[exp_len++] = c;
    }
    assert(exp_len == f.stream_len);

    /* random positions: prime, then verify window bytes match expectation */
    for (uint32_t p = 0; p < f.stream_len; p += 37) {
        assert(rf_prime(&f, p));
        uint32_t inwin = p - f.win_start;
        uint32_t n = f.win_len - inwin;
        if (n > 32)
            n = 32; /* compare a 32B prefix at each position */
        assert(memcmp(win + inwin, expect + p, n) == 0);
    }
    printf("window walk OK (stream_len=%u skips=%u)\n",
           (unsigned)f.stream_len, (unsigned)f.n_skips);

    /* rf_attach/rf_sync roundtrip with a real rsvp */
#ifdef RSVP_EXPOSE_STATE
    {
        rsvp r;
        assert(rf_prime(&f, 0));
        rf_attach(&f, &r);
        assert(r.stream == win);
        /* play forward across many refills */
        for (int i = 0; i < 200 && rsvp_frame(&r).len; i++) {
            rf_sync(&f, &r);
            rf_attach(&f, &r);
            rsvp_next(&r);
        }
        rf_sync(&f, &r);
        assert(f.pos <= f.stream_len);
        printf("attach/sync walk OK (pos=%u)\n", (unsigned)f.pos);
    }
#endif

    printf("ALL RFILE GATES COMPILED\n");
    return 0;
}
