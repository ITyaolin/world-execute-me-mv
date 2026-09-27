/* lyrics.c -- karaoke overlay and the big keyword typography.
 *
 * Everything here is a pure function of the playback time, which keeps the
 * offline export (-export) pixel-identical to the live show.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mv.h"
#include "lyrics.h"

#define LEAD_MS 300        /* how early a phrase appears on screen */
#define HOLD_MS 1200       /* how long it lingers after the voice stops */

static const MvLyric *line_at(int idx)
{
    if (idx < 0 || idx >= LYRIC_LINE_COUNT) return NULL;
    return &LYRIC_LINES[idx];
}

int lyric_line_at(int t_ms)
{
    int best = -1;
    for (int i = 0; i < LYRIC_LINE_COUNT; i++) {
        if (LYRIC_LINES[i].start_ms <= t_ms) best = i;
        else break;
    }
    if (best < 0) return -1;
    if (t_ms > LYRIC_LINES[best].end_ms + HOLD_MS) return -1;
    return best;
}

int lyric_display_line(int t_ms)
{
    int best = -1;
    for (int i = 0; i < LYRIC_LINE_COUNT; i++) {
        if (LYRIC_LINES[i].start_ms - LEAD_MS <= t_ms) best = i;
        else break;
    }
    if (best < 0) return -1;
    if (t_ms > LYRIC_LINES[best].end_ms + HOLD_MS) return -1;
    return best;
}

const char *lyric_text(int index)
{
    const MvLyric *l = line_at(index);
    return l ? l->text : "";
}

/* Index (inside the line) of the keyword whose onset is closest below t_ms, or
 * -1.  *age receives the seconds since that onset. */
int lyric_keyword_at(int t_ms, const char **text, double *age)
{
    int line = lyric_display_line(t_ms);
    if (line < 0) return -1;
    const MvLyric *l = &LYRIC_LINES[line];
    int found = -1;
    for (int i = 0; i < l->n_words; i++) {
        const MvWord *w = &LYRIC_WORDS[l->first_word + i];
        const char *s = w->text;
        size_t n = strlen(s);
        while (n && s[n - 1] == ' ') n--;
        if (n < 4) continue;
        int upper = 1, letters = 0;
        for (size_t k = 0; k < n; k++) {
            char c = s[k];
            if (c >= 'A' && c <= 'Z') { letters++; continue; }
            if (c == '-' || c == '\'' || c == '&') continue;
            upper = 0;
            break;
        }
        if (!upper || letters < 4) continue;
        if (w->start_ms <= t_ms) found = l->first_word + i;
    }
    if (found < 0) return -1;
    if (text) *text = LYRIC_WORDS[found].text;
    if (age) *age = (t_ms - LYRIC_WORDS[found].start_ms) / 1000.0;
    return found;
}

/* ------------------------------------------------------------------ drawing */

static int word_state(int t_ms, const MvWord *w)
{
    if (t_ms >= w->start_ms + w->dur_ms) return 2;   /* sung    */
    if (t_ms >= w->start_ms) return 1;               /* singing */
    return 0;                                        /* to come */
}

