/* fx_love.c -- second half of the effect library: the object lyrics, the long
 * love section, the EXECUTION storm and the ending. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fx_impl.h"
#include "fx_util.h"
#include "sprites.h"

/* ================================================== 23. GENERIC SPRITE SHOW == */

static const char *NUTRI_ROWS[] = {
    "serving size .... 1 heart",
    "affection ....... 120%",
    "attention ....... 99%",
    "side effects .... love",
    "storage ......... forever",
};

void fx_sprite(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    const char *name = c->sc->sprite ? c->sc->sprite : "heart";
    int side = c->sc->intensity >= 1;
    double cx = side ? w * 0.34 : w / 2.0;
    double cy = (h - 3) / 2.0;
    double scale = fx_fit_scale(c, name, side ? 0.34 : 0.42, 0.55) *
                   mv_lerp(0.35, 1.0, fx_enter(c, 0.7)) * c->sc->p1;
    double bob = sin(c->t * 1.3) * c->sc->p2;
    double beat = fx_beat(c, 6.0);

    /* Spotlight rings behind the subject. */
    for (int i = 0; i < 3; i++) {
        double r = (4.0 + i * 3.0) * (1.0 + 0.08 * beat);
        fb_ellipse(f, cx, cy + bob, r * 2.0, r, i == 0 ? ':' : '.',
                   rgb_scale(c1, 0.20 + 0.12 * beat), 0, 0, 1);
    }
    fb_sprite_center(f, name, (int)cx, (int)(cy + bob), scale, rgb_scale(c1, 0.45),
                     rgb_mix(c2, RGB(255, 255, 255), 0.25), 0.8 + 0.4 * beat, 1.0);
    /* Orbiting sparkles. */
    for (int i = 0; i < 14; i++) {
        double a = c->t * 1.1 + i / 14.0 * 2.0 * M_PI;
        double rr = 0.9 + 0.25 * sin(c->t * 2.0 + i);
        int x = (int)lround(cx + cos(a) * 26.0 * rr * (w / 80.0));
        int y = (int)lround(cy + bob + sin(a) * 7.0 * rr);
        fb_put(f, x, y, i % 3 ? '*' : '.', rgb_scale(c2, 0.4 + 0.5 * fabs(sin(c->t + i))));
    }
    if (side) {
        int px = w * 0.60;
        int pw = w - px - 2;
        if (pw > 12) {
            fx_panel(f, px, h / 2 - 5, pw, 9, "SPECIMEN", rgb_scale(c1, 0.7), c2);
            int n = (int)(sizeof NUTRI_ROWS / sizeof NUTRI_ROWS[0]);
            double p = mv_smooth(mv_clamp01((c->prog - 0.25) / 0.5));
            int shown = (int)(p * n + 0.001);
            for (int i = 0; i < shown && i < n; i++) {
                char buf[64];
                snprintf(buf, sizeof buf, "%.46s", NUTRI_ROWS[i]);
                fb_text(f, px + 2, h / 2 - 3 + i, buf, rgb_scale(c2, 0.9 - i * 0.08));
            }
            int bw = pw - 4;
            for (int i = 0; i < bw; i++)
                fb_put(f, px + 2 + i, h / 2 + 4, (double)i / bw < p ? '#' : '-',
                       rgb_scale(c2, 0.6));
        }
    }
    /* A label under the subject. */
    char cap[64];
    snprintf(cap, sizeof cap, "%s", name);
    for (char *q = cap; *q; q++) *q = (char)((*q >= 'a' && *q <= 'z') ? *q - 32 : *q);
    fb_text_center(f, h - 4, cap, rgb_scale(c2, 0.65));
}

/* ================================================================= 24. CAT == */

void fx_cat(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    (void)c1;
    double cx = w / 2.0, cy = (h - 3) / 2.0;
    double purr = 0.5 + 0.5 * sin(c->t * 28.0);
    double scale = fx_fit_scale(c, "cat", 0.45, 0.50) * mv_lerp(0.5, 1.0, fx_enter(c, 0.6));
    /* Purr waves radiate from the cat, wobbling with the purr. */
    for (int i = 0; i < 5; i++) {
        double r = 3.0 + i * 3.2 + fmod(c->t * 3.0, 3.2);
        fb_ellipse(f, cx, cy + 1 + purr * 1.5, r, r * 0.5,
                   i % 2 ? '~' : '-', rgb_scale(c2, mv_clamp01(0.7 - i * 0.12) * (0.5 + purr * 0.5)),
                   0, 0, 1);
    }
    fb_sprite_center(f, "cat", (int)cx, (int)(cy + purr * 0.5), scale, rgb_scale(c2, 0.45),
                     RGB(255, 255, 255), 0.85, 1.0);
    /* Purr meter shaking with the purr. */
    int bw = (w > 60) ? 30 : w - 20;
    int x0 = (w - bw) / 2 - 6;
    if (x0 < 1) x0 = 1;
    fb_text(f, x0, h - 4, "PURR rrr", rgb_scale(c2, 0.9));
    for (int i = 0; i < bw; i++) {
        double v = 0.5 + 0.5 * sin(c->t * 12.0 + i * 0.6);
        fb_put(f, x0 + 9 + i, h - 4, v > 0.35 ? '*' : '.', rgb_scale(c2, 0.4 + v * 0.6));
    }
    char buf[40];
    snprintf(buf, sizeof buf, "enjoyment %3.0f%%", 60 + 40 * purr);
    fb_text(f, 3, h - 4, buf, rgb_scale(c2, 0.75));
}

/* ================================================================= 25. GOD == */

