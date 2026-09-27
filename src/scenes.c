/* scenes.c -- the scene table, its resolution against the lyric timeline, the
 * effect dispatch and the transition compositor.
 *
 * Each scene is anchored to a lyric line index, so the picture always changes on
 * the words, and scene N automatically ends where scene N+1 begins.  Transitions
 * are rendered by drawing both scenes and running the blend compositor, which is
 * what keeps the cuts from feeling like hard jumps.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mv.h"
#include "scenes.h"
#include "fx_impl.h"
#include "fx_util.h"

/* ---------------------------------------------------------------- palettes */
/* Two spellings of every colour: C_* is an expression (compound literal) and
 * CI_* is a brace initialiser, so the scene table stays a constant expression
 * even under -Wpedantic. */
#define C_WHITE_R    235, 245, 255
#define C_CYAN_R      80, 230, 255
#define C_GREEN_R     90, 255, 130
#define C_TEAL_R      60, 220, 190
#define C_AZURE_R    120, 160, 255
#define C_MAGENTA_R  255,  90, 210
#define C_PINK_R     255, 130, 180
#define C_RED_R      255,  70,  60
#define C_ORANGE_R   255, 150,  60
#define C_AMBER_R    255, 200,  90
#define C_YELLOW_R   255, 235, 120
#define C_GOLD_R     255, 215, 120
#define C_VIOLET_R   170, 120, 255
#define C_PURPLE_R   150,  80, 220

#define C_BLACK    RGB(0, 0, 0)
#define C_WHITE    RGB(C_WHITE_R)
#define C_CYAN     RGB(C_CYAN_R)
#define C_GREEN    RGB(C_GREEN_R)
#define C_TEAL     RGB(C_TEAL_R)
#define C_AZURE    RGB(C_AZURE_R)
#define C_MAGENTA  RGB(C_MAGENTA_R)
#define C_PINK     RGB(C_PINK_R)
#define C_RED      RGB(C_RED_R)
#define C_ORANGE   RGB(C_ORANGE_R)
#define C_AMBER    RGB(C_AMBER_R)
#define C_YELLOW   RGB(C_YELLOW_R)
#define C_GOLD     RGB(C_GOLD_R)
#define C_VIOLET   RGB(C_VIOLET_R)
#define C_PURPLE   RGB(C_PURPLE_R)

#define CI_WHITE    { C_WHITE_R }
#define CI_CYAN     { C_CYAN_R }
#define CI_GREEN    { C_GREEN_R }
#define CI_TEAL     { C_TEAL_R }
#define CI_AZURE    { C_AZURE_R }
#define CI_MAGENTA  { C_MAGENTA_R }
#define CI_PINK     { C_PINK_R }
#define CI_RED      { C_RED_R }
#define CI_ORANGE   { C_ORANGE_R }
#define CI_AMBER    { C_AMBER_R }
#define CI_YELLOW   { C_YELLOW_R }
#define CI_GOLD     { C_GOLD_R }
#define CI_VIOLET   { C_VIOLET_R }
#define CI_PURPLE   { C_PURPLE_R }

/* ------------------------------------------------------------ scene table  */

#define SC(LINE, LEAD, FX, C1, C2, TR, TRMS, SEED, INT) \
    { LINE, LEAD, FX, NULL, C1, C2, TR, TRMS, SEED, INT, 1.0f, 0.4f, NULL }
#define SCS(LINE, LEAD, FX, SPR, C1, C2, TR, TRMS, SEED, INT, P1, P2) \
    { LINE, LEAD, FX, SPR, C1, C2, TR, TRMS, SEED, INT, P1, P2, NULL }
#define SCP(LINE, LEAD, FX, C1, C2, TR, TRMS, SEED, INT, P1, P2, CAP) \
    { LINE, LEAD, FX, NULL, C1, C2, TR, TRMS, SEED, INT, P1, P2, CAP }

