/* gfx.c -- cell framebuffer, drawing primitives and the transition compositor.
 *
 * A "cell" is one terminal character cell.  Cells are twice as tall as they are
 * wide, so anything meant to look round uses a 2:1 ellipse (see fb_circle) and
 * banner type is sampled into square blocks of cells (see fb_banner).
 */
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "mv.h"
#include "font_data.h"
#include "sprites.h"

const char MV_RAMP[] = " .:-=+*#%@";

#define RAMP_LEN ((int)(sizeof(MV_RAMP) - 1))

/* ------------------------------------------------------------------ helpers */

static inline int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline double clampd(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline uint8_t clamp8(double v)
{
    return (uint8_t)clampd(v, 0.0, 255.0);
}

Rgb rgb_scale(Rgb c, double k)
{
    return (Rgb){ clamp8(c.r * k), clamp8(c.g * k), clamp8(c.b * k) };
}

Rgb rgb_mix(Rgb a, Rgb b, double t)
{
    return (Rgb){ clamp8(a.r + (b.r - a.r) * t),
                  clamp8(a.g + (b.g - a.g) * t),
                  clamp8(a.b + (b.b - a.b) * t) };
}

Rgb rgb_hsv(double h, double s, double v)
{
    h = fmod(h, 1.0);
    if (h < 0) h += 1.0;
    double i = floor(h * 6.0), f = h * 6.0 - i;
    double p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    double r = 0, g = 0, b = 0;
    switch (((int)i) % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return (Rgb){ clamp8(r * 255), clamp8(g * 255), clamp8(b * 255) };
}

uint32_t mv_hash(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

double mv_rand01(uint32_t x)
{
    return (double)(mv_hash(x) >> 8) / 16777216.0;
}

double mv_noise2(int x, int y, int seed)
{
    uint32_t h = mv_hash((uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663) ^
                         (uint32_t)(seed * 83492791));
    return (double)(h >> 8) / 16777216.0;
}

double mv_density(unsigned char ch)
{
    if (ch == 0 || ch == ' ') return 0.0;
    const char *p = strchr(MV_RAMP, (char)ch);
    if (!p) return 0.85;
    return (double)(p - MV_RAMP) / (double)(RAMP_LEN - 1);
}

unsigned char mv_char_for(double d)
{
    int i = (int)lround(d * (RAMP_LEN - 1));
    return (unsigned char)MV_RAMP[clampi(i, 0, RAMP_LEN - 1)];
}

double mv_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void mv_sleep(double seconds)
{
    if (seconds <= 0) return;
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    if (ts.tv_nsec < 0) ts.tv_nsec = 0;
    nanosleep(&ts, NULL);
}

/* --------------------------------------------------------------- framebuffer */

void frame_alloc(Frame *f, int w, int h)
{
    f->w = w;
    f->h = h;
    f->cells = calloc((size_t)w * (size_t)h, sizeof(Cell));
    if (!f->cells) {
        fprintf(stderr, "%s: out of memory for a %dx%d frame\n", PROG_NAME, w, h);
        exit(1);
    }
}

void frame_free(Frame *f)
{
    free(f->cells);
    f->cells = NULL;
    f->w = f->h = 0;
}

void frame_clear(Frame *f, Rgb fg)
{
    int n = f->w * f->h;
    /* The canvas is painted explicitly: otherwise the artwork (which is drawn
     * for a dark screen) is at the mercy of the terminal's own theme. */
    int solid = CFG.canvas_solid ? 1 : 0;
    for (int i = 0; i < n; i++) {
        f->cells[i].ch = 0;
        f->cells[i].fg = fg;
        f->cells[i].bg = CFG.canvas_bg;
        f->cells[i].has_bg = solid;
    }
}

void frame_invalidate(Frame *f)
{
    int n = f->w * f->h;
    for (int i = 0; i < n; i++) {
        f->cells[i].ch = 0;
        f->cells[i].has_bg = -1;   /* matches nothing */
    }
}

void frame_set_bg(Frame *f, Rgb bg)
{
    int n = f->w * f->h;
    for (int i = 0; i < n; i++) {
        f->cells[i].bg = bg;
        f->cells[i].has_bg = 1;
    }
}

void fb_put(Frame *f, int x, int y, unsigned char ch, Rgb fg)
{
    if (x < 0 || y < 0 || x >= f->w || y >= f->h) return;
    Cell *c = &f->cells[y * f->w + x];
    c->ch = ch ? ch : ' ';
    c->fg = fg;
}

void fb_put_bg(Frame *f, int x, int y, unsigned char ch, Rgb fg, Rgb bg)
{
    if (x < 0 || y < 0 || x >= f->w || y >= f->h) return;
    Cell *c = &f->cells[y * f->w + x];
    c->ch = ch ? ch : ' ';
    c->fg = fg;
    c->bg = bg;
    c->has_bg = 1;
}

int fb_text_width(const char *s)
{
    return (int)strlen(s);
}

void fb_text(Frame *f, int x, int y, const char *s, Rgb fg)
{
    if (y < 0 || y >= f->h) return;
    for (int i = 0; s[i]; i++) {
        int px = x + i;
        if (px < 0) continue;
        if (px >= f->w) break;
        fb_put(f, px, y, (unsigned char)s[i], fg);
    }
}

void fb_text_center(Frame *f, int y, const char *s, Rgb fg)
{
    fb_text(f, (f->w - fb_text_width(s)) / 2, y, s, fg);
}

/* ---------------------------------------------------------------- bitmap font */

static inline int font_bit(unsigned char ch, int row, int col)
{
    int idx = (int)ch - FONT_FIRST;
    if (ch == ' ') return 0;
    if (idx < 0 || idx >= FONT_GLYPHS) idx = (int)'?' - FONT_FIRST;
    if (row < 0 || row >= FONT_H || col < 0 || col >= FONT_W) return 0;
    return (FONT_BITS[idx][row] >> (FONT_W - 1 - col)) & 1;
}

int fb_banner_width(int len, int cell_h)
{
    if (len <= 0 || cell_h <= 0) return 0;
    return len * cell_h + (len - 1);
}

/* Vertical ink band shared by a whole string: sampling the full 16-row cell
 * wastes half the block on empty rows, so the letters come out as solid slabs.
 * Mapping the string's own ink rows onto the block keeps them legible. */
static void banner_band(const char *s, int *r0, int *r1)
{
    int lo = FONT_H, hi = -1;
    for (const char *p = s; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch < FONT_FIRST || ch > FONT_LAST) continue;
        for (int r = 0; r < FONT_H; r++) {
            for (int c = 0; c < FONT_W; c++) {
                if (font_bit(ch, r, c)) {
                    if (r < lo) lo = r;
                    if (r > hi) hi = r;
                    break;
                }
            }
        }
    }
    if (hi < lo) { lo = 4; hi = 12; }
    *r0 = lo;
    *r1 = hi;
}

/* Push the artwork behind a letter out of the way so the type stays readable. */
void fb_dim_rect(Frame *f, int x, int y, int w, int h, double k)
{
    if (w <= 0 || h <= 0) return;
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= f->h) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= f->w) continue;
            Cell *c = &f->cells[j * f->w + i];
            c->fg = rgb_scale(c->fg, k);
            if (c->has_bg) c->bg = rgb_scale(c->bg, k);
        }
    }
}

