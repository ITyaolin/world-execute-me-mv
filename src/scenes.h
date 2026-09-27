/* scenes.h -- the scene ("movement") table and the effect interface.
 *
 * Every scene is anchored to a lyric line index from the generated
 * src/lyrics_data.h, plus a millisecond lead.  Scene N runs until scene N+1
 * starts, so the timeline is contiguous by construction and the visuals can
 * never drift away from the words.
 */
#ifndef MV_SCENES_H
#define MV_SCENES_H

#include "mv.h"

typedef enum {
    FX_POWERLINE = 0, FX_SHIELD, FX_BLOCKS, FX_DATASTREAM, FX_GLOBE, FX_GRIDWORLD,
    FX_CODE, FX_POINTS, FX_CIRCLE, FX_SINE, FX_INFINITY, FX_WAVE, FX_BLIND,
    FX_SPIN, FX_STARS, FX_TIMELINE, FX_MERGE, FX_TUNNEL, FX_PULSE, FX_HEART,
    FX_SMILE, FX_CAGE, FX_SPRITE, FX_CAT, FX_GOD, FX_GENDER, FX_MORPH, FX_CLOCK,
    FX_CHAIN, FX_TRANCE, FX_VIBRATE, FX_COMPLETE, FX_DISSOLVE, FX_VOID,
    FX_SHATTER, FX_LONELY, FX_ALERT, FX_BUILDUP, FX_EXECSTORM, FX_COUNT,
    FX_FORMULA, FX_HEART_CAGE, FX_OUTRO, FX_END,
    FX_COUNT_ID
} Fx;

typedef struct {
    int         line;        /* anchor lyric line (see lyrics_data.h)          */
    int         lead_ms;     /* start this far after that line's start         */
    Fx          fx;
    const char *sprite;      /* FX_SPRITE / some effects                        */
    Rgb         c1, c2;      /* palette: primary and accent                     */
    int         trans;       /* TR_* transition into this scene                 */
    int         trans_ms;
    int         seed;
    int         intensity;   /* 0..3, effect strength                           */
    float       p1, p2;      /* effect specific knobs                           */
    const char *caption;     /* shown in --list                                  */
} Scene;

typedef struct {
    Frame      *f;
    const Scene *sc;
    int         t_ms;
    double      t;           /* seconds since this scene started               */
    double      dur;         /* scene length in seconds                        */
    double      prog;        /* t / dur, clamped to 0..1                       */
    double      beat;        /* absolute 4/4 beat position                     */
    double      phase;       /* position inside the current beat, 0..1         */
    int         w, h;
    uint32_t    rnd;         /* per-scene random seed                          */
} Ctx;

extern const Scene SCENES[];
extern const int   SCENE_COUNT;

void scene_resolve(void);                 /* build the start/end table once   */
int  scene_start_ms(int idx);
int  scene_end_ms(int idx);
int  scene_at(int t_ms);                  /* index of the scene playing now   */
int  scene_duration_ms(int idx);
void scene_render(Frame *f, int idx, int t_ms);

/* Full composition for one moment: picks the scene, and when we are inside its
 * transition window renders the outgoing scene as well and blends the two.
 * out, tmp_new and tmp_old must be three distinct frames of the same size. */
void scene_render_at(Frame *out, Frame *tmp_new, Frame *tmp_old, int t_ms);

void scene_list(FILE *out);
const char *fx_name(Fx fx);
const char *tr_name(int tr);

/* Small shared drawing helpers used by more than one effect. */
void fx_panel(Frame *f, int x, int y, int w, int h, const char *title, Rgb border,
              Rgb title_col);
void fx_scan_dots(Frame *f, Rgb col, double density, int seed);

#endif /* MV_SCENES_H */