const Scene SCENES[] = {
/*    line  lead   effect          palette          transition          ms   seed int   params      caption */
    SC (  0,     0, FX_POWERLINE,  CI_CYAN,   CI_GREEN,   TR_DISSOLVE,  900,  11, 1),
    SC (  1,  -200, FX_SHIELD,     CI_CYAN,   CI_WHITE,   TR_RADIAL_OUT,700,  12, 1),
    SC (  3,  -150, FX_BLOCKS,     CI_AMBER,  CI_ORANGE,  TR_WIPE_L,    800,  13, 1),
    SC (  6,  -200, FX_DATASTREAM, CI_GREEN,  CI_CYAN,    TR_GLITCH,    500,  14, 1),
    SC (  8,  -100, FX_GLOBE,      CI_TEAL,   CI_AZURE,   TR_RADIAL_IN, 900,  15, 1),
    SC ( 10,  -250, FX_GRIDWORLD,  CI_MAGENTA,CI_CYAN,    TR_WIPE_U,    700,  16, 2),
    SC ( 11, -3100, FX_CODE,       CI_GREEN,  CI_CYAN,    TR_FLASH_W,   350,  17, 2),
    SC ( 12,  -400, FX_POINTS,     CI_AZURE,  CI_WHITE,   TR_DISSOLVE,  800,  18, 1),
    SC ( 15,  -250, FX_CIRCLE,     CI_AZURE,  CI_CYAN,    TR_RADIAL_IN, 700,  19, 1),
    SC ( 18,  -250, FX_SINE,       CI_TEAL,   CI_WHITE,   TR_WIPE_R,    700,  20, 1),
    SC ( 21,  -250, FX_INFINITY,   CI_VIOLET, CI_WHITE,   TR_DISSOLVE,  800,  21, 1),
    SC ( 24,  -200, FX_WAVE,       CI_AMBER,  CI_CYAN,    TR_BLINDS,    600,  22, 1),
    SC ( 26,  -250, FX_BLIND,      CI_WHITE,  CI_AMBER,   TR_GLITCH,    400,  23, 1),
    SC ( 27,  -200, FX_SPIN,       CI_MAGENTA,CI_WHITE,   TR_RADIAL_IN, 450,  24, 2),
    SC ( 28,  -200, FX_STARS,      CI_AZURE,  CI_WHITE,   TR_WIPE_R,    600,  25, 1),
    SC ( 29,  -250, FX_TIMELINE,   CI_AZURE,  CI_AMBER,   TR_DISSOLVE,  700,  26, 1),
    SC ( 30,  -250, FX_MERGE,      CI_PINK,   CI_MAGENTA, TR_RADIAL_OUT,700,  27, 1),
    SC ( 31,  -250, FX_TUNNEL,     CI_VIOLET, CI_CYAN,    TR_GLITCH,    500,  28, 1),
    SC ( 32,  -300, FX_PULSE,      CI_MAGENTA,CI_WHITE,   TR_FLASH_W,   300,  29, 1),
    SC ( 35,  -500, FX_HEART,      CI_PINK,   CI_RED,     TR_FLASH_W,   250,  30, 1),
    SC ( 38,  -300, FX_SMILE,      CI_AMBER,  CI_YELLOW,  TR_WIPE_D,    600,  31, 1),
    SC ( 41,  -300, FX_CAGE,       CI_RED,    CI_ORANGE,  TR_BLINDS,    600,  32, 1),
    SCS( 44,  -300, FX_SPRITE,  "eggplant", CI_GREEN, CI_PURPLE, TR_WIPE_L,   700, 33, 1, 1.15f, 0.6f),
    SCS( 47,  -300, FX_SPRITE,  "tomato",   CI_RED,   CI_GREEN,  TR_WIPE_R,   700, 34, 1, 1.15f, 0.6f),
    SC ( 50,  -300, FX_CAT,        CI_AMBER,  CI_YELLOW,  TR_DISSOLVE,  700,  35, 1),
    SC ( 53,  -300, FX_GOD,        CI_GOLD,   CI_WHITE,   TR_FLASH_W,   350,  36, 2),
    SC ( 56,  -250, FX_GENDER,     CI_PINK,   CI_AZURE,   TR_WIPE_L,    600,  37, 1),
    SCP( 58,  -300, FX_MORPH,      CI_VIOLET, CI_ORANGE,  TR_GLITCH,    350,  38, 1, 2.0f, 0.0f, "do whatever"),
    SC ( 59,  -250, FX_CLOCK,      CI_AMBER,  CI_AZURE,   TR_DISSOLVE,  700,  39, 1),
    SC ( 60,  -250, FX_CHAIN,      CI_RED,    CI_VIOLET,  TR_WIPE_L,    600,  40, 1),
    SC ( 62,  -250, FX_TRANCE,     CI_VIOLET, CI_MAGENTA, TR_RADIAL_IN, 800,  41, 2),
    SC ( 64,  -300, FX_VIBRATE,    CI_CYAN,   CI_MAGENTA, TR_WIPE_U,    600,  42, 1),
    SC ( 67,  -400, FX_COMPLETE,   CI_TEAL,   CI_GREEN,   TR_RADIAL_OUT,800,  43, 1),
    SC ( 70,  -300, FX_DISSOLVE,   CI_AZURE,  CI_WHITE,   TR_DISSOLVE, 1200,  44, 1),
    SC ( 76,  -250, FX_VOID,       CI_AZURE,  CI_WHITE,   TR_FLASH_B,   500,  45, 1),
    SC ( 77,  -300, FX_SHATTER,    CI_VIOLET, CI_MAGENTA, TR_GLITCH,    400,  46, 2),
    SC ( 80,  -300, FX_LONELY,     CI_VIOLET, CI_PINK,    TR_DISSOLVE,  900,  47, 1),
    SC ( 83,  -300, FX_ALERT,      CI_RED,    CI_ORANGE,  TR_GLITCH,    400,  48, 2),
    SC ( 85, 3300,  FX_BUILDUP,    CI_RED,    CI_WHITE,   TR_DISSOLVE, 1000,  49, 3),
    SC ( 86,     0, FX_EXECSTORM,  CI_RED,    CI_WHITE,   TR_FLASH_W,   200,  50, 3),
    SC ( 98,  -200, FX_COUNT,      CI_RED,    CI_WHITE,   TR_FLASH_W,   200,  51, 3),
    SC (104,     0, FX_EXECSTORM,  CI_RED,    CI_WHITE,   TR_FLASH_W,   250,  52, 2),
    SC (114,  -300, FX_CAGE,       CI_RED,    CI_ORANGE,  TR_BLINDS,    600,  53, 1),
    SCP(116,  -300, FX_FORMULA,    CI_PINK,   CI_MAGENTA, TR_DISSOLVE,  900,  54, 1, 0.0f, 0.0f, "studied LOVE"),
    SCP(119,  -300, FX_FORMULA,    CI_PINK,   CI_VIOLET,  TR_WIPE_L,    700,  55, 1, 1.0f, 0.0f, "question me"),
    SCP(122,  -300, FX_FORMULA,    CI_MAGENTA,CI_WHITE,   TR_DISSOLVE,  800,  56, 1, 2.0f, 0.0f, "algebra of LOVE"),
    SC (124,  -300, FX_HEART_CAGE, CI_PINK,   CI_RED,     TR_RADIAL_IN, 800,  57, 1),
    SC (127,  2000, FX_OUTRO,      CI_AZURE,  CI_GREEN,   TR_DISSOLVE, 1500,  58, 1),
    SC (128,  -300, FX_END,        CI_WHITE,  CI_CYAN,    TR_FLASH_B,   900,  59, 1),
};