static void cell_scrim(Frame *f, int x, int y, double k)
{
    fb_dim_rect(f, x, y, 1, 1, k);
}

/* Draw one glyph sampled into a cell_h x cell_h block of cells. */
static void banner_glyph(Frame *f, unsigned char ch, int x0, int y0, int cell_h,
                         int r0, int r1, Rgb col, double reveal, unsigned char ink)
{
    for (int r = 0; r < cell_h; r++) {
        int sy = (cell_h > 1)
                     ? r0 + (int)((double)r * (r1 - r0) / (double)(cell_h - 1) + 0.5)
                     : (r0 + r1) / 2;
        if (sy > FONT_H - 1) sy = FONT_H - 1;
        for (int c = 0; c < cell_h; c++) {
            int sx = (cell_h > 1)
                         ? (int)((double)c * (FONT_W - 1) / (double)(cell_h - 1) + 0.5)
                         : FONT_W / 2;
            if (!font_bit(ch, sy, sx)) continue;
            int x = x0 + c, y = y0 + r;
            /* reveal: left-to-right wipe with a noisy leading edge */
            double order = (double)x / (double)(f->w > 1 ? f->w - 1 : 1);
            double n = mv_noise2(x, y, 91) * 0.12;
            if (reveal < 1.0 && order > reveal + n) continue;
            cell_scrim(f, x, y, 0.30);
            fb_put(f, x, y, ink, col);
        }
    }
}

