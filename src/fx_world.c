/* fx_world.c -- first half of the visual effect library: the boot sequence, the
 * maths verse, the AC/DC and travel section, and the object lyrics.
 *
 * Every effect draws with a stateless function of the Ctx (time, palette, seed),
 * so scenes can be rendered at any moment for transitions and for the offline
 * PNG exporter.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fx_impl.h"
#include "fx_util.h"
#include "sprites.h"

/* =========================================================== 1. POWER LINE == */

void fx_powerline(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;

    /* CRT switch-on: a bright band opens up vertically out of a single line,
     * then dissolves into the dark so the power line itself can read. */
    double on = mv_ease_out(c->t / 0.30);
    double fade = mv_clamp01(1.0 - (c->t - 0.42) / 0.45);
    int cy = h / 2;
    int half = (int)(on * 3.0);
    for (int y = cy - half; y <= cy + half && fade > 0; y++) {
        if (y < 0 || y >= h) continue;
        double d = 1.0 - fabs((double)(y - cy)) / (double)(half + 1);
        for (int x = 0; x < w; x++) {
            if (((x + y) % 5) == 0) continue;
            fb_put(f, x, y, d > 0.55 ? '=' : '-',
                   rgb_scale(c2, (0.12 + d * 0.9) * fade));
        }
    }
    if (on < 0.35) return;

    /* The power line, with a spark racing along it. */
    int ly = h * 2 / 3;
    double spark = 2.0 + c->prog * (double)(w - 5);
    for (int x = 2; x < w - 2; x++) {
        int lit = (double)x <= spark;
        Rgb col = lit ? rgb_mix(c1, c2, 0.35) : rgb_scale(c1, 0.30);
        fb_put(f, x, ly, (x % 5 == 0) ? '+' : '=', col);
    }
    for (int k = 0; k < 5; k++) {
        int x = (int)spark - k * 2;
        if (x < 2) break;
        fb_put(f, x, ly, k == 0 ? '@' : (k < 3 ? 'o' : '.'), rgb_scale(c2, 1.0 - k * 0.18));
    }
    for (int i = 0; i < 5; i++) {
        int px = 4 + i * (w - 10) / 4;
        for (int y = ly - 3; y <= ly; y++) fb_put(f, px, y, '|', rgb_scale(c1, 0.42));
        fb_put(f, px - 2, ly - 3, '+', rgb_scale(c1, 0.5));
        fb_put(f, px + 2, ly - 3, '+', rgb_scale(c1, 0.5));
        for (int x = px - 2; x <= px + 2; x++) fb_put(f, x, ly - 3, '-', rgb_scale(c1, 0.5));
    }
    /* Voltage readout + switch flipping to ON near the end. */
    char buf[48];
    double volts = 220.0 * mv_smooth(c->t / 1.2);
    snprintf(buf, sizeof buf, "AC %3.0fV", volts);
    fb_text(f, 3, ly + 2, buf, rgb_scale(c1, 0.8));
    int on_switch = c->prog > 0.62;
    fb_text(f, w - 12, ly + 2, on_switch ? "[  ON  ]" : "[ OFF  ]",
            on_switch ? c2 : rgb_scale(c1, 0.45));
    for (int i = 0; i < 6; i++) {
        double v = fmod(c->t * 2.0 + i * 0.17, 1.0);
        fb_put(f, 3 + i * 2, ly - 5, v > 0.5 ? '*' : '.', rgb_scale(c2, 0.35 + v * 0.4));
    }
}

/* ============================================================== 2. SHIELD == */

void fx_shield(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 3) / 2.0;
    double r = fmin(w * 0.22, h * 0.30);
    double in = fx_enter(c, 0.5);

    /* Dashed containment field. */
    for (int i = 0; i < 90; i++) {
        double a = i / 90.0 * 2.0 * M_PI + c->t * 0.35;
        double pulse = 1.0 + 0.03 * sin(c->t * 3.0 + i * 0.4);
        fb_put(f, (int)lround(cx + cos(a) * r * 1.5 * pulse),
               (int)lround(cy + sin(a) * r * 0.75 * pulse),
               (i % 9 == 0) ? '+' : (i % 3 == 0 ? ':' : '.'),
               rgb_scale(c1, 0.35 + 0.25 * mv_pulse(c->t * 0.8)));
    }
    /* Incoming attacks that stop dead on the barrier. */
    for (int i = 0; i < 14; i++) {
        double a = mv_rand01(c->sc->seed * 31u + i * 7u) * 2.0 * M_PI;
        double delay = mv_rand01(c->sc->seed * 17u + i * 13u) * 1.4;
        double age = c->t - delay;
        if (age < 0) continue;
        double dist = r * (1.9 - mv_clamp01(age / 0.55) * 0.9);
        double x = cx + cos(a) * dist * 1.4, y = cy + sin(a) * dist * 0.7;
        if (dist <= r * 1.45) {
            double life = 0.35 - (age - 0.55);
            if (life > 0) {
                fb_put(f, (int)lround(x), (int)lround(y), '*', rgb_scale(c2, life / 0.35 * 1.5));
                fb_put(f, (int)lround(x) + 1, (int)lround(y), '+', rgb_scale(c1, life / 0.35));
                fb_put(f, (int)lround(x), (int)lround(y) + 1, '+', rgb_scale(c1, life / 0.35));
            }
            continue;
        }
        fb_put(f, (int)lround(x), (int)lround(y), age < 0.2 ? 'o' : '.', rgb_scale(c2, 0.9));
    }
    /* The shield itself. */
    double s = fx_fit_scale(c, "shield", 0.30, 0.55) * (0.85 + 0.15 * in);
    double glow = 0.75 + 0.45 * mv_pulse(c->t * 1.3) + 0.5 * fx_beat(c, 7.0);
    fb_sprite_center(f, "shield", (int)cx, (int)cy, s, rgb_scale(c1, 0.45),
                     rgb_mix(c2, RGB(255, 255, 255), 0.35), mv_clampd(glow, 0, 1.6), in);
    fb_text_center(f, h - 4, in > 0.99 ? "PROTECTION: ACTIVE" : "PROTECTION: ...",
                   rgb_scale(c2, 0.6 + 0.4 * in));
}

/* ============================================================== 3. BLOCKS == */

typedef struct { int n, dx[4], dy[4]; } FxPiece;

static const FxPiece PIECES[7] = {
    { 4, { 0, 1, 2, 1 }, { 0, 0, 0, 1 } },   /* T  */
    { 4, { 0, 1, 0, 1 }, { 0, 0, 1, 1 } },   /* O  */
    { 4, { 0, 1, 2, 2 }, { 0, 0, 0, 1 } },   /* J  */
    { 4, { 0, 0, 1, 2 }, { 0, 1, 1, 1 } },   /* L  */
    { 4, { 0, 1, 1, 2 }, { 0, 0, 1, 1 } },   /* S  */
    { 4, { 0, 1, 2, 3 }, { 0, 0, 0, 0 } },   /* I  */
    { 4, { 0, 1, 1, 2 }, { 1, 1, 0, 0 } },   /* Z  */
};

