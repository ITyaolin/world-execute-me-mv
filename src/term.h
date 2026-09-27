/* term.h -- terminal output layer (see term.c). */
#ifndef MV_TERM_H
#define MV_TERM_H

#include <stdio.h>
#include "mv.h"

void term_init(void);
void term_restore(void);
void term_set_title(const char *s);
int  term_poll_size(int *w, int *h);
void term_present(Frame *cur, Frame *prev);
int  term_color_mode(void);
void term_dump_text(const Frame *f, FILE *out);

/* Offline frame export for the PNG preview pipeline.
 * Format: "MVF 1 / size W H / time MS / c / <char rows> / g / <fg hex rows>
 *          / b / <bg hex rows or '-' >"                                        */
int  term_export_frame(const Frame *f, const char *path, long time_ms);

#endif /* MV_TERM_H */