void fx_god(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 4) / 2.0;
    double open = mv_smooth(mv_clamp01(c->t / 1.2));
    (void)c1;
    /* Rays sweeping out of the eye. */
    for (int i = 0; i < 48; i++) {
        double a = i / 48.0 * 2.0 * M_PI + c->t * 0.25;
        double len = fmin(w * 0.5, h) * (0.5 + 0.5 * mv_rand01(c->sc->seed + (uint32_t)i * 7u)) * open;
        unsigned char ch = (i % 4 == 0) ? '/' : ((i % 4 == 1) ? '\\' : ((i % 4 == 2) ? '|' : '-'));
        fb_line(f, cx, cy, cx + cos(a) * len, cy + sin(a) * len * 0.5, ch,
                rgb_scale(c2, 0.18 + 0.3 * fabs(sin(c->t + i * 0.4))));
    }
    fb_circle(f, cx, cy, fmin(w * 0.24, h * 0.5) * open, ':',
              rgb_scale(c2, 0.35 + 0.3 * fx_beat(c, 4.0)), 0, 0, 1);
    fb_sprite_center(f, "god", (int)cx, (int)cy, fx_fit_scale(c, "god", 0.44, 0.52) * open,
                     rgb_scale(c2, 0.5), RGB(255, 255, 255), 0.9, 1.0);
    /* A small creature looking up, as proof of existence. */
    int gy = h - 6;
    fb_put(f, (int)cx, gy, 'o', rgb_scale(c2, 0.85));
    fb_put(f, (int)cx, gy + 1, '|', rgb_scale(c2, 0.7));
    fb_put(f, (int)cx - 1, gy + 2, '/', rgb_scale(c2, 0.7));
    fb_put(f, (int)cx + 1, gy + 2, '\\', rgb_scale(c2, 0.7));
    for (int i = 0; i < 3; i++) {
        double a = fmod(c->t * 0.8 + i * 0.33, 1.0);
        fb_put(f, (int)(cx + (i - 1) * 3), (int)mv_lerp(gy + 4, cy + 3, a), '.',
               rgb_scale(RGB(255, 255, 255), 1.0 - a));
    }
    char buf[40];
    snprintf(buf, sizeof buf, "EXISTENCE PROOF %3.0f%%", 50 + 50 * open);
    fb_text(f, 3, h - 4, buf, rgb_scale(c2, 0.8));
}

/* ============================================================== 26. GENDER == */

void fx_gender(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cy = (h - 3) / 2.0;
    double switchat = mv_smooth(mv_clamp01((c->t_ms - 90270) / 1400.0));  /* "To F to M" */
    double cx = w / 2.0;
    double leftx = cx - w * 0.18, rightx = cx + w * 0.18;
    double s = fx_fit_scale(c, "female", 0.20, 0.40);
    double hide = fabs(switchat - 0.5) * 2.0;              /* 0 at the middle */
    fb_sprite_center(f, "female", (int)leftx, (int)cy, s, rgb_scale(c1, 0.4),
                     rgb_mix(c1, RGB(255, 255, 255), 0.3), 0.9, 1.0 - hide);
    fb_sprite_center(f, "male", (int)rightx, (int)cy, s, rgb_scale(c2, 0.4),
                     rgb_mix(c2, RGB(255, 255, 255), 0.3), 0.9, hide);
    /* Particles crossing over while the switch happens. */
    for (int i = 0; i < 24; i++) {
        double a = mv_rand01(c->sc->seed + (uint32_t)i * 11u);
        double p = fmod(a + c->t * 0.5, 1.0);
        double x = mv_lerp(leftx, rightx, p);
        double y = cy + (mv_rand01(c->sc->seed + (uint32_t)i * 23u) - 0.5) * 8.0;
        int past = p > switchat;
        fb_put(f, (int)x, (int)y, past ? '>' : '<', rgb_scale(past ? c2 : c1, 0.7));
    }
    /* The lever between them. */
    int lx = (int)leftx + (int)((rightx - leftx) * switchat);
    fb_put(f, lx, (int)cy, 'O', RGB(255, 255, 255));
    for (int x = (int)leftx; x <= (int)rightx; x++) fb_put(f, x, (int)cy - 1, '-', rgb_scale(c1, 0.4));
    fb_text(f, (int)leftx - 2, (int)cy + 3, "F", rgb_scale(c1, 0.9));
    fb_text(f, (int)rightx + 1, (int)cy + 3, "M", rgb_scale(c2, 0.9));
    char buf[48];
    snprintf(buf, sizeof buf, "gender = %s", switchat > 0.95 ? "M" : (switchat < 0.05 ? "F" : "switching"));
    fb_text_center(f, h - 4, buf, rgb_scale(c2, 0.8));
}

/* =============================================================== 27. MORPH == */

void fx_morph(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    static const char *shop[] = { "eggplant", "tomato", "cat", "heart", "question",
                                  "clock", "plug", "spiral", "smile" };
    int n = (int)(sizeof shop / sizeof shop[0]);
    double rate = 3.0 + c->sc->p1;
    int idx = (int)(c->t * rate);
    double frac = fmod(c->t * rate, 1.0);
    const char *cur = shop[idx % n];
    const char *next = shop[(idx + 1) % n];
    double cx = w / 2.0 + sin(c->t * 3.1) * (w * 0.06);
    double cy = (h - 3) / 2.0 + cos(c->t * 2.7) * 1.5;
    double s = fx_fit_scale(c, cur, 0.30, 0.50);
    fb_sprite_center(f, cur, (int)cx, (int)cy, s, rgb_scale(c1, 0.4), RGB(255, 255, 255),
                     mv_clamp01(1.0 - frac * 0.8), 1.0);
    fb_sprite_center(f, next, (int)cx, (int)cy, s, rgb_scale(c2, 0.4), RGB(255, 255, 255),
                     mv_clamp01(frac * 0.8), 1.0);
    for (int i = 0; i < 30; i++) {
        int x = (int)(mv_rand01(c->sc->seed + (uint32_t)i * 13u + (uint32_t)idx) * w);
        int y = (int)(mv_rand01(c->sc->seed + (uint32_t)i * 29u + (uint32_t)idx) * (h - 3));
        fb_put(f, x, y, mv_char_for(mv_rand01((uint32_t)(i * 7 + idx))), rgb_scale(c2, 0.5));
    }
    fb_text_center(f, h - 4, "RANDOMIZE", rgb_scale(c2, 0.7));
}

/* =============================================================== 28. CLOCK == */