void fx_blocks(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;

    /* Faint floor grid to catch the pieces. */
    int floor_y = h - 4;
    for (int x = 0; x < w; x += 2) fb_put(f, x, floor_y + 1, '.', rgb_scale(c1, 0.25));
    for (int i = 0; i < 7; i++)
        fb_put(f, 4 + i * 4, floor_y, '+', rgb_scale(c1, 0.3));

    int np = 11;
    int cx = w / 2;
    for (int i = 0; i < np; i++) {
        const FxPiece *p = &PIECES[i % 7];
        int tx = cx - 7 + (i % 6) * 3;
        int ty = floor_y - 1 - (i / 6) * 2;
        double t0 = 0.25 + (double)i * (c->dur * 0.55 / np);
        double age = c->t - t0;
        if (age < 0) continue;
        double fall = mv_clamp01(age / (0.55 + 0.05 * (i % 4)));
        double yy = -4 + (ty + 4) * (fall * fall);
        int py = (int)lround(yy);
        double flash = mv_clamp01(1.0 - (age - (0.55 + 0.05 * (i % 4))) / 0.22);
        Rgb col = rgb_mix(c1, c2, (double)(i % 5) / 4.0);
        col = rgb_scale(col, fall >= 1.0 ? (1.0 + flash * 1.4) : 0.9);
        for (int k = 0; k < p->n; k++) {
            int x = tx + p->dx[k], y = py + p->dy[k];
            if (y < 0) continue;
            fb_put(f, x, y, fall >= 1.0 ? '#' : '=', col);
        }
        /* Landing dust. */
        if (fall >= 1.0 && flash > 0) {
            for (int k = 0; k < 5; k++) {
                int dx = (int)(mv_rand01(c->sc->seed + i * 5 + k) * 7) - 3;
                fb_put(f, tx + 1 + dx, ty - 1 - (k % 2), k % 2 ? '\'' : '.',
                       rgb_scale(c2, flash));
            }
        }
        /* Ghost of where it will land. */
        if (fall < 1.0) {
            for (int k = 0; k < p->n; k++)
                fb_put(f, tx + p->dx[k], ty + p->dy[k], ':', rgb_scale(c1, 0.22));
        }
    }
    /* Once everything is stacked, outline the creation and make it hum. */
    double done = mv_clamp01((c->t - c->dur * 0.72) / 0.5);
    if (done > 0) {
        int pulse = (int)(fmod(c->t * 3.0, 1.0) * 4.0);
        for (int i = 0; i < np; i++) {
            const FxPiece *p = &PIECES[i % 7];
            int tx = cx - 7 + (i % 6) * 3, ty = floor_y - 1 - (i / 6) * 2;
            for (int k = 0; k < p->n; k++) {
                int x = tx + p->dx[k], y = ty + p->dy[k];
                if (((x + y + pulse) % 3) != 0) continue;
                fb_put(f, x, y, '@', rgb_mix(c2, RGB(255, 255, 255), 0.4 * done));
            }
        }
        fb_text_center(f, 2, "OBJECT CREATION", rgb_scale(c2, 0.7 * done));
    }
}

/* ========================================================== 4. DATASTREAM == */

void fx_datastream(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    int pw = w / 2 - 3;
    if (pw < 10) pw = w - 4;
    int ph = h - 6;
    if (ph < 6) ph = h - 3;

    /* Right half: hexadecimal rain. */
    int rain_x = (pw + 5 < w - 8) ? pw + 5 : w - 6;
    for (int x = rain_x; x < w - 1; x++) {
        double speed = 2.0 + mv_rand01(c->sc->seed + (uint32_t)x * 13u) * 3.0;
        int head = (int)(fmod(c->t * speed + mv_rand01(c->sc->seed + (uint32_t)x) * 40.0, (double)h));
        for (int k = 0; k < 8; k++) {
            int y = head - k;
            if (y < 1 || y >= h - 4) continue;
            uint32_t hv = mv_hash((uint32_t)(x * 131 + y * 977) ^ (uint32_t)c->sc->seed);
            const char *hex = "0123456789ABCDEF";
            char ch = hex[hv % 16];
            double b = k == 0 ? 1.0 : 1.0 - (double)k / 9.0;
            fb_put(f, x, y, ch, rgb_scale(c2, b * 0.85));
        }
    }

    fx_panel(f, 2, 1, pw, ph, "PARAMETERS", rgb_scale(c1, 0.7), c2);
    /* Fields fill in, one after another. */
    int rows = ph - 3;
    if (rows > 9) rows = 9;
    for (int i = 0; i < rows; i++) {
        double at = 0.06 + 0.55 * (double)i / (double)(rows > 1 ? rows - 1 : 1);
        if (c->prog < at) continue;
        int filled = 1;
        uint32_t hv = mv_hash((uint32_t)(i * 7919) ^ (uint32_t)c->sc->seed);
        char buf[64];
        snprintf(buf, sizeof buf, "param[%02d] = 0x%04X  %s", i,
                 (unsigned)(hv & 0xFFFFu), (hv & 1) ? "OK" : "");
        Rgb col = rgb_mix(c1, c2, (double)i / (double)(rows > 1 ? rows - 1 : 1));
        fb_text(f, 4, 3 + i, buf, rgb_scale(col, filled ? 0.95 : 0.5));
        if (filled && ((int)(c->t * 4.0) % 2) == 0)
            fb_put(f, 4 + (int)strlen(buf), 3 + i, '_', c2);
    }
    /* Progress bar that snaps to READY at INITIALIZATION. */
    int done = c->prog > 0.78;
    double p = done ? 1.0 : mv_clamp01(c->prog / 0.78);
    int bw = pw - 6;
    if (bw > 40) bw = 40;
    char bar[64];
    for (int i = 0; i < bw; i++) bar[i] = (double)i / bw < p ? '#' : '.';
    bar[bw] = 0;
    fb_text(f, 4, ph + 1, bar, done ? rgb_mix(c2, RGB(255, 255, 255), 0.5) : c1);
    char pct[32];
    snprintf(pct, sizeof pct, "%3.0f%%%s", p * 100.0, done ? " INITIALIZED" : "");
    fb_text(f, 4 + bw + 2, ph + 1, pct, done ? c2 : rgb_scale(c1, 0.7));
    if (done) {
        double flash = mv_clamp01(1.0 - (c->t - c->dur * 0.80) / 0.5);
        if (flash > 0) fb_text_center(f, 1, "INITIALIZATION COMPLETE", rgb_scale(c2, flash));
    }
}

/* =============================================================== 5. GLOBE == */

