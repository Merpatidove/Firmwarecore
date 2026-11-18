/* firmware/rsvp_stream.c — implementation of rsvp_stream.h. Freestanding
 * C99: manual atol/atoi replacements keep this host- and device-compilable
 * without libc. See header for wire format + title-aliasing bugfix note. */
#include "rsvp_stream.h"

static uint32_t rs_atol(const char *p)
{
    uint32_t v = 0;
    while (*p >= '0' && *p <= '9')
        v = v * 10u + (uint32_t)(*p++ - '0');
    return v;
}

static int rs_atoi(const char *p)
{
    int v = 0, neg = 0;
    if (*p == '-') {
        neg = 1;
        p++;
    }
    while (*p >= '0' && *p <= '9')
        v = v * 10 + (*p++ - '0');
    return neg ? -v : v;
}

void rs_init(rsvp_stream *s, char *buf, uint32_t buf_cap,
             rsvp_section *sections, uint16_t max_sections,
             char (*titles)[RSVP_STREAM_TITLE_MAX])
{
    s->buf = buf;
    s->buf_cap = buf_cap;
    s->buf_len = 0;
    s->truncated = false;
    s->sections = sections;
    s->max_sections = max_sections;
    s->n_sections = 0;
    s->titles = titles;
    s->ctl_len = 0;
    s->in_ctl = false;
    s->end_seen = false;
    s->theme_req = -1;
}

void rs_feed(rsvp_stream *s, char c)
{
    if (s->end_seen)
        return; /* post-E bytes (e.g. trailing newline) are ignored */

    if (s->in_ctl) { /* accumulating a control line after SOH */
        if (c == '\n') { /* control line complete: dispatch */
            s->in_ctl = false;
            s->ctl[s->ctl_len] = 0;
            if (s->ctl[0] == 'S' && s->ctl[1] == ' ' &&
                s->n_sections < s->max_sections) {
                /* find the tab manually (no strchr in freestanding) */
                uint8_t i = 2;
                while (i < s->ctl_len && s->ctl[i] != '\t')
                    i++;
                if (i < s->ctl_len) {
                    s->ctl[i] = 0;
                    uint32_t off = rs_atol(s->ctl + 2);
                    const char *t = s->ctl + i + 1;
                    uint8_t j = 0;
                    while (t[j] && j < RSVP_STREAM_TITLE_MAX - 1) {
                        s->titles[s->n_sections][j] = t[j];
                        j++;
                    }
                    s->titles[s->n_sections][j] = 0;
                    s->sections[s->n_sections].byte_offset = off;
                    s->sections[s->n_sections].title =
                        s->titles[s->n_sections];
                    s->n_sections++;
                }
            } else if (s->ctl[0] == 'E') {
                s->end_seen = true;
            } else if (s->ctl[0] == 'T') {
                s->theme_req = rs_atoi(s->ctl + 1);
            }
        } else if (s->ctl_len < RSVP_STREAM_CTL_MAX - 1) {
            s->ctl[s->ctl_len++] = c;
        }
        return;
    }

    if (c == '\x01') { /* SOH: start a control line */
        s->ctl_len = 0;
        s->in_ctl = true;
        return;
    }

    /* plain chunk byte */
    if (s->buf_len + 1 < s->buf_cap)
        s->buf[s->buf_len++] = c;
    else
        s->truncated = true;
}
