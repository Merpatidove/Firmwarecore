/* firmware/test_core.c — assert self-check for the firmware core. Build:
 *   cc -std=c99 -Wall -Wextra -o test_core test_core.c rsvp_core.c && ./test_core
 * Or cross-compile-only (no host C toolchain needed to verify it compiles):
 *   riscv32-esp-elf-gcc -std=c99 -Wall -Wextra -Wpedantic --freestanding -Os \
 *     -DRSVP_TEST -c test_core.c rsvp_core.c
 * (RSVP_TEST swaps assert for a no-op so freestanding builds don't need libc.) */
#if defined(RSVP_TEST)
#undef assert
#define assert(c) ((void)0)
#else
#include <assert.h>
#endif
#include <stdio.h>
#include <string.h>
#include "rsvp_core.h"

int main(void)
{
    /* ---- colors (feature 5) ----------------------------------------
     * rgb565 values verified by re-running the macro bit math in node */
    assert(RSVP_ACCENTS[0].rgb565 == 0xFD88); /* amber #ffb347 */
    assert(RSVP_ACCENTS[1].rgb565 == 0xFFFF); /* white #ffffff */
    assert(RSVP_ACCENTS[2].rgb565 == 0x7F30); /* green #7ee787 */
    assert(RSVP_ACCENTS[3].rgb565 == 0xFBCD); /* red #ff7a6e */
    assert(RSVP_ACCENTS[4].rgb565 == 0x7E1F); /* blue #79c0ff */
    assert(RSVP_BG_DEFAULT.rgb565 == 0x0841); /* #08090c */
    assert(RSVP_FG_DEFAULT.rgb565 == 0xEF3B); /* #e9e4dc */
    assert(RSVP_GUIDE_DEFAULT.rgb565 == 0x2966); /* #2a2e36 */

    /* ---- pivot (rule #6) ------------------------------------------- */
    {
        /* actual JS chunker output (size 2) of
         * "alpha beta gamma, delta epsilon zeta." */
        const char *stream = "alpha beta gamma,\ndelta epsilon zeta.";
        rsvp r;
        rsvp_init(&r, stream, (uint32_t)strlen(stream), 0, 0);

        rsvp_view v = rsvp_frame(&r);
        assert(v.pivot_cp == 4 && v.pivot_byte == 4); /* 17cp chunk -> ORP[4] */
        assert(v.words == 3 && v.ms == 780u);         /* ',' clause: 600*13/10 */

        assert(rsvp_next(&r));
        v = rsvp_frame(&r);
        assert(v.pivot_cp == 4 && v.text[v.pivot_byte] == 'a'); /* 19cp */
        assert(v.words == 3 && v.ms == 1080u);        /* '.': 600*18/10 */

        assert(!rsvp_next(&r));                       /* end */
        v = rsvp_frame(&r);
        assert(v.text == NULL && v.len == 0);         /* parked at end -> empty */
        assert(rsvp_prev(&r));                        /* walk back */
        v = rsvp_frame(&r);
        assert(strncmp(v.text, "delta epsilon zeta.", v.len) == 0);
        assert(rsvp_prev(&r));
        v = rsvp_frame(&r);
        assert(strncmp(v.text, "alpha beta gamma,", v.len) == 0);
        assert(!rsvp_prev(&r));                       /* start */
        rsvp_seek_chunk(&r, 0);
        v = rsvp_frame(&r);
        assert(strncmp(v.text, "alpha beta gamma,", v.len) == 0);

        /* wpm: feature 1 */
        rsvp_set_wpm(&r, 0);
        assert(rsvp_wpm(&r) == 300);
        rsvp_set_wpm(&r, 10);
        assert(rsvp_wpm(&r) == 60); /* clamp >=60, rule #1 */
        rsvp_set_wpm(&r, 600);
        assert(rsvp_wpm(&r) == 600);
    }

    /* ---- pivot details: never-on-space, UTF-8 (feature 3) ---------- */
    {
        rsvp r;
        rsvp_view v;

        rsvp_init(&r, "a b c", 5, 0, 0); /* multi-word: pivot skips spaces */
        v = rsvp_frame(&r);
        assert(v.pivot_cp == 2 && v.text[v.pivot_byte] == 'b');

        rsvp_init(&r, "hello", 5, 0, 0); /* 'hello' -> cp 1 (JS vector) */
        v = rsvp_frame(&r);
        assert(v.pivot_cp == 1 && v.pivot_byte == 1 && v.text[1] == 'e');
        assert(v.words == 1 && v.ms == 200); /* 300wpm, 1 word */

        rsvp_init(&r, "abcdefghij", 10, 0, 0); /* 10 chars -> cp 3 */
        v = rsvp_frame(&r);
        assert(v.pivot_cp == 3 && v.text[3] == 'd');

        /* CJK: こんにちは = 5 codepoints / 15 bytes -> pivot cp 1 = byte 3 */
        rsvp_init(&r, "\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf", 15, 0, 0);
        v = rsvp_frame(&r);
        assert(v.words == 1);
        assert(v.pivot_cp == 1 && v.pivot_byte == 3);

        /* naïve = 5 codepoints, 6 bytes -> pivot cp 1 = byte 1 */
        rsvp_init(&r, "na\xc3\xafve", 6, 0, 0);
        v = rsvp_frame(&r);
        assert(v.pivot_cp == 1 && v.pivot_byte == 1);

        /* "ab cdé": 6cp -> ORP[1], lands on the space -> skip to cp 3 'c' */
        rsvp_init(&r, "ab cd\xc3\xa9", 7, 0, 0);
        v = rsvp_frame(&r);
        assert(v.pivot_cp == 3 && v.text[v.pivot_byte] == 'c');

        /* 14-codepoint word -> ORP_TABLE[4] = 4 */
        rsvp_init(&r, "abcdefghijklmn", 14, 0, 0);
        v = rsvp_frame(&r);
        assert(v.pivot_cp == 4 && v.text[4] == 'e');
    }

    /* ---- timing (rule #7) ------------------------------------------ */
    {
        rsvp r;
        rsvp_init(&r, "word", 4, 0, 0);
        assert(rsvp_frame(&r).ms == 200);
        rsvp_init(&r, "word.", 5, 0, 0);
        assert(rsvp_frame(&r).ms == 360);
        rsvp_init(&r, "word!", 5, 0, 0);
        assert(rsvp_frame(&r).ms == 360);
        rsvp_init(&r, "(word.)", 7, 0, 0);
        assert(rsvp_frame(&r).ms == 360); /* closer after sentence punct */
        rsvp_init(&r, "word,", 5, 0, 0);
        assert(rsvp_frame(&r).ms == 260); /* 200*13/10 */
        rsvp_init(&r, "word;", 5, 0, 0);
        assert(rsvp_frame(&r).ms == 260);
        rsvp_init(&r, "word:", 5, 0, 0);
        assert(rsvp_frame(&r).ms == 260);
        rsvp_init(&r, "word\"", 5, 0, 0);
        assert(rsvp_frame(&r).ms == 200); /* bare closer: no punct = base */
        rsvp_init(&r, "word,\xe2\x80\xa6", 9, 0, 0); /* '…' wins over ',' */
        assert(rsvp_frame(&r).ms == 360);
        rsvp_init(&r, "(word.),", 8, 0, 0); /* ',' after closers: clause */
        assert(rsvp_frame(&r).ms == 260);
        rsvp_init(&r, "word... wait", 12, 0, 0); /* '...' is NOT '…': base */
        assert(rsvp_frame(&r).ms == 400); /* 2 words */
        /* ellipsis chunk ending in … (companion sanitize makes these) */
        rsvp_init(&r, "word \xe2\x80\xa6", 8, 0, 0);
        assert(rsvp_frame(&r).ms == 720); /* 2 words: 400*18/10 */
        rsvp_init(&r, "two words.", 10, 0, 0);
        assert(rsvp_frame(&r).ms == 720); /* 400*18/10 */
        rsvp_set_wpm(&r, 600);
        assert(rsvp_frame(&r).ms == 360); /* faster wpm -> shorter */
        rsvp_set_wpm(&r, 60);
        assert(rsvp_frame(&r).ms == 3600); /* 1000*18/10 */
    }

    /* ---- sections (rule #15, feature 2) ---------------------------- */
    {
        /* byte offsets of chunk starts: "alpha beta\ngamma delta\n..." */
        const char *stream = "alpha beta\ngamma delta\nomega.";
        const rsvp_section secs[] = {{0, "Ch 1"}, {11, "Ch 2"}};
        rsvp r;
        const char *title = 0;
        rsvp_init(&r, stream, (uint32_t)strlen(stream), secs, 2);
        assert(rsvp_section_count(&r) == 2);
        assert(rsvp_section_at(&r, 0, &title) == 0 && strcmp(title, "Ch 1") == 0);
        assert(rsvp_section_at(&r, 1, &title) == 11 && strcmp(title, "Ch 2") == 0);
        assert(rsvp_section_at(&r, 2, &title) == 0xFFFFFFFFu);
        assert(rsvp_section_at(&r, 2, 0) == 0xFFFFFFFFu); /* NULL title ok */
        rsvp_seek_chunk(&r, rsvp_section_at(&r, 1, 0));
        {
            rsvp_view v = rsvp_frame(&r);
            assert(strncmp(v.text, "gamma delta", v.len) == 0);
        }
        /* seek inside a chunk snaps to the chunk start (11 -> 11 here) */
        rsvp_seek_chunk(&r, 13);
        assert(rsvp_frame(&r).text[0] == 'g');
        /* seek past end clamps */
        rsvp_seek_chunk(&r, 9999);
        assert(rsvp_frame(&r).text == NULL);
        assert(!rsvp_next(&r));
        rsvp_prev(&r); /* from parked end */
        {
            rsvp_view v = rsvp_frame(&r);
            assert(strncmp(v.text, "omega.", v.len) == 0);
        }
    }

    /* ---- empty stream ---------------------------------------------- */
    {
        rsvp r;
        rsvp_init(&r, "", 0, 0, 0);
        rsvp_view v = rsvp_frame(&r);
        assert(v.text == NULL && v.len == 0 && v.ms == 0);
        assert(!rsvp_next(&r));
        assert(!rsvp_prev(&r));
        assert(rsvp_wpm(&r) == 300);
    }

    /* ---- themes (feature 5) ---------------------------------------- */
    {
        assert(RSVP_THEME_PRESETS == 4);
        /* night preset bg must equal the app default */
        assert(RSVP_THEMES[0].bg.rgb565 == RSVP_BG_DEFAULT.rgb565);
        assert(RSVP_THEMES[0].fg.rgb565 == RSVP_FG_DEFAULT.rgb565);
        assert(RSVP_THEMES[0].guide.rgb565 == RSVP_GUIDE_DEFAULT.rgb565);
        assert(RSVP_THEMES[0].accent.rgb565 == RSVP_ACCENTS[0].rgb565);
        /* paper: light bg, dark fg */
        assert(RSVP_THEMES[1].bg.r == 0xf4 && RSVP_THEMES[1].fg.r == 0x1a);
        /* every preset's name non-empty, 4 colors each */
        for (int i = 0; i < RSVP_THEME_PRESETS; i++) {
            assert(RSVP_THEMES[i].name[0] != 0);
        }
    }

    /* ---- state save/load (persistence) ------------------------------ */
    {
        const char *stream = "alpha beta\ngamma delta\nomega.";
        rsvp r, r2;
        rsvp_state_blob blob;
        uint8_t theme = 99; /* deliberately out of range */

        rsvp_init(&r, stream, (uint32_t)strlen(stream), 0, 0);
        rsvp_set_wpm(&r, 450);
        rsvp_seek_chunk(&r, 11); /* "gamma delta" */
        rsvp_save(&r, 2, &blob);
        assert(blob.magic == 0x52535031u && blob.version == 1);
        assert(blob.wpm == 450 && blob.byte_off == 11 && blob.theme_idx == 2);

        /* round-trip into a fresh instance */
        rsvp_init(&r2, stream, (uint32_t)strlen(stream), 0, 0);
        assert(rsvp_load(&r2, &blob, &theme));
        assert(rsvp_wpm(&r2) == 450);
        assert(theme == 2);
        {
            rsvp_view v = rsvp_frame(&r2);
            assert(strncmp(v.text, "gamma delta", v.len) == 0);
        }

        /* theme_idx clamps on save, resets on load */
        rsvp_save(&r, 200, &blob);
        assert(blob.theme_idx == RSVP_THEME_PRESETS - 1);
        theme = 123;
        blob.theme_idx = 250;
        assert(rsvp_load(&r2, &blob, &theme) && theme == 0);

        /* garbage blob rejected */
        blob.magic = 0xDEAD;
        assert(!rsvp_load(&r2, &blob, &theme));
        blob.magic = RSVP_STATE_MAGIC;
        blob.version = 7;
        assert(!rsvp_load(&r2, &blob, &theme));
    }

    printf("firmware core self-check: OK\n");
    return 0;
}