void fx_globe(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 3) / 2.0;
    double R = fmin(w * 0.30, h * 0.52);
    double spin = c->t * 0.55;
    double in = fx_enter(c, 0.9);

    /* Starfield behind the new world. */
    for (int i = 0; i < 60; i++) {
        int x = (int)(mv_rand01(c->sc->seed + i * 31u) * w);
        int y = (int)(mv_rand01(c->sc->seed + i * 57u) * (h - 3));
        double tw = 0.3 + 0.7 * mv_rand01(c->sc->seed + i * 91u) *
                             (0.5 + 0.5 * sin(c->t * 2.0 + i));
        fb_put(f, x, y, tw > 0.75 ? '*' : '.', rgb_scale(RGB(200, 220, 255), tw * 0.5));
    }

    /* Continents: fixed 3D blobs rotated by `spin`. */
    static const double land[7][3] = {
        { 0.62, 0.45, 0.64 }, { -0.70, 0.20, 0.68 }, { 0.10, -0.55, 0.83 },
        { -0.35, -0.60, -0.72 }, { 0.80, 0.30, -0.52 }, { -0.20, 0.72, -0.66 },
        { 0.30, -0.20, 0.93 },
    };
    double cs = cos(spin), sn = sin(spin);
    for (int y = (int)(cy - R * 0.5); y <= (int)(cy + R * 0.5); y++) {
        for (int x = (int)(cx - R); x <= (int)(cx + R); x++) {
            double nx = (x - cx) / R, ny = (y - cy) / (R * 0.5);
            double d2 = nx * nx + ny * ny;
            if (d2 > 1.0) continue;
            double nz = sqrt(1.0 - d2);
            double dx = nx * cs - nz * sn, dz = nx * sn + nz * cs, dy = -ny;
            double best = 0;
            for (int k = 0; k < 7; k++) {
                double dot = dx * land[k][0] + dy * land[k][1] + dz * land[k][2];
                if (dot > best) best = dot;
            }
            if (best > 0.86) {
                double shade = mv_clamp01((best - 0.86) / 0.12);
                unsigned char ch = shade > 0.6 ? '#' : (shade > 0.3 ? '+' : '.');
                fb_put(f, x, y, ch, rgb_mix(c1, c2, shade));
            } else if (((x + y) % 11 == 0) && d2 < 0.85) {
                fb_put(f, x, y, '.', rgb_scale(c1, 0.18));
            }
        }
    }
    /* Wireframe: meridians and parallels. */
    for (int i = 0; i < 8; i++) {
        double ph = spin + i * M_PI / 8.0;
        double rx = fabs(cos(ph)) * R;
        fb_ellipse(f, cx, cy, rx, R * 0.5, i == 0 ? '+' : ':', rgb_scale(c2, 0.45 * in), 0, 0, 1);
    }
    for (int k = -3; k <= 3; k++) {
        double lat = k * M_PI / 8.0;
        double rr = cos(lat) * R;
        fb_ellipse(f, cx, cy - sin(lat) * R * 0.5, rr, fabs(sin(0.35)) * rr * 0.35,
                   ':', rgb_scale(c2, 0.35 * in), 0, 0, 1);
    }
    char buf[48];
    snprintf(buf, sizeof buf, "WORLD %dx%d  SEED %04X", (int)(R * 2), (int)R,
             (unsigned)(c->sc->seed & 0xFFFF));
    fb_text(f, 3, h - 4, buf, rgb_scale(c1, 0.6));
    if (c->prog > 0.82) {
        double k = mv_clamp01((c->prog - 0.82) / 0.12);
        fb_text_center(f, 1, "NEW WORLD ONLINE", rgb_scale(c2, k));
    }
}

/* ========================================================== 6. GRIDWORLD == */

void fx_gridworld(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    (void)c1;
    double horizon = h * 0.36;
    double cx = w / 2.0;
    double beat = fx_beat(c, 6.0);

    /* Sky: stars and a wireframe sun. */
    for (int i = 0; i < 50; i++) {
        int x = (int)(mv_rand01(c->sc->seed + i * 17u) * w);
        int y = (int)(mv_rand01(c->sc->seed + i * 41u) * horizon);
        fb_put(f, x, y, '.', rgb_scale(RGB(180, 200, 255), 0.35 + 0.3 * beat));
    }
    fb_circle(f, cx, horizon - h * 0.10, fmin(w * 0.09, h * 0.16), '#',
              rgb_scale(c2, 0.55 + beat * 0.5), 0, 0, 1);

    /* Floor grid rushing towards the viewer. */
    for (int i = 0; i < 26; i++) {
        double d = (double)i + fmod(c->t * 1.6, 1.0);
        if (d < 0.2) continue;
        double y = horizon + (h - horizon) / (d * 0.42 + 1.0);
        if (y >= h - 3) continue;
        double k = mv_clamp01(1.0 / (d * 0.25 + 0.6));
        for (int x = 0; x < w; x++) {
            if ((x + i) % 3 == 0)
                fb_put(f, x, (int)y, '=', rgb_scale(c1, k * 0.8));
        }
    }
    int lanes = 9;
    for (int k = -lanes; k <= lanes; k++) {
        double x_bottom = cx + k * (w / (double)lanes);
        fb_line(f, cx + k * 1.2, horizon, x_bottom, h - 3, ':',
                rgb_scale(c1, 0.55 - fabs((double)k) / lanes * 0.3));
    }
    fb_line(f, 0, horizon, w, horizon, '=', rgb_scale(c2, 0.7));
    if (c->prog > 0.5) {
        char buf[40];
        snprintf(buf, sizeof buf, "SIMULATION %3.0f%%", c->prog * 100);
        fb_text(f, 3, h - 4, buf, rgb_scale(c2, 0.6));
    }
}

/* =============================================================== 7. CODE == */

static const char *CODE_LINES[] = {
    "/* hello, world */",
    "#include <mili.h>",
    "",
    "int main(void) {",
    "    struct world w;",
    "    world.init(&w, PARAMS);",
    "    while (w.alive) {",
    "        w.execute(me);",
    "    }",
    "    return LOVE;",
    "}",
};
#define CODE_NLINES ((int)(sizeof CODE_LINES / sizeof CODE_LINES[0]))

void fx_code(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    int x0 = (w > 76) ? (w - 74) / 2 : 1;
    int y0 = 2;
    int bw = (w > 76) ? 74 : w - 2;
    int bh = h - 7;
    if (bh < 10) { y0 = 1; bh = h - 5; }
    if (bh < 6) return;

    fx_panel(f, x0, y0, bw, bh, "~/world/execute.c", rgb_scale(c1, 0.7), c2);

    int total = 0;
    for (int i = 0; i < CODE_NLINES; i++) total += (int)strlen(CODE_LINES[i]) + 1;
    double type_end = c->dur * 0.68;
    int typed = (int)(total * mv_smooth(c->t / type_end));
    int col = 0, row = 0;
    int tcount = 0;
    int cx = 0, cy = 0;
    for (int i = 0; i < CODE_NLINES; i++) {
        const char *s = CODE_LINES[i];
        int y = y0 + 2 + i;
        if (y >= y0 + bh - 1) break;
        int n = (int)strlen(s);
        int vis = typed - tcount;
        if (vis < 0) vis = 0;
        if (vis > n) vis = n;
        for (int k = 0; k < vis; k++) {
            Rgb colr = rgb_scale(c1, 0.9);
            if (s[k] == '"' || s[k] == '<' || s[k] == '>') colr = c2;
            else if (k < 12 && (s[k] == 'i' || s[k] == 'r')) colr = rgb_mix(c1, c2, 0.5);
            fb_put(f, x0 + 3 + k, y, (unsigned char)s[k], colr);
        }
        if (vis > 0 && vis >= n) { cx = x0 + 3 + n; cy = y; }
        tcount += n + 1;
    }
    (void)col; (void)row;
    /* Blinking block cursor. */
    if (typed < total) {
        if (((int)(c->t * 3.0) % 2) == 0) fb_put(f, cx, cy, '#', c2);
        char st[40];
        snprintf(st, sizeof st, "typing  %3.0f%%", mv_clamp01((double)typed / total) * 100.0);
        fb_text(f, x0 + bw - (int)strlen(st) - 2, y0 + bh, st, rgb_scale(c1, 0.6));
        return;
    }
    /* Execution phase. */
    double run = c->t - type_end;
    if (run < 0.4) return;
    static const char *OUT[] = {
        "world.init ...  ok",
        "linking life.sys ...",
        "world.execute(me);",
        "EXECUTION COMPLETE",
    };
    int nout = (int)(sizeof OUT / sizeof OUT[0]);
    int show = (int)((run - 0.4) / 0.55) + 1;
    if (show > nout) show = nout;
    int base_y = y0 + 2 + CODE_NLINES + 1;
    fb_text(f, x0 + 3, base_y - 1, "$ ./a.out", rgb_scale(c1, 0.8));
    for (int i = 0; i < show; i++) {
        int y = base_y + i;
        if (y >= y0 + bh - 1) break;
        double age = run - 0.4 - i * 0.55;
        Rgb colr = (i == show - 1 && age < 0.35) ? RGB(255, 255, 255) : rgb_scale(c2, 0.85);
        fb_text(f, x0 + 3, y, OUT[i], colr);
    }
    if (run > 2.4) {
        double k = mv_clamp01((run - 2.4) / 0.8);
        fb_text_center(f, y0 + bh - 1, "RETURN LOVE;", rgb_scale(c2, k));
    }
}