void lyrics_draw(Frame *f, int t_ms, Rgb base, Rgb accent, int show_progress)
{
    int line = lyric_display_line(t_ms);
    if (line < 0 || f->h < 3) return;
    const MvLyric *l = &LYRIC_LINES[line];
    Rgb dim = rgb_scale(base, 0.72);
    Rgb sung = rgb_mix(base, accent, 0.75);
    int y = f->h - 2;

    /* Previous phrase, faint, so the eye can follow the song. */
    if (line > 0 && f->h >= 6) {
        const char *prev = LYRIC_LINES[line - 1].text;
        int w = (int)strlen(prev);
        if (w <= f->w - 2) {
            fb_dim_rect(f, (f->w - w) / 2 - 1, f->h - 3, w + 2, 1, 0.35);
            fb_text(f, (f->w - w) / 2, f->h - 3, prev, rgb_scale(base, 0.48));
        }
    }

    /* Current phrase, word by word. */
    int total = (int)strlen(l->text);
    int x = (f->w - total) / 2;
    if (x < 1) x = 1;
    fb_dim_rect(f, x - 1, y, total + 2, 1, 0.28);
    for (int i = 0; i < l->n_words; i++) {
        const MvWord *w = &LYRIC_WORDS[l->first_word + i];
        int st = word_state(t_ms, w);
        Rgb col;
        if (st == 1) {
            double pulse = 0.62 + 0.38 * sin((double)t_ms * 0.008);
            col = rgb_mix(sung, RGB(255, 255, 255), pulse * 0.8);
        } else if (st == 2) {
            col = sung;
        } else {
            col = dim;
        }
        fb_text(f, x, y, w->text, col);
        x += (int)strlen(w->text);
    }
    if (l->n_words == 0) fb_text(f, x, y, l->text, dim);

    if (!show_progress) return;
    fb_dim_rect(f, 0, f->h - 1, f->w, 1, 0.45);

    /* Bottom strip: elapsed / duration and a progress bar for this phrase. */
    int end_ms = l->start_ms + 1;
    for (int i = 0; i < l->n_words; i++) {
        const MvWord *w = &LYRIC_WORDS[l->first_word + i];
        if (w->start_ms + w->dur_ms > end_ms) end_ms = w->start_ms + w->dur_ms;
    }
    double p = (double)(t_ms - l->start_ms) / (double)(end_ms - l->start_ms);
    if (p < 0) p = 0;
    if (p > 1) p = 1;

    char stamp[32];
    int secs = t_ms / 1000;
    snprintf(stamp, sizeof stamp, "%d:%02d", secs / 60, secs % 60);
    fb_text(f, 1, f->h - 1, stamp, rgb_scale(base, 0.65));

    int left = 8, right = f->w - 8;
    int barw = right - left - 2;
    if (barw > 70) barw = 70;
    if (barw > 4) {
        char bar[128];
        if (barw > (int)sizeof bar - 1) barw = (int)sizeof bar - 1;
        int filled = (int)lround(p * (barw - 2));
        bar[0] = '[';
        for (int i = 0; i < barw - 2; i++) bar[1 + i] = i < filled ? '=' : '-';
        bar[barw - 1] = ']';
        bar[barw] = 0;
        fb_text(f, left, f->h - 1, bar, rgb_scale(accent, 0.9));
    }
    int total_secs = LYRIC_TOTAL_MS / 1000;
    snprintf(stamp, sizeof stamp, "%d:%02d", total_secs / 60, total_secs % 60);
    fb_text(f, f->w - (int)strlen(stamp) - 1, f->h - 1, stamp, rgb_scale(base, 0.65));
}

void lyrics_draw_keywords(Frame *f, int t_ms, Rgb accent, Rgb base)
{
    const char *kw = NULL;
    double age = 0;
    if (lyric_keyword_at(t_ms, &kw, &age) < 0 || !kw) return;

    char text[48];
    size_t n = strlen(kw);
    while (n && kw[n - 1] == ' ') n--;
    if (n >= sizeof text) n = sizeof text - 1;
    memcpy(text, kw, n);
    text[n] = 0;

    if (age < 0 || age > 1.6) return;

    /* Snap in, then decay: brightness flash + a quick left-to-right reveal. */
    double reveal = age < 0.22 ? age / 0.22 : 1.0;
    double fade = age < 1.05 ? 1.0 : 1.0 - (age - 1.05) / 0.55;
    if (fade < 0) fade = 0;
    double flash = age < 0.18 ? 1.0 + (1.0 - age / 0.18) * 0.9 : 1.0;
    Rgb col = rgb_mix(base, accent, 0.55);
    col = rgb_scale(col, fade * flash);
    Rgb hot = rgb_scale(RGB(255, 255, 255), fade * flash);

    int cy = (f->h - 4) / 2;
    if (cy < 2) cy = f->h / 2;
    int maxh = f->h / 3;
    if (maxh > 7) maxh = 7;
    if (maxh < 3) maxh = 3;

    /* Two-tone: a hot copy underneath, the palette colour on top, so the word
     * reads as glowing. */
    int w = fb_banner_width((int)n, maxh);
    int h2 = maxh;
    while (h2 > 3 && w > f->w - 4) {
        h2--;
        w = fb_banner_width((int)n, h2);
    }
    if (w > f->w - 2) return;
    fb_banner(f, f->w / 2, cy, text, h2, rgb_scale(hot, 0.35), reveal, '#', 1);
    fb_banner(f, f->w / 2, cy, text, h2, col, reveal, '#', 0);
}

void lyrics_draw_hud(Frame *f, int t_ms, Rgb dim, int show_hud)
{
    if (!show_hud) return;
    Rgb c = rgb_scale(dim, 0.5);
    fb_text(f, 1, 0, "world.execute (me);", c);
    char buf[48];
    int secs = t_ms / 1000;
    snprintf(buf, sizeof buf, "%d:%02d", secs / 60, secs % 60);
    fb_text(f, f->w - (int)strlen(buf) - 1, 0, buf, c);

    /* Four-on-the-floor beat tick, handy for judging the sync by eye. */
    double beat = (double)t_ms * LYRIC_BPM / 60000.0;
    int whole = (int)beat;
    int x0 = f->w / 2 - 8;
    fb_text(f, x0, 0, "beat", rgb_scale(dim, 0.35));
    for (int i = 0; i < 4; i++) {
        int on = ((whole + i) % 4) == 2;
        double ph = beat - floor(beat);
        int lit = on && ph < 0.35;
        fb_put(f, x0 + 6 + (i * 2), 0, lit ? '@' : '.', lit ? dim : rgb_scale(dim, 0.3));
    }
}