const int SCENE_COUNT = (int)(sizeof SCENES / sizeof SCENES[0]);

static int s_start[sizeof SCENES / sizeof SCENES[0]];
static int s_end[sizeof SCENES / sizeof SCENES[0]];
static int s_resolved = 0;

void scene_resolve(void)
{
    if (s_resolved) return;
    for (int i = 0; i < SCENE_COUNT; i++) {
        int line = SCENES[i].line;
        if (line < 0) line = 0;
        if (line >= LYRIC_LINE_COUNT) line = LYRIC_LINE_COUNT - 1;
        int start = LYRIC_LINES[line].start_ms + SCENES[i].lead_ms;
        if (start < 0) start = 0;
        if (i > 0 && start < s_start[i - 1] + 200) start = s_start[i - 1] + 200;
        s_start[i] = start;
    }
    for (int i = 0; i < SCENE_COUNT; i++)
        s_end[i] = (i + 1 < SCENE_COUNT) ? s_start[i + 1] : LYRIC_TOTAL_MS;
    s_resolved = 1;
}

int scene_start_ms(int idx)
{
    scene_resolve();
    return (idx >= 0 && idx < SCENE_COUNT) ? s_start[idx] : 0;
}

int scene_end_ms(int idx)
{
    scene_resolve();
    return (idx >= 0 && idx < SCENE_COUNT) ? s_end[idx] : 0;
}