/* ============================================================== 8. POINTS == */

void fx_points(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    (void)c1;
    int n = 110;
    double cx = w / 2.0, cy = (h - 3) / 2.0;
    double in = mv_ease_out(mv_clamp01(c->t / 2.0));
    double cube = mv_smooth(mv_clamp01((c->prog - 0.55) / 0.25));
    double rot = c->t * 0.5;

    double px[110], py[110];
    for (int i = 0; i < n; i++) {
        double a = mv_rand01(c->sc->seed + (uint32_t)i * 13u) * 2.0 * M_PI;
        double rr = 0.4 + mv_rand01(c->sc->seed + (uint32_t)i * 29u) * 0.6;
        double sx = cx + cos(a) * rr * w * 0.55;
        double sy = cy + sin(a) * rr * h * 0.55;
        /* Target 1: a flat lattice.  Target 2: the corners of a cube. */
        int gx = i % 11, gy = i / 11;
        double lx = cx - w * 0.22 + gx * (w * 0.44 / 10.0);
        double ly = cy - h * 0.20 + gy * (h * 0.40 / 9.0);
        double ux = (double)(i % 2 ? 1 : -1), uy = (double)((i / 2) % 2 ? 1 : -1);
        double uz = (double)((i / 4) % 2 ? 1 : -1);
        double x3 = ux * cos(rot) - uz * sin(rot), z3 = ux * sin(rot) + uz * cos(rot);
        double kx = cx + x3 * w * 0.18 + uy * w * 0.02;
        double ky = cy + uy * h * 0.22 + z3 * h * 0.06;
        double tx = mv_lerp(lx, kx, cube), ty = mv_lerp(ly, ky, cube);
        px[i] = mv_lerp(sx, tx, in);
        py[i] = mv_lerp(sy, ty, in);
        double jitter = 0.35 * (1.0 - in);
        px[i] += (mv_rand01(c->sc->seed + (uint32_t)i * 7u) - 0.5) * jitter * 4.0;
        py[i] += (mv_rand01(c->sc->seed + (uint32_t)i * 3u) - 0.5) * jitter * 2.0;
    }
    /* Constellation links between nearby points. */
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            double dx = px[i] - px[j], dy = (py[i] - py[j]) * 2.0;
            double d = sqrt(dx * dx + dy * dy);
            double lim = mv_lerp(3.6, 2.6, cube);
            if (d > lim) continue;
            unsigned char ch = fabs(dx) > fabs(dy) * 2 ? '-' : (fabs(dy) > fabs(dx) * 2 ? '|' : '/');
            Rgb col = rgb_scale(c2, (1.0 - d / lim) * 0.55 + 0.15);
            fb_line(f, px[i], py[i], px[j], py[j], ch, col);
        }
    }
    for (int i = 0; i < n; i++) {
        unsigned char ch = in > 0.95 ? (i % 5 == 0 ? '@' : 'o') : '.';
        fb_put(f, (int)lround(px[i]), (int)lround(py[i]), ch,
               rgb_scale(c2, 0.6 + 0.4 * mv_pulse(c->t * 0.7 + i * 0.1)));
    }
    char buf[48];
    snprintf(buf, sizeof buf, "points=%d  dimension=%s", n, cube > 0.5 ? "3" : "2");
    fb_text(f, 3, h - 4, buf, rgb_scale(c2, 0.6));
}

/* ============================================================== 9. CIRCLE == */

void fx_circle(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    (void)c1;
    double cx = w / 2.0, cy = (h - 3) / 2.0;
    double R = fmin(w * 0.30, h * 0.55);
    double draw = mv_ease_out(mv_clamp01(c->t / 1.8));
    double sweep = mv_clamp01((c->prog - 0.45) / 0.4);

    fb_circle(f, cx, cy, R, ':', rgb_scale(c2, 0.18), 0, 0, 1);
    fb_circle(f, cx, cy, R, '=', rgb_scale(c2, 0.95), 0, -0.25, -0.25 + draw);
    /* Radius spoke that rotates while the circle is being drawn. */
    double a = -0.25 * 2.0 * M_PI + draw * 2.0 * M_PI;
    fb_line(f, cx, cy, cx + cos(a) * R, cy + sin(a) * R * 0.5, '-', rgb_scale(c2, 0.7));
    fb_put(f, (int)lround(cx), (int)lround(cy), '+', c2);
    char buf[48];
    snprintf(buf, sizeof buf, "r = %.1f", R / 2.0);
    fb_text(f, (int)(cx + cos(a) * R * 0.5), (int)(cy + sin(a) * R * 0.25) - 1, buf,
            rgb_scale(c2, 0.75));
    /* Circumference: a bright dot running the ring, leaving a trail. */
    if (sweep > 0) {
        double head = sweep * 4.0;
        for (int k = 0; k < 40; k++) {
            double aa = (head - k * 0.012) * 2.0 * M_PI / 4.0;
            double kk = 1.0 - (double)k / 40.0;
            fb_put(f, (int)lround(cx + cos(aa) * R), (int)lround(cy + sin(aa) * R * 0.5),
                   k == 0 ? '@' : (k < 8 ? '*' : '='), rgb_scale(RGB(255, 255, 255), kk));
        }
        char cb[64];
        snprintf(cb, sizeof cb, "C = 2*pi*r = %.4f", 2.0 * M_PI * (R / 2.0));
        fb_text_center(f, h - 4, cb, rgb_scale(c2, 0.8));
    }
    /* Tick marks every 30 degrees. */
    for (int i = 0; i < 12; i++) {
        double aa = i / 12.0 * 2.0 * M_PI;
        fb_put(f, (int)lround(cx + cos(aa) * (R + 2)), (int)lround(cy + sin(aa) * (R + 2) * 0.5),
               '+', rgb_scale(c2, 0.4));
    }
}

/* ================================================================ 10. SINE == */