int fb_banner(Frame *f, int cx, int cy, const char *s, int cell_h, Rgb fg,
              double reveal, unsigned char ink, int shadow)
{
    int len = (int)strlen(s);
    if (len <= 0 || cell_h < 1) return 0;
    int width = fb_banner_width(len, cell_h);
    int x = cx - width / 2;
    int y = cy - cell_h / 2;
    int r0 = 4, r1 = 12;
    banner_band(s, &r0, &r1);

    if (shadow) {
        Rgb sh = rgb_scale(fg, 0.22);
        for (int i = 0; i < len; i++)
            banner_glyph(f, (unsigned char)s[i], x + 1 + i * (cell_h + 1), y + 1,
                         cell_h, r0, r1, sh, reveal, ink);
    }
    for (int i = 0; i < len; i++)
        banner_glyph(f, (unsigned char)s[i], x + i * (cell_h + 1), y, cell_h,
                     r0, r1, fg, reveal, ink);
    return width;
}

void fb_banner_fit(Frame *f, int cx, int cy, const char *s, int max_h, int max_w,
                   Rgb fg, double reveal, unsigned char ink, int shadow)
{
    int len = (int)strlen(s);
    if (len <= 0) return;
    int h = max_h;
    while (h > 2 && fb_banner_width(len, h) > max_w) h--;
    if (fb_banner_width(len, h) > max_w) return;   /* too long to show at all */
    fb_banner(f, cx, cy, s, h, fg, reveal, ink, shadow);
}

/* ------------------------------------------------------------------ geometry */

void fb_line(Frame *f, double x0, double y0, double x1, double y1,
             unsigned char ch, Rgb fg)
{
    double dx = x1 - x0, dy = y1 - y0;
    int steps = (int)ceil(fmax(fabs(dx), fabs(dy)) * 2.0);
    if (steps < 1) steps = 1;
    if (steps > 20000) steps = 20000;
    for (int i = 0; i <= steps; i++) {
        double t = (double)i / steps;
        fb_put(f, (int)lround(x0 + dx * t), (int)lround(y0 + dy * t), ch, fg);
    }
}

void fb_rect(Frame *f, int x, int y, int w, int h, unsigned char ch, Rgb fg)
{
    for (int i = 0; i < w; i++) {
        fb_put(f, x + i, y, ch, fg);
        fb_put(f, x + i, y + h - 1, ch, fg);
    }
    for (int j = 0; j < h; j++) {
        fb_put(f, x, y + j, ch, fg);
        fb_put(f, x + w - 1, y + j, ch, fg);
    }
}

/* Ellipse/arc in cell space.  arc0/arc1 are in turns (0..1). */
void fb_ellipse(Frame *f, double cx, double cy, double rx, double ry, unsigned char ch,
                Rgb fg, int filled, double arc0, double arc1)
{
    (void)filled;
    double turns = arc1 - arc0;
    int steps = (int)(fmax(rx, ry * 2.0) * 12.0);
    if (steps < 24) steps = 24;
    if (steps > 8000) steps = 8000;
    for (int i = 0; i <= steps; i++) {
        double a = (arc0 + turns * (double)i / steps) * 2.0 * M_PI;
        fb_put(f, (int)lround(cx + cos(a) * rx), (int)lround(cy + sin(a) * ry), ch, fg);
    }
}