int scene_duration_ms(int idx)
{
    return scene_end_ms(idx) - scene_start_ms(idx);
}

int scene_at(int t_ms)
{
    scene_resolve();
    int best = -1;
    for (int i = 0; i < SCENE_COUNT; i++) {
        if (s_start[i] <= t_ms) best = i;
        else break;
    }
    return best;
}

/* --------------------------------------------------------------- utilities */

void fx_panel(Frame *f, int x, int y, int w, int h, const char *title, Rgb border,
              Rgb title_col)
{
    if (w < 4 || h < 3) return;
    for (int i = 0; i < w; i++) {
        fb_put(f, x + i, y, '-', border);
        fb_put(f, x + i, y + h - 1, '-', border);
    }
    for (int j = 0; j < h; j++) {
        fb_put(f, x, y + j, '|', border);
        fb_put(f, x + w - 1, y + j, '|', border);
    }
    fb_put(f, x, y, '+', border);
    fb_put(f, x + w - 1, y, '+', border);
    fb_put(f, x, y + h - 1, '+', border);
    fb_put(f, x + w - 1, y + h - 1, '+', border);
    if (title && *title) {
        int tl = (int)strlen(title);
        if (tl + 4 < w) {
            int tx = x + (w - tl) / 2;
            fb_put(f, tx - 2, y, '[', border);
            fb_text(f, tx, y, title, title_col);
            fb_put(f, tx + tl + 1, y, ']', border);
        }
    }
}

void fx_scan_dots(Frame *f, Rgb col, double density, int seed)
{
    for (int y = 0; y < f->h; y++)
        for (int x = 0; x < f->w; x++)
            if (mv_noise2(x, y, seed) < density)
                fb_put(f, x, y, mv_noise2(x, y, seed + 1) > 0.5 ? '.' : ':', col);
}

/* ---------------------------------------------------------------- dispatch */

static void render_one(Frame *f, int idx, int t_ms)
{
    scene_resolve();
    const Scene *sc = &SCENES[idx];
    Ctx c;
    c.f = f;
    c.sc = sc;
    c.t_ms = t_ms;
    c.t = (t_ms - s_start[idx]) / 1000.0;
    c.dur = (s_end[idx] - s_start[idx]) / 1000.0;
    if (c.dur < 0.05) c.dur = 0.05;
    c.prog = mv_clamp01(c.t / c.dur);
    double beatf = (double)t_ms * LYRIC_BPM / 60000.0;
    c.beat = beatf;
    c.phase = beatf - floor(beatf);
    c.w = f->w;
    c.h = f->h;
    c.rnd = mv_hash((uint32_t)(sc->seed * 2654435761u + (uint32_t)idx));
    frame_clear(f, C_BLACK);
    switch (sc->fx) {
    case FX_POWERLINE:  fx_powerline(&c);  break;
    case FX_SHIELD:     fx_shield(&c);     break;
    case FX_BLOCKS:     fx_blocks(&c);     break;
    case FX_DATASTREAM: fx_datastream(&c); break;
    case FX_GLOBE:      fx_globe(&c);      break;
    case FX_GRIDWORLD:  fx_gridworld(&c);  break;
    case FX_CODE:       fx_code(&c);       break;
    case FX_POINTS:     fx_points(&c);     break;
    case FX_CIRCLE:     fx_circle(&c);     break;
    case FX_SINE:       fx_sine(&c);       break;
    case FX_INFINITY:   fx_infinity(&c);   break;
    case FX_WAVE:       fx_wave(&c);       break;
    case FX_BLIND:      fx_blind(&c);      break;
    case FX_SPIN:       fx_spin(&c);       break;
    case FX_STARS:      fx_stars(&c);      break;
    case FX_TIMELINE:   fx_timeline(&c);   break;
    case FX_MERGE:      fx_merge(&c);      break;
    case FX_TUNNEL:     fx_tunnel(&c);     break;
    case FX_PULSE:      fx_pulse(&c);      break;
    case FX_HEART:      fx_heart(&c);      break;
    case FX_SMILE:      fx_smile(&c);      break;
    case FX_CAGE:       fx_cage(&c);       break;
    case FX_SPRITE:     fx_sprite(&c);     break;
    case FX_CAT:        fx_cat(&c);        break;
    case FX_GOD:        fx_god(&c);        break;
    case FX_GENDER:     fx_gender(&c);     break;
    case FX_MORPH:      fx_morph(&c);      break;
    case FX_CLOCK:      fx_clock(&c);      break;
    case FX_CHAIN:      fx_chain(&c);      break;
    case FX_TRANCE:     fx_trance(&c);     break;
    case FX_VIBRATE:    fx_vibrate(&c);    break;
    case FX_COMPLETE:   fx_complete(&c);   break;
    case FX_DISSOLVE:   fx_dissolve(&c);   break;
    case FX_VOID:       fx_void(&c);       break;
    case FX_SHATTER:    fx_shatter(&c);    break;
    case FX_LONELY:     fx_lonely(&c);     break;
    case FX_ALERT:      fx_alert(&c);      break;
    case FX_BUILDUP:    fx_buildup(&c);    break;
    case FX_EXECSTORM:  fx_execstorm(&c);  break;
    case FX_COUNT:      fx_count(&c);      break;
    case FX_FORMULA:    fx_formula(&c);    break;
    case FX_HEART_CAGE: fx_heart_cage(&c); break;
    case FX_OUTRO:      fx_outro(&c);      break;
    case FX_END:        fx_end(&c);        break;
    default: break;
    }
}