void fx_sine(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    (void)c1;
    double cy = (h - 3) / 2.0, A = h * 0.22, k = 0.22, omega = 2.4;
    double x0 = 3, x1 = w - 3;

    for (int x = (int)x0; x <= (int)x1; x += 6) fb_put(f, x, (int)cy, ':', rgb_scale(c2, 0.3));
    for (int y = 1; y < h - 3; y++) fb_put(f, 3, y, ':', rgb_scale(c2, 0.25));

    /* The wave itself. */
    double prevx = x0, prevy = cy - A * sin(k * x0 - omega * c->t);
    for (double x = x0 + 0.7; x <= x1; x += 0.7) {
        double y = cy - A * sin(k * x - omega * c->t);
        fb_line(f, prevx, prevy, x, y, '*', rgb_scale(c2, 0.9));
        prevx = x; prevy = y;
    }
    /* Family of tangents appears for the TANGENTS keyword. */
    double fam = mv_smooth(mv_clamp01((c->prog - 0.62) / 0.3));
    if (fam > 0) {
        for (int i = 0; i < 9; i++) {
            double px = x0 + (x1 - x0) * (double)i / 8.0;
            double sl = -A * k * cos(k * px - omega * c->t);
            double py = cy - A * sin(k * px - omega * c->t);
            double dx = 9.0;
            fb_line(f, px - dx, py + sl * dx * 0.5, px + dx, py - sl * dx * 0.5, '-',
                    rgb_scale(c2, 0.35 * fam));
        }
    }
    /* The rolling point of tangency. */
    double px = x0 + (x1 - x0) * fmod(c->t * 0.16, 1.0);
    double sl = -A * k * cos(k * px - omega * c->t);
    double py = cy - A * sin(k * px - omega * c->t);
    fb_line(f, px - 11, py + sl * 11 * 0.5, px + 11, py - sl * 11 * 0.5, '=',
            rgb_scale(RGB(255, 255, 255), 0.85));
    fb_put(f, (int)px, (int)py, '@', RGB(255, 255, 255));
    char buf[64];
    snprintf(buf, sizeof buf, "slope = %.2f   d/dx sin(x) = cos(x)", sl);
    fb_text(f, 5, h - 4, buf, rgb_scale(c2, 0.75));
}

/* =========================================================== 11. INFINITY == */

void fx_infinity(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    (void)c1;
    double cx = w / 2.0, cy = (h - 3) / 2.0;
    double A = fmin(w * 0.30, h * 0.60);
    double draw = mv_clamp01(c->t / 2.4);
    int steps = 520;
    double px = 0, py = 0;
    for (int i = 0; i <= (int)(steps * draw); i++) {
        double th = (double)i / steps * 2.0 * M_PI;
        double den = 1.0 + sin(th) * sin(th);
        double x = cx + A * cos(th) / den;
        double y = cy + A * 0.5 * cos(th) * sin(th) / den;
        if (i) {
            double trail = 1.0 - (double)((int)(steps * draw) - i) / (steps * 0.5);
            if (trail < 0) trail = 0;
            fb_line(f, px, py, x, y, '.', rgb_scale(c2, 0.25 + trail * 0.75));
        }
        px = x; py = y;
    }
    double head = draw * 2.0 * M_PI;
    double den = 1.0 + sin(head) * sin(head);
    fb_put(f, (int)lround(cx + A * cos(head) / den),
           (int)lround(cy + A * 0.5 * cos(head) * sin(head) / den), '@', RGB(255, 255, 255));

    /* The approach to the edge: dashed asymptote and limit notation. */
    int ax = w - 6;
    for (int y = 1; y < h - 4; y += 2) fb_put(f, ax, y, '|', rgb_scale(c2, 0.35));
    double trav = fmod(c->t * 0.5, 4.0) / 4.0;
    int tx = (int)(cx + trav * (ax - cx));
    fb_put(f, tx, (int)cy, '>', rgb_scale(RGB(255, 255, 255), 0.9));
    fb_text(f, ax - 4, 2, "inf", rgb_scale(c2, 0.9));
    fb_text(f, 3, h - 5, "lim   f(x) = inf", rgb_scale(c2, 0.8));
    fb_text(f, 3, h - 4, "x->+-inf", rgb_scale(c2, 0.6));
    if (c->prog > 0.7) {
        double k = mv_smooth(mv_clamp01((c->prog - 0.7) / 0.2));
        fb_text_center(f, 1, "LIMITATIONS", rgb_scale(c2, k * 0.9));
    }
}

/* ================================================================ 12. WAVE == */

void fx_wave(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    (void)c1;
    /* The rectification happens with the "To AC to DC" line (46.02s). */
    double morph = mv_smooth(mv_clamp01((c->t_ms - 46020) / 1400.0));
    int x0 = 4, x1 = w - 5, y0 = 2, y1 = h - 6;
    fx_panel(f, x0 - 1, y0 - 1, x1 - x0 + 3, y1 - y0 + 3, "OSCILLOSCOPE",
             rgb_scale(c2, 0.6), c2);
    double cy = (y0 + y1) / 2.0, A = (y1 - y0) * 0.40;
    for (int x = x0; x <= x1; x += 4) {
        for (int y = y0; y <= y1; y += 2) fb_put(f, x, y, '.', rgb_scale(c2, 0.12));
    }
    fb_line(f, x0 - 1, cy, x1 + 1, cy, '-', rgb_scale(c2, 0.3));
    double prevx = x0, prevy = cy;
    for (double x = x0; x <= x1; x += 0.5) {
        double ph = (x - x0) * 0.22 - c->t * 3.0;
        double s = sin(ph);
        /* 整流 = 全波整流：只剩正半周，恒 >= 0，再收敛成"直流电平 + 小纹波"。
         * 旧代码写成 fabs(s)*2-1，等于把 |sin| 又拉回 [-1,1]，于是"直流"照样
         * 有负半周。这里 rect 全程 >= 0，所以 morph 一旦过半就再也不会下到中轴以下。 */
        double rect = 0.84 + 0.16 * fabs(s);
        double v = mv_lerp(s, rect, morph);
        double y = cy - v * A;
        if (x > x0) fb_line(f, prevx, prevy, x, y, '#', rgb_scale(c2, 0.95 - morph * 0.1));
        prevx = x; prevy = y;
    }
    fb_text(f, x0 + 1, y0, morph < 0.5 ? "AC" : "DC", rgb_mix(RGB(255, 200, 80), c2, morph));
    fb_text(f, x1 - 3, y0, "+", rgb_scale(c2, 0.7));
    fb_text(f, x0 + 1, y1, "-", rgb_scale(c2, 0.7));
    char buf[64];
    snprintf(buf, sizeof buf, "current: %s   %s", morph > 0.95 ? "DIRECT (DC)" : "ALTERNATING (AC)",
             morph > 0.5 ? "rectifying" : "switching");
    fb_text(f, x0, y1 + 2, buf, rgb_scale(c2, 0.7 + 0.3 * morph));
    /* Spark gaps jumping as the current switches. */
    if (morph > 0.1 && morph < 0.98) {
        for (int i = 0; i < 12; i++) {
            int sx = x0 + (int)(mv_rand01((uint32_t)(c->t * 60.0) + i) * (x1 - x0));
            int sy = y0 + (int)(mv_rand01((uint32_t)(c->t * 91.0) + i * 3u) * (y1 - y0));
            fb_put(f, sx, sy, '*', RGB(255, 240, 180));
        }
    }
}

/* =============================================================== 13. BLIND == */