void fx_clock(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 4) / 2.0;
    double R = fmin(w * 0.18, h * 0.36);
    double hours = c->t * 1.6;                       /* sweeps AM -> PM */
    double daynight = mv_smooth(mv_clamp01(c->t / c->dur));
    (void)c1;
    /* Morning warms into night. */
    fb_tint(f, rgb_mix(RGB(255, 190, 120), RGB(40, 60, 140), daynight), 0.25);
    fb_circle(f, cx, cy, R, '=', rgb_scale(c2, 0.9), 0, 0, 1);
    for (int i = 0; i < 12; i++) {
        double a = i / 12.0 * 2.0 * M_PI - M_PI / 2.0;
        int x = (int)lround(cx + cos(a) * (R - 2)), y = (int)lround(cy + sin(a) * (R - 2) * 0.5);
        fb_put(f, x, y, (i % 3 == 0) ? '+' : '.', rgb_scale(c2, i % 3 == 0 ? 0.95 : 0.6));
    }
    double ha = hours / 12.0 * 2.0 * M_PI - M_PI / 2.0;
    double ma = fmod(hours * 12.0, 12.0) / 12.0 * 2.0 * M_PI - M_PI / 2.0;
    fb_line(f, cx, cy, cx + cos(ha) * R * 0.55, cy + sin(ha) * R * 0.28, '#', RGB(255, 255, 255));
    fb_line(f, cx, cy, cx + cos(ma) * R * 0.85, cy + sin(ma) * R * 0.42, '=', rgb_scale(c2, 0.95));
    fb_put(f, (int)cx, (int)cy, '@', RGB(255, 255, 255));
    char buf[48];
    int hr = (int)(hours) % 24;
    snprintf(buf, sizeof buf, "%02d:%02d %s", hr % 12 == 0 ? 12 : hr % 12,
             (int)(c->t * 60) % 60, hr < 12 ? "AM" : "PM");
    fb_text_center(f, h - 4, buf, daynight > 0.5 ? rgb_scale(c2, 0.9) : RGB(255, 210, 140));
    for (int i = 0; i < 20; i++) {
        double a = mv_rand01(c->sc->seed + (uint32_t)i * 7u) * 2.0 * M_PI;
        double rr = R * (1.3 + 0.6 * mv_rand01(c->sc->seed + (uint32_t)i * 11u));
        fb_put(f, (int)(cx + cos(a) * rr), (int)(cy + sin(a) * rr * 0.5),
               daynight > 0.5 ? '.' : '*', rgb_scale(c2, 0.3 + 0.5 * fabs(sin(c->t + i))));
    }
}

/* =============================================================== 29. CHAIN == */

void fx_chain(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 4) / 2.0;
    double role = mv_smooth(mv_clamp01((c->t_ms - 97710) / 1400.0));   /* "To S to M" */
    double s = fx_fit_scale(c, "chain", 0.50, 0.40);
    fb_sprite_center(f, "chain", (int)cx, (int)cy, s, rgb_scale(c1, 0.45),
                     rgb_mix(c1, RGB(255, 255, 255), 0.25), 0.85, 1.0);
    /* One link tightens as the role slider moves. */
    int tw = (int)(w * 0.35);
    int x0 = (int)(cx - tw / 2);
    for (int i = 0; i < tw; i++) {
        double p = (double)i / tw;
        int on = p < role;
        fb_put(f, x0 + i, cy + (int)(s * 4.0) + 4, on ? '=' : '-',
               on ? rgb_scale(c2, 0.9) : rgb_scale(c1, 0.35));
    }
    fb_put(f, x0 + (int)(tw * role), cy + (int)(s * 4.0) + 4, 'O', RGB(255, 255, 255));
    fb_text(f, x0 - 3, cy + (int)(s * 4.0) + 4, "S", rgb_scale(c2, 0.9));
    fb_text(f, x0 + tw + 2, cy + (int)(s * 4.0) + 4, "M", rgb_scale(c2, 0.9));
    for (int i = 0; i < 18; i++) {
        double a = c->t * 1.4 + i / 18.0 * 2.0 * M_PI;
        double rr = 0.5 + 0.5 * mv_rand01(c->sc->seed + (uint32_t)i * 5u);
        fb_put(f, (int)(cx + cos(a) * (w * 0.3) * rr), (int)(cy + sin(a) * (h * 0.4) * rr),
               '.', rgb_scale(c2, 0.3 + 0.5 * fabs(sin(c->t * 2 + i))));
    }
    char buf[48];
    snprintf(buf, sizeof buf, "role = %s", role > 0.95 ? "M" : (role < 0.05 ? "S" : "switching"));
    fb_text_center(f, h - 4, buf, rgb_scale(c2, 0.8));
}

/* ============================================================== 30. TRANCE == */

void fx_trance(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 3) / 2.0;
    double speed = 1.0 + c->prog * 2.5;
    double hue = fmod(c->t * 0.08, 1.0);
    Rgb col = rgb_mix(c2, rgb_hsv(hue, 0.7, 0.9), 0.6);
    (void)c1;
    for (int arm = 0; arm < 2; arm++) {
        for (double r = 1.0; r < fmin(w * 0.55, h); r += 0.5) {
            double a = r * 0.22 + c->t * speed + arm * M_PI;
            double k = mv_clamp01(1.0 - r / fmin(w * 0.55, h));
            fb_put(f, (int)lround(cx + cos(a) * r), (int)lround(cy + sin(a) * r * 0.5),
                   k > 0.55 ? '#' : (k > 0.25 ? '+' : '.'), rgb_scale(col, 0.2 + k * 0.9));
        }
    }
    for (int i = 0; i < 4; i++) {
        double ph = fmod(c->t * 0.35 + i / 4.0, 1.0);
        fb_ellipse(f, cx, cy, ph * w * 0.55, ph * h * 0.55, '-',
                   rgb_scale(col, (1.0 - ph) * 0.6), 0, 0, 1);
    }
    fb_put(f, (int)cx, (int)cy, '@', RGB(255, 255, 255));
    char buf[48];
    snprintf(buf, sizeof buf, "TRANCE DEPTH %3.0f%%", c->prog * 100.0);
    fb_text(f, 3, h - 4, buf, rgb_scale(col, 0.8));
    /* Slow zoom wobble sells the hypnotic pull. */
    fb_shift(f, (int)lround(sin(c->t * 0.9) * 1.2), 0);
}

/* ============================================================ 31. VIBRATE == */

void fx_vibrate(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    (void)c1;
    int strings = 7;
    double amp = 2.0 + 4.0 * fx_beat(c, 4.0);
    double pick = fmod(c->t * 0.45, 1.0) * (w - 8) + 4;
    for (int i = 0; i < strings; i++) {
        double y0 = 2.0 + i * ((h - 8.0) / (strings - 1));
        double prevy = y0;
        for (int x = 4; x < w - 4; x++) {
            double d = (x - pick) / 24.0;
            double env = exp(-fabs(d) * 0.6) * amp * sin(c->t * 22.0 - fabs(d) * 3.0);
            double y = y0 + env;
            if (x > 4) fb_line(f, x - 1, prevy, x, y, '-', rgb_scale(c2, 0.35 + 0.35 * i / strings));
            prevy = y;
        }
    }
    fb_put(f, (int)pick, (int)(2.0 + 3 * ((h - 8.0) / (strings - 1))), '@', RGB(255, 255, 255));
    /* Waveform strip at the bottom. */
    int yy = h - 4;
    fb_line(f, 4, yy, w - 5, yy, ':', rgb_scale(c2, 0.3));
    for (int x = 4; x < w - 5; x++) {
        double v = 0.0;
        for (int k = 0; k < 4; k++)
            v += sin((x + c->t * 60.0) * (0.15 + k * 0.07)) / (k + 1);
        fb_put(f, x, (int)lround(yy + v * 2.0 * fx_beat(c, 3.0)), '=', rgb_scale(c2, 0.75));
    }
    if (c->t_ms > 106290) {
        double hot = mv_smooth(mv_clamp01((c->t_ms - 106290) / 500.0));
        fb_text_center(f, 1, "VIBRATIONS", rgb_scale(c2, hot));
    }
}