void fb_circle(Frame *f, double cx, double cy, double r, unsigned char ch, Rgb fg,
               int filled, double arc0, double arc1)
{
    if (filled) {
        for (int y = (int)floor(cy - r * 0.5); y <= (int)ceil(cy + r * 0.5); y++) {
            for (int x = (int)floor(cx - r); x <= (int)ceil(cx + r); x++) {
                double nx = (x - cx) / (r > 0 ? r : 1);
                double ny = (y - cy) / (r > 0 ? r * 0.5 : 1);
                if (nx * nx + ny * ny <= 1.0) fb_put(f, x, y, ch, fg);
            }
        }
        return;
    }
    fb_ellipse(f, cx, cy, r, r * 0.5, ch, fg, 0, arc0, arc1);
}

/* ------------------------------------------------------------------- sprites */

int fb_sprite_size(const char *name, int *w, int *h)
{
    const MvSprite *sp = mv_sprite(name);
    if (!sp) return 0;
    if (w) *w = sp->w;
    if (h) *h = sp->h;
    return 1;
}

void fb_sprite(Frame *f, const char *name, int x, int y, Rgb dim, Rgb bright,
               double gain, double reveal)
{
    const MvSprite *sp = mv_sprite(name);
    if (!sp) return;
    for (int r = 0; r < sp->h; r++) {
        const char *row = sp->rows[r];
        for (int c = 0; c < sp->w && row[c]; c++) {
            if (row[c] == ' ') continue;
            int px = x + c, py = y + r;
            if (px < 0 || py < 0 || px >= f->w || py >= f->h) continue;
            double order = (double)c / (double)(sp->w > 1 ? sp->w - 1 : 1);
            if (reveal < 1.0 && order + mv_noise2(px, py, 33) * 0.15 > reveal) continue;
            double d = mv_density((unsigned char)row[c]) * clampd(gain, 0.0, 2.0);
            d = clampd(d, 0.0, 1.0);
            /* Even the sparsest ink of the subject stays well clear of black. */
            Rgb col = rgb_mix(rgb_mix(dim, bright, 0.45), bright, d * 0.85);
            fb_put(f, px, py, (unsigned char)row[c], col);
        }
    }
}

void fb_sprite_center(Frame *f, const char *name, int cx, int cy, double scale,
                      Rgb dim, Rgb bright, double gain, double reveal)
{
    const MvSprite *sp = mv_sprite(name);
    if (!sp) return;
    int tw = (int)lround(sp->w * scale), th = (int)lround(sp->h * scale);
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;
    int x0 = cx - tw / 2, y0 = cy - th / 2;
    /* Spotlight: dim the texture right behind the subject. */
    fb_scrim_ellipse(f, cx, cy, tw * 0.62, th * 0.72, 0.62);
    for (int r = 0; r < th; r++) {
        int sr = (int)((double)r * sp->h / th);
        const char *row = sp->rows[clampi(sr, 0, sp->h - 1)];
        for (int c = 0; c < tw; c++) {
            int sc = (int)((double)c * sp->w / tw);
            char g = row[clampi(sc, 0, sp->w - 1)];
            if (g == ' ') continue;
            int px = x0 + c, py = y0 + r;
            if (px < 0 || py < 0 || px >= f->w || py >= f->h) continue;
            double order = (double)c / (double)(tw > 1 ? tw - 1 : 1);
            if (reveal < 1.0 && order + mv_noise2(px, py, 33) * 0.15 > reveal) continue;
            double d = mv_density((unsigned char)g) * clampd(gain, 0.0, 2.0);
            d = clampd(d, 0.0, 1.0);
            fb_put(f, px, py, (unsigned char)g,
                   rgb_mix(rgb_mix(dim, bright, 0.45), bright, d * 0.85));
        }
    }
}

/* ------------------------------------------------------------- post-process */

/* A smoothstep tone map: everything below the toe falls towards black,
 * everything above the shoulder saturates towards full brightness. */
static double tone_map(double l, double amount)
{
    const double lo = 0.06, hi = 0.58;
    double x = clampd((l - lo) / (hi - lo), 0.0, 1.0);
    double s = x * x * (3.0 - 2.0 * x);
    return mv_lerp(l, s, amount);
}

