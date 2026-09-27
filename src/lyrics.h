/* lyrics.h -- karaoke overlay and keyword typography. */
#ifndef MV_LYRICS_H
#define MV_LYRICS_H

#include "mv.h"

/* Index of the lyric line that should be on screen at t_ms (300 ms early so the
 * words are already there when the voice arrives), or -1 when nothing applies. */
int  lyric_display_line(int t_ms);
int  lyric_line_at(int t_ms);

/* Bottom-of-screen karaoke: previous line, current line with per-word lighting
 * from the .yrc timings, and a progress/time strip. */
void lyrics_draw(Frame *f, int t_ms, Rgb base, Rgb accent, int show_progress);

/* Big centred typography for the ALL-CAPS keyword in the current phrase. */
void lyrics_draw_keywords(Frame *f, int t_ms, Rgb accent, Rgb base);

/* Top-left song title, top-right clock/beat. */
void lyrics_draw_hud(Frame *f, int t_ms, Rgb dim, int show_hud);

/* Pure text helpers shared with the scenes. */
const char *lyric_text(int index);
int         lyric_keyword_at(int t_ms, const char **text, double *age);

#endif /* MV_LYRICS_H */