/* =========================================================== 32. COMPLETE == */

void fx_complete(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 4) / 2.0;
    double R = fmin(w * 0.20, h * 0.42);
    double p = mv_smooth(mv_clamp01(c->prog / 0.78));
    (void)c1;
    fb_circle(f, cx, cy, R, ':', rgb_scale(c2, 0.2), 0, 0, 1);
    fb_circle(f, cx, cy, R, '=', rgb_scale(c2, 0.95), 0, -0.25, -0.25 + p);
    fb_put(f, (int)(cx + cos((-0.25 + p) * 2.0 * M_PI) * R),
           (int)(cy + sin((-0.25 + p) * 2.0 * M_PI) * R * 0.5), '@', RGB(255, 255, 255));
    char buf[32];
    snprintf(buf, sizeof buf, "%3.0f%%", p * 100.0);
    fb_text(f, (int)cx - 2, (int)cy, buf, RGB(255, 255, 255));
    /* Checklist that ticks itself off. */
    static const char *TODO[] = { "find you", "hold on", "be complete" };
    for (int i = 0; i < 3; i++) {
        double at = 0.2 + i * 0.22;
        int done = c->prog > at;
        char line[40];
        snprintf(line, sizeof line, "[%c] %s", done ? 'x' : ' ', TODO[i]);
        fb_text(f, 3, 2 + i, line, rgb_scale(done ? c2 : c2, done ? 0.95 : 0.35));
    }
    if (p >= 1.0) {
        double flash = mv_clamp01(1.0 - (c->prog - 0.80) / 0.2);
        fb_text_center(f, 1, "COMPLETION", rgb_mix(c2, RGB(255, 255, 255), flash));
        for (int i = 0; i < 30; i++) {
            double a = mv_rand01(c->sc->seed + (uint32_t)i * 7u) * 2.0 * M_PI;
            double rr = R * (1.0 + 0.8 * mv_rand01(c->sc->seed + (uint32_t)i * 11u));
            fb_put(f, (int)(cx + cos(a) * rr), (int)(cy + sin(a) * rr * 0.5), '*',
                   rgb_scale(RGB(255, 255, 255), flash * 0.9));
        }
    }
}

/* =========================================================== 33. DISSOLVE == */

void fx_dissolve(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 4) / 2.0;
    /* Each "You have left" takes another piece of the figure away. */
    int lefts = 0;
    for (int i = 70; i <= 75; i++)
        if (c->t_ms >= LYRIC_LINES[i].start_ms) lefts++;
    double gone = mv_clamp01((double)lefts / 6.0);
    double s = fx_fit_scale(c, "heart", 0.30, 0.55);
    const MvSprite *sp = mv_sprite("heart");
    if (sp) {
        int tw = (int)lround(sp->w * s), th = (int)lround(sp->h * s);
        int x0 = (int)cx - tw / 2, y0 = (int)cy - th / 2;
        for (int r = 0; r < th; r++) {
            const char *row = sp->rows[fx_clampi((int)((double)r * sp->h / th), 0, sp->h - 1)];
            for (int col = 0; col < tw; col++) {
                char g = row[fx_clampi((int)((double)col * sp->w / tw), 0, sp->w - 1)];
                if (g == ' ') continue;
                int x = x0 + col, y = y0 + r;
                uint32_t hv = mv_hash((uint32_t)(col * 131 + r * 977) ^ (uint32_t)c->sc->seed);
                double thresh = mv_rand01(hv);
                double level = gone * (0.4 + 0.9 * (double)col / tw) + 0.05 * sin(c->t + col);
                if (thresh < level) {
                    /* Drifting ember of a cell that has let go. */
                    double age = (level - thresh) * 3.0;
                    double dy = -age * 2.2, dx = (mv_rand01(hv ^ 7u) - 0.5) * age * 4.0;
                    fb_put(f, x + (int)dx, y + (int)dy,
                           age > 1.2 ? '.' : '+',
                           rgb_scale(c2, mv_clamp01(1.0 - age / 2.5) * 0.7));
                    continue;
                }
                double k = 1.0 - gone * 0.5;
                fb_put(f, x, y, (unsigned char)g, rgb_scale(rgb_mix(c1, c2, 0.6), k));
            }
        }
    }
    char buf[48];
    snprintf(buf, sizeof buf, "you have left x%d", lefts);
    fb_text(f, 3, h - 4, buf, rgb_scale(c1, 0.7));
    for (int i = 0; i < 20; i++) {
        double a = mv_rand01(c->sc->seed + (uint32_t)i * 13u);
        double y = fmod(c->t * 0.6 + a * 5.0, (double)(h - 3));
        fb_put(f, (int)(a * w), (int)(h - 3 - y), '.', rgb_scale(c1, 0.25));
    }
}

/* =============================================================== 34. VOID == */

void fx_void(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 4) / 2.0;
    (void)c1;
    /* A single lonely signal in the dark, and one slow ring leaving it. */
    double ph = fmod(c->t * 0.5, 1.0);
    fb_ellipse(f, cx, cy, ph * w * 0.6, ph * h * 0.6, '.',
               rgb_scale(c2, (1.0 - ph) * 0.35), 0, 0, 1);
    fb_put(f, (int)cx, (int)cy, '@', rgb_scale(RGB(255, 255, 255), 0.6 + 0.4 * sin(c->t * 2.0)));
    fb_put(f, (int)cx - 1, (int)cy, '-', rgb_scale(c2, 0.25));
    fb_put(f, (int)cx + 1, (int)cy, '-', rgb_scale(c2, 0.25));
    for (int i = 0; i < 8; i++) {
        int x = (int)(mv_rand01(c->sc->seed + (uint32_t)i * 17u) * w);
        int y = (int)(mv_rand01(c->sc->seed + (uint32_t)i * 31u) * (h - 3));
        fb_put(f, x, y, '.', rgb_scale(RGB(170, 200, 255), 0.18));
    }
    fb_text(f, 3, h - 4, "signal: 1", rgb_scale(c2, 0.45));
    fb_text(f, w - 16, h - 4, "others: 0", rgb_scale(c2, 0.3));
}

/* ============================================================ 35. SHATTER == */