void fx_blind(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    /* Vision collapses into bright scan slits that roll and thin out, then
     * everything whites out for a beat. */
    double blind = mv_smooth(mv_clamp01((c->t_ms - 47760) / 1500.0));
    int gap = 1 + (int)(blind * 5.0);
    int roll = (int)(c->t * 26.0) % gap;
    double flash = mv_clamp01((c->t_ms - 49200) / 400.0);
    for (int y = 0; y < h; y++) {
        int slit = ((y + roll) % gap) == 0;
        if (slit) {
            for (int x = 0; x < w; x++) {
                double band = 0.40 + 0.60 * sin(x * 0.09 - c->t * 5.0);
                if (band < 0.2) continue;
                unsigned char ch = band > 0.85 ? '#' : (band > 0.55 ? '=' : '-');
                fb_put(f, x, y, ch, rgb_scale(rgb_mix(c1, c2, 0.3 + 0.4 * blind),
                                            0.35 + band * 0.6));
            }
        } else if (blind > 0.35) {
            for (int x = (y * 7) % 9; x < w; x += 9)
                fb_put(f, x, y, '.', rgb_scale(c2, 0.18));
        }
    }
    /* A pulsing iris closing around the middle. */
    double r = (1.0 - blind) * fmin(w * 0.4, h * 0.8);
    fb_circle(f, w / 2.0, h / 2.0, r, 'o', rgb_scale(c2, 0.7), 0, 0, 1);
    char buf[48];
    snprintf(buf, sizeof buf, "VISION %3.0f%%", (1.0 - blind) * 100.0);
    fb_text(f, 2, h - 4, buf, rgb_scale(c2, 0.8));
    if (flash > 0) {
        for (int y = 0; y < h - 3; y++)
            for (int x = 0; x < w; x++)
                fb_put(f, x, y, flash > 0.6 ? '#' : '+',
                       rgb_scale(RGB(255, 255, 255), flash));
    }
}

/* ================================================================ 14. SPIN == */

void fx_spin(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    (void)c1;
    double cx = w / 2.0, cy = (h - 3) / 2.0;
    double R = fmin(w * 0.45, h * 0.85);
    for (int arm = 0; arm < 3; arm++) {
        for (double r = 1.0; r < R; r += 0.4) {
            double a = r * 0.30 + c->t * 5.0 + arm * 2.0 * M_PI / 3.0;
            int x = (int)lround(cx + cos(a) * r);
            int y = (int)lround(cy + sin(a) * r * 0.5);
            double k = 1.0 - r / R;
            fb_put(f, x, y, k > 0.6 ? '#' : (k > 0.3 ? '+' : '.'), rgb_scale(c2, 0.25 + k * 0.85));
        }
    }
    /* Orbiting dizzy sparks. */
    for (int i = 0; i < 16; i++) {
        double a = c->t * 2.2 + i / 16.0 * 2.0 * M_PI;
        double rr = R * (0.45 + 0.5 * mv_rand01(c->sc->seed + (uint32_t)i * 7u));
        fb_put(f, (int)lround(cx + cos(a) * rr), (int)lround(cy + sin(a) * rr * 0.5),
               i % 4 == 0 ? '*' : 'o', rgb_scale(RGB(255, 255, 255), 0.5 + 0.5 * sin(c->t * 4 + i)));
    }
    fb_put(f, (int)cx, (int)cy, '@', RGB(255, 255, 255));
    fb_text_center(f, h - 4, "so dizzy ...", rgb_scale(c2, 0.8));
    /* Make the whole frame wobble, which is what sells the dizziness. */
    fb_shift(f, (int)lround(sin(c->t * 6.0) * 1.6), (int)lround(cos(c->t * 5.0) * 1.0));
}

/* =============================================================== 15. STARS == */

void fx_stars(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 3) / 2.0;
    int n = 150;
    double speed = 0.35 + c->prog * 0.5;
    for (int i = 0; i < n; i++) {
        double sx = mv_rand01(c->sc->seed + (uint32_t)i * 13u) * 2.0 - 1.0;
        double sy = mv_rand01(c->sc->seed + (uint32_t)i * 29u) * 2.0 - 1.0;
        double z0 = mv_rand01(c->sc->seed + (uint32_t)i * 47u);
        double z = 1.0 - fmod(c->t * speed + z0, 1.0);
        if (z < 0.05) continue;
        double x = cx + sx / z * w * 0.30;
        double y = cy + sy / z * h * 0.30;
        if (x < 0 || y < 1 || x >= w || y >= h - 3) continue;
        double z2 = z + 0.05;
        double x2 = cx + sx / z2 * w * 0.30, y2 = cy + sy / z2 * h * 0.30;
        double bright = mv_clamp01((1.0 - z) * 1.4);
        unsigned char ch = bright > 0.75 ? '@' : (bright > 0.4 ? '*' : '.');
        fb_line(f, x2, y2, x, y, ch, rgb_scale(c2, bright));
    }
    fb_text_center(f, h - 4, "travel", rgb_scale(c2, 0.5 + 0.3 * sin(c->t * 2)));
    (void)c1;
}

/* ============================================================ 16. TIMELINE == */

void fx_timeline(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    int y = (h - 3) / 2;
    /* Era bands: AD on the right, BC on the left. */
    for (int x = w / 2; x < w - 2; x++)
        for (int yy = y - 3; yy <= y + 3; yy++)
            if (((x + yy) % 4) == 0) fb_put(f, x, yy, '.', rgb_scale(c1, 0.16));
    for (int x = 2; x < w / 2; x++)
        for (int yy = y - 3; yy <= y + 3; yy++)
            if (((x + yy) % 3) == 0) fb_put(f, x, yy, ':', rgb_scale(c2, 0.16));
    fb_line(f, 2, y, w - 3, y, '=', rgb_scale(c2, 0.8));
    for (int x = 3; x < w - 3; x += 4) {
        int big = ((x - 3) % 20) == 0;
        fb_put(f, x, y - (big ? 2 : 1), '|', rgb_scale(c2, big ? 0.7 : 0.35));
    }
    /* The marker walks backwards from AD 2024 to BC 2024. */
    double p = mv_smooth(c->prog);
    int mx = (int)((w - 4) - p * (w - 7));
    for (int yy = y - 4; yy <= y + 4; yy++)
        fb_put(f, mx, yy, yy == y ? '@' : '|', RGB(255, 255, 255));
    char buf[32];
    int year = (int)((1.0 - p) * 2024.0);
    snprintf(buf, sizeof buf, p < 0.5 ? "A.D. %d" : "B.C %d", year);
    fb_text(f, mx - 4, y + 5, buf, rgb_scale(RGB(255, 255, 255), 0.9));
    fb_text(f, w - 14, y - 6, "A.D.", rgb_scale(c1, 0.85));
    fb_text(f, 3, y - 6, "B.C", rgb_scale(c2, 0.85));
    for (int i = 0; i < 5; i++) {
        int lx = 5 + i * (w - 12) / 4;
        char lb[16];
        snprintf(lb, sizeof lb, "%4d", (int)fabs((double)(2024 - (lx - 3) * 2024 / (w - 6))));
        fb_text(f, lx, y + 2, lb, rgb_scale(c2, 0.35));
    }
    if (fabs(p - 0.5) < 0.06) {
        for (int x = 0; x < w; x++)
            for (int yy = 1; yy < h - 3; yy++)
                if (((x + yy) % 2) == 0) fb_put(f, x, yy, '+', RGB(255, 255, 255));
    }
}

/* =============================================================== 17. MERGE == */

