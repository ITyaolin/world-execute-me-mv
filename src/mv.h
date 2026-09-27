/* mv.h -- shared types and configuration for the world.execute(me) ASCII MV.
 *
 * Layout of the program:
 *   main.c    CLI, timing loop, export/dump modes
 *   term.c    terminal raw mode, size, colour quantisation, diff rendering
 *   gfx.c     cell framebuffer + drawing primitives (text, banner, sprite, ...)
 *   lyrics.c  karaoke overlay and keyword "banner" typography
 *   scenes.c  the scene (movement) table and every visual effect
 *   audio.c   optional mp3 playback, kept in sync with the clock
 */
#ifndef MV_H
#define MV_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lyrics_data.h"

/* ------------------------------------------------------------------ colours */

typedef struct { uint8_t r, g, b; } Rgb;

#define RGB(R, G, B) ((Rgb){ (uint8_t)(R), (uint8_t)(G), (uint8_t)(B) })

Rgb  rgb_scale(Rgb c, double k);
Rgb  rgb_mix(Rgb a, Rgb b, double t);
Rgb  rgb_hsv(double h, double s, double v);

/* ----------------------------------------------------------------- framebuffer */
/* One Cell is one terminal character cell.  ch == 0 means "blank". */

typedef struct {
    unsigned char ch;
    Rgb           fg;
    Rgb           bg;
    unsigned char has_bg;
} Cell;

typedef struct {
    int   w, h;
    Cell *cells;
} Frame;

void frame_alloc(Frame *f, int w, int h);
void frame_free(Frame *f);
void frame_clear(Frame *f, Rgb fg);
void frame_set_bg(Frame *f, Rgb bg);
/* Mark every cell as unknown so the next present repaints all of it. */
void frame_invalidate(Frame *f);

/* ------------------------------------------------------------------ gfx API */

enum { COLOR_MONO = 0, COLOR_16, COLOR_256, COLOR_TRUECOLOR };

/* Transition styles available to the scene table. */
enum {
    TR_DISSOLVE = 0, TR_WIPE_L, TR_WIPE_R, TR_WIPE_U, TR_WIPE_D,
    TR_RADIAL_IN, TR_RADIAL_OUT, TR_GLITCH, TR_FLASH_W, TR_FLASH_B,
    TR_SLIDE_L, TR_BLINDS, TR_BURN, TR_COUNT
};
#define TR_DEFAULT TR_DISSOLVE

void fb_put(Frame *f, int x, int y, unsigned char ch, Rgb fg);
void fb_put_bg(Frame *f, int x, int y, unsigned char ch, Rgb fg, Rgb bg);
void fb_text(Frame *f, int x, int y, const char *s, Rgb fg);
void fb_text_center(Frame *f, int y, const char *s, Rgb fg);
int  fb_text_width(const char *s);

/* Big "banner" typography: samples the baked 8x16 font into a cell_h x cell_h
 * block per glyph, which keeps letters in proportion on a 1:2 cell grid. */
int  fb_banner_width(int len, int cell_h);
int  fb_banner(Frame *f, int cx, int cy, const char *s, int cell_h, Rgb fg,
               double reveal, unsigned char ink, int shadow);
void fb_banner_fit(Frame *f, int cx, int cy, const char *s, int max_h, int max_w,
                   Rgb fg, double reveal, unsigned char ink, int shadow);

void fb_line(Frame *f, double x0, double y0, double x1, double y1,
             unsigned char ch, Rgb fg);
void fb_rect(Frame *f, int x, int y, int w, int h, unsigned char ch, Rgb fg);
void fb_circle(Frame *f, double cx, double cy, double r, unsigned char ch, Rgb fg,
               int filled, double arc0, double arc1);
void fb_ellipse(Frame *f, double cx, double cy, double rx, double ry, unsigned char ch,
                Rgb fg, int filled, double arc0, double arc1);

void fb_sprite(Frame *f, const char *name, int x, int y, Rgb dim, Rgb bright,
               double gain, double reveal);