void fx_shatter(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 4) / 2.0;
    double burst = mv_smooth(mv_clamp01((c->t - c->dur * 0.18) / 0.7));
    double s = fx_fit_scale(c, "heart", 0.26, 0.50);
    const MvSprite *sp = mv_sprite("heart");
    double shock = mv_clamp01((c->t - c->dur * 0.18) / 1.2);
    if (sp) {
        int tw = (int)lround(sp->w * s), th = (int)lround(sp->h * s);
        int x0 = (int)cx - tw / 2, y0 = (int)cy - th / 2;
        for (int r = 0; r < th; r++) {
            const char *row = sp->rows[fx_clampi((int)((double)r * sp->h / th), 0, sp->h - 1)];
            for (int col = 0; col < tw; col++) {
                char g = row[fx_clampi((int)((double)col * sp->w / tw), 0, sp->w - 1)];
                if (g == ' ') continue;
                uint32_t hv = mv_hash((uint32_t)(col * 7919 + r * 104729) ^ (uint32_t)c->sc->seed);
                double a = mv_rand01(hv) * 2.0 * M_PI;
                double sp2 = 0.6 + 1.6 * mv_rand01(hv ^ 3u);
                double d = burst * sp2 * 6.0;
                int x = x0 + col + (int)lround(cos(a) * d);
                int y = y0 + r + (int)lround(sin(a) * d * 0.5);
                int rot = (int)(burst * 6.0);
                unsigned char ch = (unsigned char)g;
                if (((col + r + rot) % 3) == 0) ch = '.';
                fb_put(f, x, y, ch, rgb_scale(rgb_mix(c1, c2, 0.6),
                                             mv_clamp01(1.0 - burst * 0.8)));
                if (burst > 0.05) {
                    fb_put(f, x - 1, y, '\'', rgb_scale(c2, mv_clamp01(1.0 - burst * 0.9) * 0.5));
                }
            }
        }
    }
    if (shock > 0 && shock < 1) {
        double r = shock * fmin(w * 0.6, h * 1.4);
        fb_ellipse(f, cx, cy, r, r * 0.5, '*', rgb_scale(RGB(255, 255, 255), 1.0 - shock), 0, 0, 1);
    }
    double erase = mv_smooth(mv_clamp01((c->t_ms - 119280) / 2200.0));
    char buf[48];
    snprintf(buf, sizeof buf, "ERASING FRAGMENTS %3.0f%%", erase * 100.0);
    fb_text(f, 3, h - 4, buf, rgb_scale(c2, 0.8));
}

/* ============================================================= 36. LONELY == */

void fx_lonely(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 4) / 2.0;
    double breathe = 1.0 + 0.05 * sin(c->t * 1.1);
    double crack = mv_smooth(mv_clamp01((c->prog - 0.4) / 0.4));
    fb_sprite_center(f, "heart_broken", (int)cx, (int)cy, fx_fit_scale(c, "heart_broken", 0.26, 0.5) * breathe,
                     rgb_scale(c1, 0.4), rgb_mix(c2, RGB(255, 255, 255), 0.15), 0.7, 1.0);
    /* The crack widens until the two halves lean apart. */
    for (int i = -3; i <= 3; i++) {
        int y = (int)(cy - 3 + i * 1.2);
        int off = (int)(crack * (i < 0 ? 1 : -1));
        fb_put(f, (int)cx + off, y, '|', rgb_scale(c1, 0.3));
    }
    /* Falling petals. */
    for (int i = 0; i < 26; i++) {
        double a = mv_rand01(c->sc->seed + (uint32_t)i * 13u);
        double y = fmod(c->t * 1.1 + a * 8.0, (double)(h - 3));
        int x = (int)((a * w) + sin(c->t + i) * 2.0);
        fb_put(f, x, (int)(h - 3 - y), i % 4 == 0 ? '*' : '.', rgb_scale(c2, 0.35));
    }
    double hope = 0.15 + 0.1 * sin(c->t * 0.7);
    char buf[48];
    snprintf(buf, sizeof buf, "hope %3.0f%%", hope * 100.0);
    fb_text(f, 3, h - 4, buf, rgb_scale(c1, 0.6));
}

/* ============================================================== 37. ALERT == */

static const char *ERR_LINES[] = {
    "ILLEGAL ARGUMENT: faith",
    "SEGFAULT AT 0xDEADBEEF",
    "GOD NOT FOUND",
    "assert(you == mine) FAILED",
    "MEMORY VIOLATION IN heart.c",
    "unhandled exception: love",
    "STACK TRACE: me -> you -> null",
    "DIVISION BY ZERO: us",
};

void fx_alert(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double rage = mv_smooth(mv_clamp01((c->t - c->dur * 0.30) / 3.0));
    double base = 0.3 + 0.7 * rage;
    /* Warning triangles tiled in the background. */
    for (int gy = 1; gy < h - 3; gy += 5) {
        for (int gx = 2; gx < w - 8; gx += 12) {
            double blink = mv_pulse(c->t * (0.6 + rage) + (gx + gy) * 0.05);
            double k = (0.25 + 0.75 * blink) * base;
            fb_sprite(f, "warning", gx, gy, rgb_scale(c1, 0.5), c2, k, 1.0);
        }
    }
    /* Scrolling error log. */
    int n = (int)(sizeof ERR_LINES / sizeof ERR_LINES[0]);
    for (int i = 0; i < n; i++) {
        double y = fmod((double)i * 2.0 - c->t * (1.0 + rage * 3.0), (double)(h - 3));
        if (y < 0) y += h - 3;
        int yy = (int)y + 1;
        if (yy < 1 || yy >= h - 3) continue;
        Rgb col = rgb_scale(c2, 0.4 + 0.6 * fabs(sin(c->t * 2.0 + i)));
        int ex = (i % 2) ? w / 2 : 3;
        int el = (int)strlen(ERR_LINES[i]);
        fb_dim_rect(f, ex - 1, yy, el + 2, 1, 0.28);
        fb_text(f, ex, yy, ERR_LINES[i], col);
    }
    /* Siren bar and an alarm counter. */
    int bw = w - 8;
    for (int i = 0; i < bw; i++) {
        double v = 0.5 + 0.5 * sin(c->t * 4.0 + i * 0.2);
        fb_put(f, 4 + i, h - 4, v > 0.5 ? '#' : '=', rgb_scale(c1, 0.4 + v * 0.6));
    }
    if (rage > 0.7) {
        fb_text_center(f, 1, "!!! ILLEGAL ARGUMENTS !!!", RGB(255, 80, 60));
        fb_shift(f, (int)lround(sin(c->t * 30.0) * rage * 1.5), 0);
        fb_glitch(f, (int)(c->t * 10.0), rage * 0.6, c2);
    }
    fb_text(f, 2, h - 4, "SYSTEM: FAITH MODULE FAILING", rgb_scale(c1, 0.9));
}