static Rgb tone_rgb(Rgb c, double amount, double extra)
{
    double l = (0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b) / 255.0;
    if (l < 1e-4) return c;
    return rgb_scale(c, tone_map(l, amount) * extra / l);
}

void fb_contrast(Frame *f, double amount)
{
    if (amount <= 0.001) return;
    if (amount > 1.0) amount = 1.0;
    int n = f->w * f->h;
    for (int i = 0; i < n; i++) {
        Cell *c = &f->cells[i];
        /* Ink made of sparse glyphs is texture, dense glyphs are structure.
         * Sink the mid-brightness texture a little; never touch highlights. */
        double d = mv_density(c->ch);
        double l = (0.2126 * c->fg.r + 0.7152 * c->fg.g + 0.0722 * c->fg.b) / 255.0;
        double fade = clampd((0.65 - l) / 0.65, 0.0, 1.0);
        double extra = 1.0 - 0.45 * (1.0 - d) * amount * fade;
        c->fg = tone_rgb(c->fg, amount, extra);
        /* The background is the canvas the user asked for, not artwork: it
         * must survive the tone map untouched. */
    }
}

void fb_scrim_ellipse(Frame *f, double cx, double cy, double rx, double ry, double k)
{
    if (k <= 0.0 || rx <= 0 || ry <= 0) return;
    int x0 = (int)floor(cx - rx * 1.1), x1 = (int)ceil(cx + rx * 1.1);
    int y0 = (int)floor(cy - ry * 1.1), y1 = (int)ceil(cy + ry * 1.1);
    for (int y = y0; y <= y1; y++) {
        if (y < 0 || y >= f->h) continue;
        for (int x = x0; x <= x1; x++) {
            if (x < 0 || x >= f->w) continue;
            double dx = (x - cx) / rx, dy = (y - cy) / ry;
            double d = sqrt(dx * dx + dy * dy);
            if (d > 1.1) continue;
            double a = clampd((1.1 - d) / 0.35, 0.0, 1.0);
            Cell *c = &f->cells[y * f->w + x];
            c->fg = rgb_scale(c->fg, 1.0 - k * a);
            if (c->has_bg == 1) c->bg = rgb_scale(c->bg, 1.0 - k * a);
        }
    }
}

void fb_shade(Frame *f, double k)
{
    int n = f->w * f->h;
    for (int i = 0; i < n; i++) {
        f->cells[i].fg = rgb_scale(f->cells[i].fg, k);
        if (f->cells[i].has_bg) f->cells[i].bg = rgb_scale(f->cells[i].bg, k);
    }
}

void fb_tint(Frame *f, Rgb c, double k)
{
    int n = f->w * f->h;
    for (int i = 0; i < n; i++) {
        f->cells[i].fg = rgb_mix(f->cells[i].fg, c, k);
        if (f->cells[i].has_bg) f->cells[i].bg = rgb_mix(f->cells[i].bg, c, k);
    }
}

void fb_scanlines(Frame *f, double amount)
{
    for (int y = 1; y < f->h; y += 2)
        for (int x = 0; x < f->w; x++) {
            Cell *c = &f->cells[y * f->w + x];
            c->fg = rgb_scale(c->fg, 1.0 - amount);
        }
}

void fb_vignette(Frame *f, double amount)
{
    double cx = (f->w - 1) / 2.0, cy = (f->h - 1) / 2.0;
    double maxr = sqrt(cx * cx + cy * cy);
    for (int y = 0; y < f->h; y++) {
        for (int x = 0; x < f->w; x++) {
            double dx = (x - cx), dy = (y - cy) * 2.0;
            double r = sqrt(dx * dx + dy * dy) / (maxr > 0 ? maxr : 1);
            double k = 1.0 - amount * clampd(r * r, 0.0, 1.0);
            Cell *c = &f->cells[y * f->w + x];
            c->fg = rgb_scale(c->fg, k);
        }
    }
}

