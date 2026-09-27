/* fx_util.h -- tiny helpers shared by the effect implementations.
 * Everything here is a pure function of the Ctx, so an effect can be rendered at
 * any playback time (needed for the offline exporter and for transitions). */
#ifndef MV_FX_UTIL_H
#define MV_FX_UTIL_H

#include <math.h>
#include "mv.h"
#include "scenes.h"

static inline int fx_clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Smooth 0..1 ramp over the first `secs` of a scene. */
static inline double fx_enter(const Ctx *c, double secs)
{
    return secs <= 0 ? 1.0 : mv_ease_out(c->t / secs);
}

/* 1.0 until the last `secs`, then fades to 0. */
static inline double fx_fadeout(const Ctx *c, double secs)
{
    double remain = c->dur - c->t;
    return remain >= secs ? 1.0 : mv_clamp01(remain / (secs > 0 ? secs : 1));
}

/* Decaying hit on every beat; `decay` ~ 3 (slow) .. 12 (snappy). */
static inline double fx_beat(const Ctx *c, double decay)
{
    return exp(-c->phase * decay);
}

static inline Rgb fx_col(const Ctx *c, double t)
{
    return rgb_mix(c->sc->c1, c->sc->c2, mv_clamp01(t));
}

/* Random integer in [0,n) that is stable for a given index. */
static inline int fx_randi(uint32_t seed, int n)
{
    return n > 0 ? (int)(mv_rand01(seed) * n) % n : 0;
}

static inline double fx_rand(uint32_t seed)
{
    return mv_rand01(seed);
}

/* A soft horizontal "screen line" with a bright core, used by several scenes. */
static inline void fx_glow_line(Frame *f, int y, Rgb col, double k)
{
    if (y < 0 || y >= f->h) return;
    for (int x = 0; x < f->w; x++) {
        double d = mv_noise2(x, y, 3);
        if (d < 0.55) continue;
        fb_put(f, x, y, d > 0.85 ? '=' : '-', rgb_scale(col, k * (0.5 + d * 0.5)));
    }
}

/* Largest sprite scale that keeps a sprite inside the given share of the
 * screen (frac_w/frac_h are fractions of the frame size). */
static inline double fx_fit_scale(const Ctx *c, const char *name, double frac_w,
                                  double frac_h)
{
    int sw = 0, sh = 0;
    if (!fb_sprite_size(name, &sw, &sh) || sw <= 0 || sh <= 0) return 1.0;
    double s = fmin((c->w * frac_w) / sw, (c->h * frac_h) / sh);
    return s < 0.5 ? 0.5 : s;
}

#endif /* MV_FX_UTIL_H */