/* ============================================================ 38. BUILDUP == */

void fx_buildup(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double p = c->prog;
    double surge = pow(p, 2.2);
    /* Converging sparks: everything falls into the centre. */
    for (int i = 0; i < 90; i++) {
        double a = mv_rand01(c->sc->seed + (uint32_t)i * 13u) * 2.0 * M_PI;
        double rr = (1.0 - fmod(p * 2.0 + mv_rand01(c->sc->seed + (uint32_t)i * 29u), 1.0)) *
                    fmin(w * 0.6, h * 1.1);
        fb_put(f, (int)(w / 2.0 + cos(a) * rr), (int)((h - 3) / 2.0 + sin(a) * rr * 0.5),
               i % 5 ? '.' : '+', rgb_scale(c2, 0.35 + surge * 0.65));
    }
    /* Overload meter. */
    int bw = w - 20;
    if (bw > 60) bw = 60;
    int x0 = (w - bw) / 2;
    for (int i = 0; i < bw; i++) {
        double frac = (double)i / bw;
        int on = frac < p;
        Rgb col = rgb_mix(c2, c1, frac);
        if (!on) col = rgb_scale(col, 0.15);
        fb_put(f, x0 + i, h - 4, on ? '#' : '-', col);
    }
    char buf[48];
    snprintf(buf, sizeof buf, "OVERLOAD %3.0f%%", p * 100.0);
    fb_text_center(f, h - 6, buf, rgb_scale(c2, 0.9));
    /* Every beat, a bigger ring and a shake. */
    for (int k = 0; k < 4; k++) {
        double age = c->phase + k;
        double r = age * fmin(w, h * 2.0) * (0.08 + surge * 0.10);
        fb_ellipse(f, w / 2.0, (h - 3) / 2.0, r, r * 0.5, '=',
                   rgb_scale(c2, mv_clamp01(1.0 - age / 4.0) * (0.4 + surge * 0.6)), 0, 0, 1);
    }
    /* The word that is coming, flickering faster and faster. */
    double flick = mv_rand01((uint32_t)(c->t * (2.0 + surge * 30.0)));
    if (flick > 0.45 - surge * 0.35) {
        double k = 0.15 + surge * 0.55;
        fb_banner_fit(f, w / 2, (h - 3) / 2, "EXECUTION", 5, w - 4,
                      rgb_scale(c2, k), 1.0, '#', 1);
    }
    if (surge > 0.55) {
        fb_shift(f, (int)lround(sin(c->t * 18.0) * surge * 2.0), 0);
        fb_glitch(f, (int)(c->t * 8.0), (surge - 0.55) * 1.2, c2);
    }
    if (c->prog > 0.93) {
        /* The white-out blooms out of the middle: a flat full-screen fill
         * reads as one bright smear, a radial blast reads as an explosion. */
        double white = (c->prog - 0.93) / 0.07;
        double ex = w / 2.0, ey = (h - 3) / 2.0;
        double mr = sqrt(ex * ex + ey * ey * 4.0);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                double dx = x - ex, dy = (y - ey) * 2.0;
                double r = sqrt(dx * dx + dy * dy) / (mr > 0 ? mr : 1.0);
                double k = mv_clamp01(white * 1.15 - r * 1.90);
                if (k < 0.07) continue;
                fb_put(f, x, y, k > 0.62 ? '#' : (k > 0.32 ? '*' : '.'),
                       rgb_scale(RGB(255, 255, 255), k));
            }
        }
    }
}

/* ========================================================= 39. EXECSTORM == */

/* Absolute start times of the twelve machine-gun "EXECUTION" lines. */
static int exec_hit_ms(int i)
{
    int idx = 86 + i;
    if (idx > 97) idx = 97;
    return LYRIC_LINES[idx].start_ms;
}

void fx_execstorm(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 3) / 2.0;
    int power = c->sc->intensity;

    /* Which hit are we on, and how old is it? */
    int hit = 0;
    for (int i = 0; i < 12; i++)
        if (c->t_ms >= exec_hit_ms(i)) hit = i;
    double age = (c->t_ms - exec_hit_ms(hit)) / 1000.0;
    double ramp = 0.45 + 0.55 * ((double)hit / 11.0) * (power / 3.0 + 0.34);

    /* Shockwaves from this and the previous two hits. */
    for (int k = 0; k < 3; k++) {
        int hi = hit - k;
        if (hi < 0) continue;
        double a = (c->t_ms - exec_hit_ms(hi)) / 1000.0;
        if (a < 0 || a > 1.3) continue;
        double r = a * fmin(w * 0.7, h * 1.6) * (0.8 + ramp * 0.4);
        fb_ellipse(f, cx, cy, r, r * 0.5, k == 0 ? '#' : '=',
                   rgb_scale(k == 0 ? RGB(255, 255, 255) : c2,
                             mv_clamp01(1.0 - a / 1.3) * (0.9 - k * 0.25)), 0, 0, 1);
    }
    /* Slam-in of the word itself: overshoot in brightness, then settle. */
    double slam = age < 0.16 ? 1.6 : 1.0 - (age - 0.16) * 0.35;
    if (slam < 0.25) slam = 0.25;
    int maxh = (f->h / 3 > 7) ? 7 : f->h / 3;
    fb_banner_fit(f, (int)cx, (int)(cy + 1), "EXECUTION", maxh, w - 2,
                  rgb_scale(RGB(255, 255, 255), mv_clamp01(slam)),
                  1.0, '#', 1);
    fb_banner_fit(f, (int)cx, (int)cy, "EXECUTION", maxh, w - 2,
                  rgb_scale(c1, mv_clamp01(slam) * (0.7 + 0.3 * ramp)), 1.0, '#', 0);
    /* Damage readout and a belt of hits. */
    for (int i = 0; i < 12; i++) {
        int on = i <= hit;
        fb_put(f, 3 + i * 2, h - 4, on ? '#' : '.', rgb_scale(on ? c2 : c1, on ? 0.95 : 0.25));
    }
    char buf[48];
    snprintf(buf, sizeof buf, "EXEC(%d/12)  damage %3.0f%%", hit + 1,
             mv_clamp01(ramp * (0.4 + (double)hit / 11.0)) * 100.0);
    fb_text(f, 3, h - 5, buf, rgb_scale(c2, 0.85));
    if (age < 0.2) {
        for (int y = 0; y < h - 3; y++)
            for (int x = 0; x < w; x++)
                if (((x * 7 + y * 13) % 5) == 0)
                    fb_put(f, x, y, '+', rgb_scale(RGB(255, 255, 255), 1.0 - age / 0.2));
    }
    fb_shift(f, (int)lround(sin(c->t * 47.0) * ramp * 2.0), 0);
    fb_glitch(f, (int)(c->t * 12.0), ramp * 0.35, c2);
}