void fb_sprite_center(Frame *f, const char *name, int cx, int cy, double scale,
                      Rgb dim, Rgb bright, double gain, double reveal);
int  fb_sprite_size(const char *name, int *w, int *h);

/* Post-processing passes. */
/* Push shadows down and highlights up: without this the mid-tones of the
 * artwork and of the background texture sit at the same brightness. */
void fb_contrast(Frame *f, double amount);
/* Dim whatever is already drawn inside an ellipse (a spotlight backdrop for
 * the subject, so it separates from the surrounding texture). */
void fb_scrim_ellipse(Frame *f, double cx, double cy, double rx, double ry, double k);
void fb_shade(Frame *f, double k);
/* Dim a rectangle of cells (used to keep text readable over busy artwork). */
void fb_dim_rect(Frame *f, int x, int y, int w, int h, double k);
void fb_tint(Frame *f, Rgb c, double k);
void fb_scanlines(Frame *f, double amount);
void fb_vignette(Frame *f, double amount);
void fb_grain(Frame *f, int seed, double amount, Rgb c);
void fb_glitch(Frame *f, int seed, double amount, Rgb c);
void fb_shift(Frame *f, int dx, int dy);

/* Transition compositor: writes a blend of a (weight 1-t) and b (weight t) into
 * dst.  mode is one of the TR_* values in scenes.h. */
void fb_blend(Frame *dst, const Frame *a, const Frame *b, double t, int mode, int seed);

/* Deterministic pseudo random helpers -- effects must be stateless functions of
 * time so that transitions and exports always look identical. */
uint32_t mv_hash(uint32_t x);
double   mv_rand01(uint32_t x);
double   mv_noise2(int x, int y, int seed);

double   mv_density(unsigned char ch);
unsigned char mv_char_for(double d);
extern const char MV_RAMP[];   /* " .:-=+*#%@" */

static inline double mv_clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline double mv_clampd(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline double mv_ease_out(double v)   /* decelerate */
{
    v = mv_clamp01(v);
    return 1.0 - (1.0 - v) * (1.0 - v) * (1.0 - v);
}
static inline double mv_ease_in(double v)
{
    v = mv_clamp01(v);
    return v * v * v;
}
static inline double mv_smooth(double v)     /* smoothstep */
{
    v = mv_clamp01(v);
    return v * v * (3.0 - 2.0 * v);
}
static inline double mv_pulse(double turns)  /* 0..1 sine, turns in cycles */
{
    return 0.5 - 0.5 * cos(turns * 2.0 * 3.14159265358979);
}
static inline double mv_lerp(double a, double b, double t) { return a + (b - a) * t; }

/* ------------------------------------------------------------------- options */

typedef struct {
    const char *audio_path;
    const char *player;        /* force a specific player binary          */
    bool        no_audio;
    bool        loop;
    int         offset_ms;     /* shift the whole show later (+) / earlier (-) */
    int         start_ms;
    int         duration_ms;   /* 0 = the whole song                      */
    int         audio_lead_ms; /* wait this long after spawning the player */
    int         fps;
    int         color_mode;    /* -1 = auto                               */
    bool        unicode;
    bool        karaoke;
    bool        banner;
    bool        grain;
    bool        altscreen;
    bool        hud;
    int         force_w, force_h;
    bool        quiet;
    bool        canvas_solid;  /* paint our own background (default on)   */
    Rgb         canvas_bg;     /* colour of that background               */
    double      contrast;      /* 0 = off, 1 = full tone map             */

    /* Offline modes. */
    const char *export_dir;
    int         export_from, export_to, export_step;
    int         export_w, export_h;
    long        dump_ms;       /* >=0: print one frame as text and exit */
    bool        list_scenes;
    bool        selftest;
} Config;

extern Config CFG;

extern const char *PROG_NAME;

/* -------------------------------------------------------------------- misc */

double mv_now(void);            /* monotonic seconds */
void   mv_sleep(double seconds);
int    mv_term_width(void);
int    mv_term_height(void);

#endif /* MV_H */