void fb_grain(Frame *f, int seed, double amount, Rgb c)
{
    if (amount <= 0) return;
    for (int y = 0; y < f->h; y++) {
        for (int x = 0; x < f->w; x++) {
            double n = mv_noise2(x, y, seed);
            if (n > amount) continue;
            Cell *cell = &f->cells[y * f->w + x];
            double d = mv_density(cell->ch);
            if (d < 0.05) {
                if (n < amount * 0.35)
                    fb_put(f, x, y, mv_char_for(n * 6.0), rgb_scale(c, 0.7));
            } else {
                cell->fg = rgb_mix(cell->fg, c, 0.10 * amount);
            }
        }
    }
}

void fb_glitch(Frame *f, int seed, double amount, Rgb c)
{
    if (amount <= 0) return;
    for (int y = 0; y < f->h; y++) {
        double r = mv_noise2(y, seed, 5);
        if (r > amount * 0.9) continue;
        int shift = (int)lround((mv_noise2(y, seed, 11) - 0.5) * 12.0 * amount);
        if (shift == 0) continue;
        for (int x = 0; x < f->w; x++) {
            int sx = x - shift;
            Cell *dst = &f->cells[y * f->w + x];
            if (sx < 0 || sx >= f->w) {
                dst->ch = 0;
                continue;
            }
            Cell src = f->cells[y * f->w + sx];
            src.fg = rgb_mix(src.fg, c, 0.35);
            *dst = src;
        }
    }
    /* Random corrupt cells keep it from looking like a clean slide. */
    int hits = (int)(amount * f->w * f->h * 0.03);
    for (int i = 0; i < hits; i++) {
        int x = (int)(mv_rand01((uint32_t)(seed * 31 + i)) * f->w);
        int y = (int)(mv_rand01((uint32_t)(seed * 17 + i * 7 + 3)) * f->h);
        fb_put(f, x, y, mv_char_for(mv_rand01((uint32_t)(i * 13 + seed))), c);
    }
}

void fb_shift(Frame *f, int dx, int dy)
{
    if (!dx && !dy) return;
    Frame tmp;
    frame_alloc(&tmp, f->w, f->h);
    frame_clear(&tmp, RGB(0, 0, 0));
    for (int y = 0; y < f->h; y++) {
        int sy = y - dy;
        if (sy < 0 || sy >= f->h) continue;
        for (int x = 0; x < f->w; x++) {
            int sx = x - dx;
            if (sx < 0 || sx >= f->w) continue;
            tmp.cells[y * f->w + x] = f->cells[sy * f->w + sx];
        }
    }
    memcpy(f->cells, tmp.cells, (size_t)f->w * (size_t)f->h * sizeof(Cell));
    frame_free(&tmp);
}

/* --------------------------------------------------------------- transitions */

/* Weight map in [0,1]: how much of the incoming frame b to take at cell x,y. */
static double blend_weight(int x, int y, int w, int h, double t, int mode, int seed)
{
    double nx = w > 1 ? (double)x / (w - 1) : 0.0;
    double ny = h > 1 ? (double)y / (h - 1) : 0.0;
    double n = mv_noise2(x, y, seed + 7);
    double cx = (w - 1) / 2.0, cy = (h - 1) / 2.0;
    double dx = (x - cx) / (cx > 0 ? cx : 1), dy = (y - cy) / (cy > 0 ? cy : 1);
    double r = sqrt(dx * dx + dy * dy) / 1.4142;

    switch (mode) {
    case TR_WIPE_L:  return clampd(t * 1.25 - nx + (n - 0.5) * 0.10, 0.0, 1.0);
    case TR_WIPE_R:  return clampd(t * 1.25 - (1.0 - nx) + (n - 0.5) * 0.10, 0.0, 1.0);
    case TR_WIPE_U:  return clampd(t * 1.25 - ny + (n - 0.5) * 0.10, 0.0, 1.0);
    case TR_WIPE_D:  return clampd(t * 1.25 - (1.0 - ny) + (n - 0.5) * 0.10, 0.0, 1.0);
    case TR_RADIAL_IN:  return clampd(t * 1.25 - r + (n - 0.5) * 0.10, 0.0, 1.0);
    case TR_RADIAL_OUT: return clampd(t * 1.25 - (1.0 - r) + (n - 0.5) * 0.10, 0.0, 1.0);
    case TR_BLINDS: {
        int band = y / 2, parity = band % 2;
        double local = parity ? (t - 0.5) * 2.0 : t * 2.0;
        double edge = (y % 2) ? 0.15 : 0.0;
        return clampd(local - edge + (n - 0.5) * 0.05, 0.0, 1.0);
    }
    case TR_GLITCH: {
        double band = (double)(y / 3);
        double phase = fmod(band * 0.37 + (double)seed * 0.11, 1.0);
        return clampd(t * 1.6 - phase + (n - 0.5) * 0.5, 0.0, 1.0);
    }
    case TR_FLASH_W:
    case TR_FLASH_B:
        return t > 0.45 ? 1.0 : 0.0;
    case TR_SLIDE_L:
        return clampd((t * 1.0 - nx) * 4.0 + 0.5, 0.0, 1.0);
    case TR_BURN:
    case TR_DISSOLVE:
    default:
        return clampd((t - 0.18) / 0.64 - n * 0.5 + 0.25, 0.0, 1.0);
    }
}

