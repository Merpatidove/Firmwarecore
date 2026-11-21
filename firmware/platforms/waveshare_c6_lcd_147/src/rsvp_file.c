/* firmware/rsvp_file.c — windowed SD book reader, see rsvp_file.h.
 * Freestanding C99, no libc: manual loops, no memset/strchr.
 *
 * Pass A (rf_scan): sequential walk, feeding every byte through the REAL
 * serial parser (rsvp_stream) into a 2-byte sink buffer — chunk bytes are
 * dropped but section/theme/E parsing is bit-identical to serial ingest.
 * While feeding, each byte is accounted skip (SOH + control-line bytes) or
 * keep (stream bytes), building the skip map.
 *
 * Pass B (rf_prime/rf_attach): windowed segmented reads around the read
 * position; logical->file mapping walks the skip map. */
#include "rsvp_file.h"

/* The real scan: one persistent parser + skip accounting per byte. */
bool rf_scan(rf_reader *f)
{
    char sink[2]; /* chunk bytes go here and die: we only want parsing */
    /* NOTE: rs_init needs the section storage; pass it straight through. */
    rsvp_stream ps;
    rs_init(&ps, sink, sizeof(sink), f->sections, f->max_sections,
            f->titles);

    f->n_skips = 0;
    f->n_sections = 0;
    f->file_len = 0;
    f->stream_len = 0;
    f->end_seen = false;
    f->theme_req = -1;
    f->overflow = false;

    /* sequential walk in 4KB chunks */
    static const uint32_t CH = 4096;
    uint32_t file_off = 0;
    bool in_ctl = false; /* mirror of ps.in_ctl BEFORE feeding each byte */

    for (;;) {
        char rb[4096];
        uint32_t got = 0;
        if (!f->io.read_at(f->io.ud, file_off, rb, CH, &got))
            return false;
        if (got == 0)
            break; /* EOF */

        for (uint32_t i = 0; i < got; i++) {
            in_ctl = ps.in_ctl;
            char c = rb[i];
            rs_feed(&ps, c);
            /* a byte is a SKIP byte iff it belongs to a control line:
             * the SOH marker itself, or any byte while in_ctl */
            bool skip = (c == '\x01') || in_ctl;
            if (ps.n_sections > f->max_sections)
                f->overflow = true;

            if (skip) {
                if (f->n_skips > 0) {
                    rf_skip *last = &f->skips[f->n_skips - 1];
                    if (last->start + last->len == file_off + i) {
                        last->len++;
                        continue;
                    }
                }
                if (f->n_skips >= RF_SKIP_MAX) {
                    f->overflow = true;
                    return false;
                }
                f->skips[f->n_skips].start = file_off + i;
                f->skips[f->n_skips].len = 1;
                f->skips[f->n_skips].stream_start = f->stream_len;
                f->n_skips++;
            } else {
                f->stream_len++;
            }
        }
        file_off += got;
    }

    f->file_len = file_off;
    f->n_sections = ps.n_sections;
    f->end_seen = ps.end_seen;
    f->theme_req = ps.theme_req;
    return !f->overflow;
}

/* ---- logical -> file offset mapping -------------------------------- */

/* Map logical stream offset -> absolute file offset using stream_start:
 * a skip with stream_start <= logical shifts everything after it. Skips
 * are ordered; find the LAST skip whose stream_start <= logical. */
uint32_t rf_file_off(const rf_reader *f, uint32_t logical)
{
    uint32_t file_off = logical;
    for (uint16_t i = 0; i < f->n_skips; i++) {
        if (f->skips[i].stream_start <= logical)
            file_off += f->skips[i].len;
        else
            break;
    }
    return file_off;
}

/* ---- pass B: window management ------------------------------------- */

/* Segmented read: fill dst with logical range [logical, logical+len) by
 * concatenating the non-skip file runs. Returns false on IO error or if
 * the range passes EOF. */
static bool read_logical(rf_reader *f, uint32_t logical, char *dst,
                         uint32_t len)
{
    if (logical + len > f->stream_len)
        return false;
    uint32_t done = 0;
    while (done < len) {
        uint32_t cur = logical + done;
        uint32_t fo = rf_file_off(f, cur);
        /* longest contiguous logical run: distance to next skip boundary
         * in logical coordinates */
        uint32_t run = len - done;
        for (uint16_t i = 0; i < f->n_skips; i++) {
            const rf_skip *sk = &f->skips[i];
            if (sk->stream_start > cur) {
                uint32_t d = sk->stream_start - cur;
                if (d < run)
                    run = d;
                break; /* skips ordered: first one past cur wins */
            }
        }
        if (fo + run > f->file_len)
            run = f->file_len - fo; /* clamp at EOF (defensive) */
        if (run == 0)
            return false;
        uint32_t got = 0;
        if (!f->io.read_at(f->io.ud, fo, dst + done, run, &got))
            return false;
        if (got == 0)
            return false;
        done += got;
    }
    return true;
}

bool rf_prime(rf_reader *f, uint32_t logical_pos)
{
    if (logical_pos > f->stream_len)
        logical_pos = f->stream_len;

    /* window start: back up a quarter window so backward scans (prev,
     * line-start search) have room. On big windows keep a full RF_MARGIN
     * behind; on small ones never sacrifice forward room. pos must land
     * strictly inside the window either way. */
    uint32_t back = f->win_cap / 4;
    if (back > logical_pos)
        back = logical_pos;
    if (f->win_cap >= 4 * RF_MARGIN && back < RF_MARGIN &&
        logical_pos > RF_MARGIN)
        back = RF_MARGIN;
    uint32_t start = logical_pos - back;

    uint32_t avail = f->stream_len - start;
    uint32_t len = f->win_cap < avail ? f->win_cap : avail;
    /* pos at the window's trailing edge is only legal as the parked
     * end-of-stream state (off == len == stream_len): frame is empty, no
     * scan happens. For any non-final pos, slide the window back so the
     * line scan at pos has its byte inside. */
    if (logical_pos < f->stream_len && logical_pos - start >= len &&
        start > 0) {
        start = logical_pos + 1 - len; /* shift window back, same length */
        avail = f->stream_len - start;
        len = f->win_cap < avail ? f->win_cap : avail;
    }
    if (len < 2 || start > logical_pos)
        return false;
    if (logical_pos - start >= len && logical_pos != f->stream_len)
        return false; /* non-park pos would land outside the window */

    if (!read_logical(f, start, f->win, len))
        return false;

    f->win_start = start;
    f->win_len = len;
    f->pos = logical_pos;
    return true;
}

/* ---- board-side attach/sync ---------------------------------------- */

#ifdef RSVP_EXPOSE_STATE
void rf_attach(rf_reader *f, rsvp *r)
{
    uint32_t pos = f->pos;
    /* effective margin shrinks with small windows: the core never scans
     * further than one line (<=63B) each side of pos */
    uint32_t eff = RF_MARGIN;
    if (f->win_cap / 4 < eff)
        eff = f->win_cap / 4;
    bool ok = pos >= f->win_start &&
              pos + eff <= f->win_start + f->win_len &&
              pos - f->win_start >= eff;
    if (!ok)
        rf_prime(f, pos);

    uint32_t rel = f->pos - f->win_start;
    r->stream = f->win;
    r->len = f->win_len;
    r->off = rel;
}

void rf_sync(rf_reader *f, rsvp *r)
{
    f->pos = f->win_start + r->off;
}
#endif