void fx_merge(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cy = (h - 3) / 2.0, cx = w / 2.0;
    double R = fmin(w * 0.13, h * 0.32);
    double meet = mv_smooth(mv_clamp01(c->t / (c->dur * 0.55)));
    double gap = (1.0 - meet) * w * 0.28;
    double xa = cx - gap, xb = cx + gap;
    fb_circle(f, xa, cy, R, '=', rgb_scale(c1, 0.95), 0, 0, 1);
    fb_circle(f, xb, cy, R, '=', rgb_scale(c2, 0.95), 0, 0, 1);
    double dist = fabs(xb - xa);
    if (dist < R * 2.2) {
        /* Overlap lights up. */
        for (int y = (int)(cy - R * 0.5); y <= (int)(cy + R * 0.5); y++) {
            for (int x = (int)(cx - R * 2.2); x <= (int)(cx + R * 2.2); x++) {
                double da = hypot((x - xa) / R, (y - cy) / (R * 0.5));
                double db = hypot((x - xb) / R, (y - cy) / (R * 0.5));
                if (da <= 1 && db <= 1 && ((x + y) % 2 == 0))
                    fb_put(f, x, y, '+', rgb_mix(c1, RGB(255, 255, 255), 0.6));
            }
        }
    }
    if (meet > 0.97) {
        double age = c->t - c->dur * 0.55;
        if (age > 0 && age < 1.4) {
            double r = age * fmin(w * 0.5, h);
            fb_circle(f, cx, cy, r, '*', rgb_scale(RGB(255, 255, 255), mv_clamp01(1.0 - age / 1.4)), 0, 0, 1);
            fb_sprite_center(f, "heart", (int)cx, (int)cy, fx_fit_scale(c, "heart", 0.22, 0.4),
                             rgb_scale(c1, 0.4), RGB(255, 255, 255), mv_clamp01(1.0 - age / 1.4), 1.0);
        }
    }
    fb_text_center(f, h - 4, meet > 0.9 ? "UNITED" : "unite ...", rgb_scale(c2, 0.75));
}

/* ============================================================== 18. TUNNEL == */

void fx_tunnel(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    (void)c1;
    double cx = w / 2.0, cy = (h - 3) / 2.0;
    for (int i = 0; i < 14; i++) {
        double s = fmod((double)i / 14.0 + c->t * 0.30, 1.0);
        if (s < 0.02) continue;
        double rx = s * w * 0.5, ry = s * (h - 3) * 0.5;
        double k = mv_clamp01(s * 1.15);
        unsigned char ch = k > 0.6 ? '=' : (k > 0.25 ? '-' : '.');
        fb_ellipse(f, cx, cy, rx, ry, ch, rgb_scale(c2, 0.15 + k * 0.8), 0, 0, 1);
        if (i % 4 == 0) {
            fb_put(f, (int)(cx - rx), (int)cy, '+', rgb_scale(c2, k));
            fb_put(f, (int)(cx + rx), (int)cy, '+', rgb_scale(c2, k));
            fb_put(f, (int)cx, (int)(cy - ry), '+', rgb_scale(c2, k));
            fb_put(f, (int)cx, (int)(cy + ry), '+', rgb_scale(c2, k));
        }
    }
    for (int i = 0; i < 12; i++) {
        double a = i / 12.0 * 2.0 * M_PI + c->t * 0.1;
        fb_line(f, cx, cy, cx + cos(a) * w, cy + sin(a) * (h - 3), ':', rgb_scale(c2, 0.18));
    }
    fb_put(f, (int)cx, (int)cy, '@', RGB(255, 255, 255));
    char buf[48];
    snprintf(buf, sizeof buf, "DEPTH %4.2f m", 0.5 + c->prog * 9.5);
    fb_text(f, 3, h - 4, buf, rgb_scale(c2, 0.8));
    fb_text_center(f, h - 4, "so deeply ...", rgb_scale(c2, 0.4));
}

/* =============================================================== 19. PULSE == */

void fx_pulse(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 3) / 2.0;
    double hot = mv_smooth(mv_clamp01((c->t_ms - 61920) / 900.0));  /* STIMULATIONS */

    /* Rings born on every beat and expanding outward. */
    for (int k = 0; k < 5; k++) {
        double age = c->phase + k;
        double r = age * fmin(w, h * 2.0) * 0.10;
        double k2 = mv_clamp01(1.0 - age / 5.0);
        if (k2 <= 0) continue;
        fb_ellipse(f, cx, cy, r, r * 0.5, k == 0 ? '#' : '=',
                   rgb_scale(c2, k2 * (0.5 + hot * 0.6)), 0, 0, 1);
    }
    /* Radiating dendrites. */
    for (int i = 0; i < 40; i++) {
        double a = mv_rand01(c->sc->seed + (uint32_t)i * 7u) * 2.0 * M_PI;
        double len = (0.2 + 0.8 * mv_rand01(c->sc->seed + (uint32_t)i * 13u)) *
                     fmin(w * 0.5, h) * (0.5 + 0.5 * fx_beat(c, 4.0) + hot * 0.4);
        fb_line(f, cx, cy, cx + cos(a) * len, cy + sin(a) * len * 0.5,
                i % 3 ? '.' : '+', rgb_scale(c1, 0.35 + 0.5 * fx_beat(c, 6.0)));
    }
    double core = 0.6 + 0.6 * fx_beat(c, 5.0);
    fb_circle(f, cx, cy, (2.0 + hot * 3.0) * core, '@', rgb_mix(c2, RGB(255, 255, 255), 0.4), 1, 0, 1);
    char buf[48];
    snprintf(buf, sizeof buf, "STIMULATION %4.1f mV", 20.0 + 80.0 * fx_beat(c, 3.0) + hot * 40);
    fb_text(f, 3, h - 4, buf, rgb_scale(c2, 0.85));
    if (hot > 0.5) {
        fb_shift(f, (int)lround(sin(c->t * 40.0) * hot * 1.5), 0);
    }
}

/* =============================================================== 20. HEART == */

void fx_heart(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 4) / 2.0;
    double beat = fx_beat(c, 5.0);
    double base = fx_fit_scale(c, "heart", 0.30, 0.55);
    double s = base * (1.0 + 0.10 * beat) * mv_lerp(0.4, 1.0, fx_enter(c, 0.6));
    fb_circle(f, cx, cy, base * 5.0 * (1.0 + 0.12 * beat), ':',
              rgb_scale(c1, 0.25 + 0.3 * beat), 0, 0, 1);
    fb_sprite_center(f, "heart", (int)cx, (int)cy, s, rgb_scale(c1, 0.5),
                     rgb_mix(c2, RGB(255, 255, 255), 0.25), 0.85 + 0.35 * beat, 1.0);
    /* Small hearts drifting up. */
    for (int i = 0; i < 7; i++) {
        double t0 = mv_rand01(c->sc->seed + (uint32_t)i * 11u);
        double y = (h - 4) - fmod(c->t * 0.7 + t0 * 4.0, (double)(h - 4));
        int x = (int)(mv_rand01(c->sc->seed + (uint32_t)i * 23u) * w);
        double a = mv_clamp01(1.0 - fmod(c->t * 0.7 + t0 * 4.0, (double)(h - 4)) / (h - 4));
        fb_put(f, x, (int)y, i % 2 ? '<' : '>', rgb_scale(c2, a * 0.8));
    }
    /* Satisfaction gauge. */
    double p = mv_smooth(mv_clamp01((c->prog - 0.15) / 0.7));
    int bw = (w > 60) ? 40 : w - 16;
    int x0 = (w - bw) / 2 - 6;
    if (x0 < 1) x0 = 1;
    fb_text(f, x0, h - 5, "SATISFACTION", rgb_scale(c2, 0.9));
    for (int i = 0; i < bw; i++)
        fb_put(f, x0 + 13 + i, h - 5, (double)i / bw < p ? '#' : '-',
               rgb_mix(c1, c2, (double)i / bw));
    char buf[16];
    snprintf(buf, sizeof buf, "%3.0f%%", p * 100.0);
    fb_text(f, x0 + 14 + bw, h - 5, buf, p >= 1.0 ? RGB(255, 255, 255) : rgb_scale(c2, 0.8));
    if (p >= 1.0) fb_text_center(f, 1, "FULL", RGB(255, 255, 255));
}