void fb_blend(Frame *dst, const Frame *a, const Frame *b, double t, int mode, int seed)
{
    t = clampd(t, 0.0, 1.0);
    double flash = 0.0;
    Rgb flash_col = RGB(255, 255, 255);
    if (mode == TR_FLASH_W || mode == TR_FLASH_B) {
        flash = sin(M_PI * clampd(t, 0, 1));
        flash_col = (mode == TR_FLASH_W) ? RGB(255, 255, 255) : RGB(0, 0, 0);
    }
    /* A slight dip in brightness mid-transition makes the per-cell flip read as
     * a soft dissolve instead of a hard cut. */
    double dip = 1.0 - 0.30 * sin(M_PI * t);
    if (flash > 0) dip = 1.0;

    int slide = (mode == TR_SLIDE_L) ? (int)lround((1.0 - t) * dst->w) : 0;
    double maxr = 0.0;
    if (flash > 0.001) {
        double cx0 = (dst->w - 1) / 2.0, cy0 = (dst->h - 1) / 2.0;
        maxr = sqrt(cx0 * cx0 + cy0 * cy0);
    }

    for (int y = 0; y < dst->h; y++) {
        for (int x = 0; x < dst->w; x++) {
            const Cell *ca = &a->cells[y * a->w + x];
            const Cell *cb = &b->cells[y * b->w + x];
            if (slide) {
                int sx = x - slide - dst->w;
                if (sx < 0) sx += dst->w * 2;
                if (sx >= dst->w) sx -= dst->w * 2;
                if (sx < 0 || sx >= dst->w) sx = x;
                cb = &b->cells[y * b->w + sx];
            }
            double w = blend_weight(x, y, dst->w, dst->h, t, mode, seed);
            int take_b = (w >= 0.5);
            const Cell *src = take_b ? cb : ca;
            Cell out = *src;
            /* Blend the colour a little as well, so edges melt. */
            double k = dip;
            out.fg = rgb_scale(out.fg, k);
            if (out.has_bg) out.bg = rgb_scale(out.bg, k);
            if (!out.ch) out.ch = ' ';
            if (flash > 0.001) {
                /* Not a flat wall of white: the flash is brightest in the
                 * middle, so the eye keeps some structure to hold on to. */
                double dx = x - (dst->w - 1) / 2.0;
                double dy = y - (dst->h - 1) / 2.0;
                double rr = sqrt(dx * dx + dy * dy) / (maxr > 0 ? maxr : 1.0);
                double amt = flash * 0.66 * (1.0 - 0.35 * rr);
                Rgb base = out.ch == ' ' ? RGB(0, 0, 0) : out.fg;
                out.fg = rgb_mix(base, flash_col, amt);
                if (out.ch == ' ' && amt > 0.3) out.ch = mv_char_for(amt * 0.5);
            }
            dst->cells[y * dst->w + x] = out;
        }
    }
}