void scene_render(Frame *f, int idx, int t_ms)
{
    if (idx < 0 || idx >= SCENE_COUNT) {
        frame_clear(f, C_BLACK);
        return;
    }
    render_one(f, idx, t_ms);
}

/* Full composition for a moment in time: the incoming scene, the outgoing one
 * and the transition between them.  out/tmp_new/tmp_old must be distinct frames
 * of the same size. */
void scene_render_at(Frame *out, Frame *tmp_new, Frame *tmp_old, int t_ms)
{
    scene_resolve();
    int idx = scene_at(t_ms);
    if (idx < 0) {
        frame_clear(out, C_BLACK);
        return;
    }
    int start = s_start[idx];
    int trms = SCENES[idx].trans_ms;
    if (idx > 0 && trms > 0 && t_ms < start + trms) {
        double t = (double)(t_ms - start) / (double)trms;
        if (t < 0) t = 0;
        render_one(tmp_new, idx, t_ms);
        render_one(tmp_old, idx - 1, t_ms);
        fb_blend(out, tmp_old, tmp_new, t, SCENES[idx].trans, SCENES[idx].seed);
        return;
    }
    render_one(out, idx, t_ms);
}

void scene_list(FILE *out)
{
    scene_resolve();
    fprintf(out, "  #  start     end   dur  effect        transition   line  caption\n");
    for (int i = 0; i < SCENE_COUNT; i++) {
        const char *cap = SCENES[i].caption ? SCENES[i].caption : "";
        fprintf(out, "%3d  %6.2fs %6.2fs %5.2fs  %-12s  %-11s  %4d  %s\n", i,
                s_start[i] / 1000.0, s_end[i] / 1000.0,
                (s_end[i] - s_start[i]) / 1000.0,
                fx_name(SCENES[i].fx), tr_name(SCENES[i].trans), SCENES[i].line, cap);
    }
}

const char *fx_name(Fx fx)
{
    static const char *names[] = {
        "powerline", "shield", "blocks", "datastream", "globe", "gridworld",
        "code", "points", "circle", "sine", "infinity", "wave", "blind", "spin",
        "stars", "timeline", "merge", "tunnel", "pulse", "heart", "smile", "cage",
        "sprite", "cat", "god", "gender", "morph", "clock", "chain", "trance",
        "vibrate", "complete", "dissolve", "void", "shatter", "lonely", "alert",
        "buildup", "execstorm", "count", "formula", "heart_cage", "outro", "end"
    };
    if ((int)fx < 0 || (int)fx >= (int)(sizeof names / sizeof names[0])) return "?";
    return names[(int)fx];
}

const char *tr_name(int tr)
{
    static const char *names[] = {
        "dissolve", "wipe-left", "wipe-right", "wipe-up", "wipe-down",
        "iris-in", "iris-out", "glitch", "flash-white", "flash-black",
        "slide-left", "blinds", "burn"
    };
    if (tr < 0 || tr >= (int)(sizeof names / sizeof names[0])) return "?";
    return names[tr];
}