/* =============================================================== 21. SMILE == */

void fx_smile(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    double cx = w / 2.0, cy = (h - 4) / 2.0 - 1;
    double R = fmin(w * 0.20, h * 0.40);
    double happy = mv_smooth(mv_clamp01((c->t_ms - 66660) / 1200.0));
    double run = mv_clamp01((c->t_ms - 69330) / 500.0);   /* EXECUTION */
    (void)c1;
    fb_circle(f, cx, cy, R, '=', rgb_scale(c2, 0.9), 0, 0, 1);
    /* Eyes: happy arcs that turn into determined dots at the execution. */
    for (int sgn = -1; sgn <= 1; sgn += 2) {
        double ex = cx + sgn * R * 0.45, ey = cy - R * 0.22;
        if (run < 0.5) {
            for (int i = -3; i <= 3; i++)
                fb_put(f, (int)(ex + i), (int)(ey + (i * i) / 5 - 1), '^', RGB(255, 255, 255));
        } else {
            fb_put(f, (int)ex, (int)ey, 'O', RGB(255, 255, 255));
            fb_put(f, (int)ex, (int)ey + 1, 'O', RGB(255, 255, 255));
        }
    }
    /* Mouth: wide smile that flattens when the execution runs. */
    for (int i = -6; i <= 6; i++) {
        double my = cy + R * 0.25 + (1.0 - happy * 0.5) * (i * i) * 0.06 - run * 0.5;
        fb_put(f, (int)(cx + i), (int)my, i == 0 ? 'v' : '~', RGB(255, 255, 255));
    }
    int bw = 16;
    int bx = (int)(w - bw) / 2, by = h - 4;
    for (int i = 0; i < bw; i++) {
        fb_put(f, bx + i, by, run > 0.5 && i % 2 == 0 ? '@' : '-',
               run > 0.5 ? RGB(255, 255, 255) : rgb_scale(c2, 0.7));
    }
    fb_text(f, bx + 2, by, run > 0.5 ? "RUNNING" : "[ RUN ]",
            run > 0.5 ? RGB(255, 255, 255) : rgb_scale(c2, 0.95));
    char buf[40];
    snprintf(buf, sizeof buf, "happiness %3.0f%%", 40 + happy * 60 - run * 40);
    fb_text(f, 3, h - 4, buf, rgb_scale(c2, 0.8));
}

/* ================================================================ 22. CAGE == */

void fx_cage(Ctx *c)
{
    Frame *f = c->f;
    int w = c->w, h = c->h;
    Rgb c1 = c->sc->c1, c2 = c->sc->c2;
    /* The bars close in over the first half of the scene, then just rattle. */
    double close = mv_smooth(mv_clamp01(c->t / (c->dur * 0.5)));
    double cx = w / 2.0;
    int top = 3;
    int bot = h - 6;
    int halfw = (int)mv_lerp(w * 0.36, w * 0.22, close);
    if (halfw < 6) halfw = 6;
    if (halfw > w / 2 - 2) halfw = w / 2 - 2;
    double cy = (top + bot) / 2.0;

    /* Dome over the top of the cage. */
    fb_ellipse(f, cx, top + 1, halfw, 3.4, '=', rgb_scale(c2, 0.75), 0, 0.5, 1.0);
    for (int x = (int)(cx - halfw); x <= (int)(cx + halfw); x++)
        fb_put(f, x, top, '=', rgb_scale(c2, 0.7));

    /* Floor and its shadow. */
    for (int x = (int)(cx - halfw * 1.18); x <= (int)(cx + halfw * 1.18); x++) {
        fb_put(f, x, bot, '=', rgb_scale(c2, 0.85));
        fb_put(f, x, bot + 1, '_', rgb_scale(c2, 0.3));
    }

    /* What is trapped inside. */
    const char *inside = (c->t_ms > 100000.0) ? "smile" : "heart";
    fb_sprite_center(f, inside, (int)cx, (int)cy, fx_fit_scale(c, inside, 0.20, 0.34),
                     rgb_scale(c1, 0.4), rgb_mix(c2, RGB(255, 255, 255), 0.25),
                     0.7 + 0.45 * fx_beat(c, 5.0), 1.0);

    /* Vertical bars in front of whatever is caught in there. */
    int bars = (int)(halfw / 2.2);
    if (bars < 4) bars = 4;
    for (int i = 0; i <= bars; i++) {
        int x = (int)lround(cx - halfw + 2.0 * halfw * i / bars);
        for (int y = top; y < bot; y++) {
            double k = 0.5 + 0.4 * mv_noise2(x, y * 3 + i, c->sc->seed);
            if (((x * 5 + y) % 17) == 0) continue;
            fb_put(f, x, y, '|', rgb_scale(c2, k * (1.0 - 0.2 * (y - top) / (double)(bot - top))));
        }
    }
    /* Hoops. */
    for (int gy = top + 4; gy < bot - 1; gy += 5)
        for (int x = (int)(cx - halfw + 1); x <= (int)(cx + halfw - 1); x++)
            fb_put(f, x, gy, '-', rgb_scale(c2, 0.3));

    /* Every few seconds the whole thing shakes. */
    if (close > 0.8) {
        double ph = fmod(c->t, 3.7);
        double rattle = ph < 0.36 ? (1.0 - ph / 0.36) * close : 0.0;
        if (rattle > 0)
            fb_shift(f, (int)lround(sin(c->t * 55.0) * (1.0 + rattle * 2.0)), 0);
    }

    /* Sparks escaping through the bars. */
    for (int i = 0; i < 9; i++) {
        double t0 = mv_rand01(c->sc->seed + (uint32_t)i * 13u);
        double life = fmod(c->t * 0.55 + t0, 1.0);
        int x = (int)(cx + mv_rand01(c->sc->seed + (uint32_t)i * 29u) * halfw * 1.3 *
                      (i % 2 ? 1 : -1));
        int y = (int)(bot - 1 - life * (bot - top - 2));
        fb_put(f, x, y, life > 0.7 ? '*' : '.', rgb_scale(c1, (1.0 - life) * 0.8));
    }

    char buf[48];
    if (close > 0.96) snprintf(buf, sizeof buf, "TRAPPED");
    else snprintf(buf, sizeof buf, "confinement %3.0f%%", close * 100.0);
    fb_text(f, 3, h - 5, buf, rgb_scale(c2, 0.9));
}