/* ============================================================== 40. COUNT == */

void fx_count(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    static const char *words[] = { "EIN", "DOS", "TROIS", "NE", "FEM", "LIU" };
    int n = 0;
    for (int i = 0; i < 6; i++)
        if (c->t_ms >= LYRIC_LINES[98 + i].start_ms) n = i + 1;
    if (n < 1) n = 1;
    double age = (c->t_ms - LYRIC_LINES[98 + n - 1].start_ms) / 1000.0;
    double hue = (double)(n - 1) / 6.0;
    Rgb col = rgb_mix(c2, rgb_hsv(hue, 0.85, 1.0), 0.75);
    (void)c1;
    /* Radial burst that fires on each count. */
    for (int i = 0; i < 60; i++) {
        double a = i / 60.0 * 2.0 * M_PI + n * 0.3;
        double len = fmin(w * 0.5, h) * mv_clamp01(1.0 - age * 0.7) *
                     (0.4 + 0.6 * mv_rand01(c->sc->seed + (uint32_t)i * 7u));
        double life = mv_clamp01(1.0 - age * 0.7);
        double ray = (i % 3 == 0) ? 1.0 : (i % 3 == 1 ? 0.62 : 0.38);
        fb_line(f, w / 2.0, (h - 3) / 2.0, w / 2.0 + cos(a) * len,
                (h - 3) / 2.0 + sin(a) * len * 0.5, i % 4 ? '-' : '+',
                rgb_scale(col, life * 0.6 * ray));
    }
    char num[16];
    snprintf(num, sizeof num, "%d", n);
    double slam = age < 0.14 ? 1.7 : 1.0 - (age - 0.14) * 0.4;
    fb_banner_fit(f, w / 2, (h - 5) / 2, num, (h / 3 > 8) ? 8 : h / 3, w - 4,
                  rgb_scale(col, mv_clamp01(slam)), 1.0, '#', 1);
    fb_text_center(f, h - 4, words[n - 1], rgb_scale(col, 0.9));
    fb_shift(f, (int)lround(sin(c->t * 55.0) * 2.0), 0);
}

/* ============================================================ 41. FORMULA == */

void fx_formula(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    int mode = (int)c->sc->p1;
    (void)c1;
    /* Floating hearts always present. */
    for (int i = 0; i < 12; i++) {
        double a = mv_rand01(c->sc->seed + (uint32_t)i * 13u);
        double y = fmod(c->t * 0.8 + a * 6.0, (double)(h - 3));
        fb_put(f, (int)(a * w), (int)(h - 4 - y), i % 3 == 0 ? '<' : '>',
               rgb_scale(c2, 0.35));
    }
    if (mode == 0) {
        static const char *F[] = {
            "LOVE = integral(you + me) dt",
            "d(heart)/dt = you",
            "lim  love(x)  =  inf",
            "x->you",
            "if (you.alive) me.happy = true;",
            "return LOVE;",
        };
        int n = (int)(sizeof F / sizeof F[0]);
        for (int i = 0; i < n; i++) {
            int y = 2 + i * 3;
            if (y >= h - 4) break;
            double k = mv_smooth(mv_clamp01((c->prog - i * 0.11) / 0.18));
            fb_text(f, 4 + (i % 3) * 2, y, F[i], rgb_scale(c2, 0.25 + 0.75 * k));
        }
        fb_sprite_center(f, "heart", w - 12, h / 2, fx_fit_scale(c, "heart", 0.12, 0.24),
                         rgb_scale(c2, 0.4), RGB(255, 255, 255), 0.9, 1.0);
    } else if (mode == 1) {
        static const char *Q[] = { "Q: what is love?", "Q: do you love me?",
                                   "Q: how many?", "Q: forever?" };
        static const char *A[] = { "A: world.execute(you);", "A: 0xFFFFFFFF times",
                                   "A: 1 (only)", "A: while(true);" };
        int n = (int)(sizeof Q / sizeof Q[0]);
        for (int i = 0; i < n; i++) {
            int y = 2 + i * 4;
            if (y + 1 >= h - 4) break;
            double kq = mv_smooth(mv_clamp01((c->prog - i * 0.2) / 0.12));
            double ka = mv_smooth(mv_clamp01((c->prog - i * 0.2 - 0.09) / 0.12));
            fb_text(f, 3, y, Q[i], rgb_scale(RGB(255, 255, 255), 0.35 + 0.6 * kq));
            fb_text(f, 6, y + 1, A[i], rgb_scale(c2, 0.35 + 0.65 * ka));
        }
        for (int i = 0; i < 9; i++) {
            double a = mv_rand01(c->sc->seed + (uint32_t)i * 7u) * 2.0 * M_PI;
            fb_put(f, (int)(w * 0.8 + cos(a) * 12), (int)(h / 2 + sin(a) * 5), '?',
                   rgb_scale(c2, 0.3 + 0.5 * fabs(sin(c->t + i))));
        }
    } else {
        static const char *ALG[] = {
            "     LOVE = (A . B) / |A - B| + SUM heart_i",
            "          = d(you)/d(me) * INF",
            "          = 1 - e^(-t/tau)   as t -> forever",
        };
        fx_panel(f, 4, 2, w - 8, 10, "ALGEBRA", rgb_scale(c2, 0.6), c2);
        int total = 0;
        for (int i = 0; i < 3; i++) total += (int)strlen(ALG[i]);
        int typed = (int)(total * mv_smooth(mv_clamp01(c->prog / 0.75)));
        int acc = 0;
        for (int i = 0; i < 3; i++) {
            const char *s = ALG[i];
            int n = (int)strlen(s);
            int vis = fx_clampi(typed - acc, 0, n);
            if (vis > 0) {
                char tmp[80];
                snprintf(tmp, sizeof tmp, "%.*s", vis, s);
                fb_text(f, 6, 4 + i * 2, tmp, rgb_scale(RGB(255, 255, 255), 0.9));
            }
            acc += n;
            if (vis == n && ((int)(c->t * 3.0) % 2) == 0 && i == 2)
                fb_put(f, 6 + n, 4 + i * 2, '_', c2);
        }
        fb_sprite_center(f, "heart", w / 2, h - 6, fx_fit_scale(c, "heart", 0.10, 0.18),
                         rgb_scale(c2, 0.4), RGB(255, 255, 255), 0.9, 1.0);
    }
}

/* ========================================================= 42. HEART CAGE == */

void fx_heart_cage(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 4) / 2.0;
    double beat = fx_beat(c, 5.0);
    fb_sprite_center(f, "heart", (int)cx, (int)cy, fx_fit_scale(c, "heart", 0.24, 0.46) *
                         (1.0 + 0.06 * beat),
                     rgb_scale(c1, 0.45), rgb_mix(c2, RGB(255, 255, 255), 0.2),
                     0.8 + 0.3 * beat, 1.0);
    /* Bars in front, with the right-hand side left open. */
    for (int x = (int)(cx - w * 0.18); x <= (int)(cx + w * 0.06); x += 3) {
        for (int y = 2; y < h - 5; y++)
            fb_put(f, x, y, '|', rgb_scale(c2, 0.55 + 0.2 * mv_noise2(x, y, c->sc->seed)));
    }
    for (int x = (int)(cx - w * 0.18); x <= (int)(cx + w * 0.06); x++) {
        fb_put(f, x, 2, '=', rgb_scale(c2, 0.7));
        fb_put(f, x, h - 6, '=', rgb_scale(c2, 0.7));
    }
    /* You are free: something small flies out of the open side. */
    double away = mv_clamp01((c->prog - 0.15) / 0.7);
    int bx = (int)mv_lerp(cx + w * 0.08, w - 4, away);
    int by = (int)(cy - 3 - away * 3.0);
    fb_put(f, bx, by, '>', rgb_scale(RGB(255, 255, 255), 0.9));
    fb_put(f, bx - 1, by + (int)(sin(c->t * 8) * 1.5), '\'', rgb_scale(c2, 0.6));
    fb_text(f, 3, h - 4, "you: free      me: trapped", rgb_scale(c2, 0.8));
    if (c->t_ms > 191430) {
        double k = mv_smooth(mv_clamp01((c->t_ms - 191430) / 600.0));
        fb_text_center(f, 1, "in LOVE", rgb_scale(RGB(255, 255, 255), k * 0.9));
    }
}

/* ============================================================= 43. OUTRO == */

void fx_outro(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double p = c->prog;
    (void)c1;
    /* The world winds down: rain slows, grid dims, heartbeat traces. */
    int rain_x = w - 24;
    for (int x = rain_x; x < w - 1; x++) {
        double speed = (1.0 - p * 0.85) * (1.0 + mv_rand01(c->sc->seed + (uint32_t)x * 13u));
        int head = (int)fmod(c->t * speed + mv_rand01(c->sc->seed + (uint32_t)x) * 30.0, (double)h);
        for (int k = 0; k < 5; k++) {
            int y = head - k;
            if (y < 0 || y >= h - 3) continue;
            fb_put(f, x, y, "0123456789ABCDEF"[mv_hash((uint32_t)(x * 131 + y * 977)) % 16],
                   rgb_scale(c2, (1.0 - (double)k / 6.0) * (0.7 - p * 0.5)));
        }
    }
    /* ECG trace across the screen. */
    int base = h - 8;
    fb_line(f, 2, base, w - 4, base, ':', rgb_scale(c2, 0.25));
    double flat = mv_smooth(mv_clamp01((p - 0.72) / 0.15));
    double prevy = base;
    for (int x = 2; x < w - 4; x++) {
        double ph = fmod((x + c->t * 14.0) * 0.10, 3.0);
        double v = 0.0;
        if (ph < 0.18) v = -4.0 * (ph / 0.18);
        else if (ph < 0.30) v = -4.0 + 12.0 * ((ph - 0.18) / 0.12);
        else if (ph < 0.42) v = 8.0 - 9.0 * ((ph - 0.30) / 0.12);
        else v = 0;
        v *= (1.0 - flat);
        if (x > 2 && x < w - 5) fb_line(f, x - 1, prevy, x, base + v, '=',
                                        rgb_scale(RGB(120, 255, 160), 0.8 - p * 0.4));
        prevy = base + v;
    }
    if (flat > 0.9) {
        double k = mv_smooth(mv_clamp01((p - 0.78) / 0.1));
        fb_text_center(f, 2, "LOVE = 0", rgb_scale(RGB(255, 255, 255), k * 0.85));
        fb_text_center(f, h - 4, "simulation halted", rgb_scale(c2, k * 0.7));
    }
    for (int i = 0; i < 14; i++) {
        int x = (int)(mv_rand01(c->sc->seed + (uint32_t)i * 7u) * w);
        int y = (int)(mv_rand01(c->sc->seed + (uint32_t)i * 11u) * (h - 3));
        double tw = 0.5 + 0.5 * sin(c->t * (1.0 - p) + i);
        fb_put(f, x, y, '.', rgb_scale(RGB(200, 220, 255), tw * (0.5 - p * 0.45)));
    }
    fb_shade(f, 1.0 - p * 0.55);
}

/* =============================================================== 44. END == */

void fx_end(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    (void)c1;
    const char *word = "EXECUTION";
    int n = (int)strlen(word);
    double typed = mv_clamp01((c->t - 0.3) / 1.6) * n;
    char buf[32];
    int vis = fx_clampi((int)typed, 0, n);
    snprintf(buf, sizeof buf, "%.*s", vis, word);
    fb_text_center(f, h / 2 - 1, buf, rgb_scale(c2, 0.95));
    if (((int)(c->t * 3.0) % 2) == 0 && vis < n)
        fb_put(f, (w + vis) / 2, h / 2 - 1, '#', RGB(255, 255, 255));
    if (c->t > 2.2) {
        double k = mv_smooth(mv_clamp01((c->t - 2.2) / 0.8));
        fb_banner_fit(f, w / 2, h / 2 - 3, "EXECUTION", 5, w - 4, rgb_scale(c2, k * 0.35), 1.0, '#', 0);
        fb_text_center(f, h / 2 + 1, "world.execute (me);", rgb_scale(RGB(255, 255, 255), k * 0.9));
        fb_text_center(f, h / 2 + 3, "Mili", rgb_scale(c2, k * 0.6));
    }
    if (c->t > 4.6) {
        double k = mv_smooth(mv_clamp01((c->t - 4.6) / 2.0));
        fb_text_center(f, h - 4, "ASCII MV - rendered live in your terminal", rgb_scale(c2, k * 0.45));
        fb_shade(f, 1.0 - k * 0.75);
    }
    if (c->t > 7.0) {
        double k = mv_smooth(mv_clamp01((c->t - 7.0) / 2.0));
        fb_shade(f, 1.0 - k);
        if (((int)(c->t * 1.5) % 2) == 0)
            fb_put(f, 3, 1, '>', rgb_scale(c2, 0.4 * (1.0 - k)));
    }
}