/* term.c -- raw terminal handling, colour quantisation and diff rendering.
 *
 * Only the cells that actually changed since the previous frame are written, and
 * cursor moves / colour escapes are suppressed when the terminal is already in
 * the right state.  That keeps a 30 fps full-screen animation cheap even over a
 * slow pty.
 */
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "mv.h"
#include "term.h"

static struct termios g_saved;
static int  g_have_saved = 0;
static int  g_raw = 0;
static int  g_altscreen = 0;
static int  g_color = COLOR_TRUECOLOR;
static int  g_unicode = 0;

static int  g_w = 80, g_h = 24;

/* Persistent terminal output state. */
static int  g_last_fg = -1;
static int  g_last_bg = -1;
static int  g_cx = -1, g_cy = -1;
static int  g_bold = -1;

static char *g_buf = NULL;
static size_t g_cap = 0, g_len = 0;

/* ------------------------------------------------------------------- buffer */

static void buf_need(size_t extra)
{
    if (g_len + extra <= g_cap) return;
    size_t cap = g_cap ? g_cap * 2 : 8192;
    while (cap < g_len + extra) cap *= 2;
    char *p = realloc(g_buf, cap);
    if (!p) { fprintf(stderr, "%s: out of memory\n", PROG_NAME); exit(1); }
    g_buf = p;
    g_cap = cap;
}

static void buf_put(const char *s, size_t n)
{
    buf_need(n);
    memcpy(g_buf + g_len, s, n);
    g_len += n;
}

static void buf_puts(const char *s) { buf_put(s, strlen(s)); }

static void buf_printf(const char *fmt, ...)
{
    char tmp[64];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n > 0) buf_put(tmp, (size_t)n);
}

static void buf_flush(void)
{
    if (!g_len) return;
    size_t off = 0;
    while (off < g_len) {
        ssize_t n = write(STDOUT_FILENO, g_buf + off, g_len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        off += (size_t)n;
    }
    g_len = 0;
}

/* ------------------------------------------------------------------- colours */

static int rgb_luma(Rgb c)
{
    return (c.r * 30 + c.g * 59 + c.b * 11) / 100;
}

static int rgb_to_256(Rgb c)
{
    /* Grey ramp when the colour is (nearly) neutral, cube otherwise. */
    int mx = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
    int mn = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
    if (mx - mn < 12) {
        int avg = (c.r + c.g + c.b) / 3;
        if (avg < 8) return 16;
        if (avg > 244) return 231;
        int idx = (avg - 8) / 10;
        if (idx < 0) idx = 0;
        if (idx > 23) idx = 23;
        return 232 + idx;
    }
    int r = (c.r * 6) / 256, g = (c.g * 6) / 256, b = (c.b * 6) / 256;
    if (r > 5) r = 5;
    if (g > 5) g = 5;
    if (b > 5) b = 5;
    return 16 + 36 * r + 6 * g + b;
}

static int rgb_to_16(Rgb c)
{
    int luma = rgb_luma(c);
    int bright = luma > 128;
    int idx = (c.r > 96) | ((c.g > 96) << 1) | ((c.b > 96) << 2);
    return bright ? idx + 8 : idx;
}

static void emit_fg(Rgb c)
{
    int code;
    switch (g_color) {
    case COLOR_TRUECOLOR:
        code = (c.r << 16) | (c.g << 8) | c.b;
        if (code == g_last_fg) return;
        g_last_fg = code;
        buf_printf("\033[38;2;%d;%d;%dm", c.r, c.g, c.b);
        break;
    case COLOR_256:
        code = rgb_to_256(c);
        if (code == g_last_fg) return;
        g_last_fg = code;
        buf_printf("\033[38;5;%dm", code);
        break;
    case COLOR_16:
        code = rgb_to_16(c);
        if (code == g_last_fg) return;
        g_last_fg = code;
        buf_printf("\033[%dm", code < 8 ? 30 + code : 90 + (code - 8));
        break;
    default: {
        int bold = rgb_luma(c) > 140;
        if (bold != g_bold) {
            g_bold = bold;
            buf_puts(bold ? "\033[1m" : "\033[22m");
        }
        g_last_fg = -1;
        break;
    }
    }
}

static void emit_bg(Rgb c, int has)
{
    if (g_color == COLOR_MONO) return;
    int code;
    if (!has) {
        if (g_last_bg == -2) return;
        g_last_bg = -2;
        buf_puts("\033[49m");
        return;
    }
    switch (g_color) {
    case COLOR_TRUECOLOR:
        code = (c.r << 16) | (c.g << 8) | c.b;
        if (code == g_last_bg) return;
        g_last_bg = code;
        buf_printf("\033[48;2;%d;%d;%dm", c.r, c.g, c.b);
        break;
    case COLOR_256:
        code = rgb_to_256(c);
        if (code == g_last_bg) return;
        g_last_bg = code;
        buf_printf("\033[48;5;%dm", code);
        break;
    default:
        code = rgb_to_16(c);
        if (code == g_last_bg) return;
        g_last_bg = code;
        buf_printf("\033[%dm", code < 8 ? 40 + code : 100 + (code - 8));
        break;
    }
}

/* ------------------------------------------------------------------ terminal */

static int detect_color_mode(void)
{
    if (CFG.color_mode >= 0) return CFG.color_mode;
    const char *ct = getenv("COLORTERM");
    if (ct && (strstr(ct, "truecolor") || strstr(ct, "24bit"))) return COLOR_TRUECOLOR;
    const char *term = getenv("TERM");
    if (!term || !*term || !strcmp(term, "dumb")) return COLOR_MONO;
    if (strstr(term, "256color")) return COLOR_256;
    if (strstr(term, "direct")) return COLOR_TRUECOLOR;
    return COLOR_16;
}

static void read_size(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        g_w = ws.ws_col;
        g_h = ws.ws_row;
    } else {
        const char *c = getenv("COLUMNS"), *l = getenv("LINES");
        g_w = (c && atoi(c) > 0) ? atoi(c) : 80;
        g_h = (l && atoi(l) > 0) ? atoi(l) : 24;
    }
    if (CFG.force_w > 0) g_w = CFG.force_w;
    if (CFG.force_h > 0) g_h = CFG.force_h;
    if (g_w > 1000) g_w = 1000;
    if (g_h > 400) g_h = 400;
    if (g_w < 20) g_w = 20;
    if (g_h < 8) g_h = 8;
}

void term_init(void)
{
    g_color = detect_color_mode();
    g_unicode = CFG.unicode;
    read_size();

    if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &g_saved) == 0) {
        g_have_saved = 1;
        struct termios raw = g_saved;
        raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
        raw.c_iflag &= (tcflag_t)~(IXON | ICRNL);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) g_raw = 1;
    }

    buf_puts("\033[?25l");   /* hide cursor           */
    buf_puts("\033[?7l");    /* no autowrap           */
    if (CFG.altscreen && isatty(STDOUT_FILENO)) {
        buf_puts("\033[?1049h");  /* alternate screen  */
        g_altscreen = 1;
    }
    buf_puts("\033[0m\033[2J");
    g_last_fg = g_last_bg = -1;
    g_cx = g_cy = -1;
    buf_flush();
}

void term_restore(void)
{
    if (!g_buf && !g_raw && !g_altscreen) return;
    g_len = 0;
    buf_puts("\033[0m");
    buf_puts("\033[?7h");
    if (g_altscreen) {
        buf_puts("\033[?1049l");
        g_altscreen = 0;
    } else {
        /* Leave the final frame on screen and park the cursor below it. */
        buf_printf("\033[%d;1H\n", g_h);
    }
    buf_puts("\033[?25h");
    buf_flush();

    if (g_raw && g_have_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_saved);
        g_raw = 0;
    }
    g_last_fg = g_last_bg = -1;
    g_cx = g_cy = -1;
}

void term_set_title(const char *s)
{
    buf_puts("\033]0;");
    buf_puts(s);
    buf_puts("\007");
    buf_flush();
}

int mv_term_width(void) { return g_w; }
int mv_term_height(void) { return g_h; }

int term_poll_size(int *w, int *h)
{
    int ow = g_w, oh = g_h;
    read_size();
    if (w) *w = g_w;
    if (h) *h = g_h;
    return (ow != g_w || oh != g_h);
}

/* --------------------------------------------------------------- frame output */

static void move_to(int x, int y)
{
    if (g_cx == x && g_cy == y) return;
    buf_printf("\033[%d;%dH", y + 1, x + 1);
    g_cx = x;
    g_cy = y;
}

void term_present(Frame *cur, Frame *prev)
{
    g_len = 0;
    for (int y = 0; y < cur->h; y++) {
        for (int x = 0; x < cur->w; x++) {
            Cell *c = &cur->cells[y * cur->w + x];
            Cell *p = &prev->cells[y * prev->w + x];
            unsigned char ch = c->ch ? c->ch : ' ';
            if (p->ch == c->ch && p->has_bg == c->has_bg &&
                p->fg.r == c->fg.r && p->fg.g == c->fg.g && p->fg.b == c->fg.b &&
                p->bg.r == c->bg.r && p->bg.g == c->bg.g && p->bg.b == c->bg.b)
                continue;
            if (ch != ' ' || c->has_bg) {
                move_to(x, y);
                emit_bg(c->bg, c->has_bg);
                emit_fg(c->fg);
                buf_put((const char *)&ch, 1);
                g_cx = x + 1;
                g_cy = y;
            } else if (p->has_bg) {
                /* Clearing a previously filled cell back to the default bg. */
                move_to(x, y);
                emit_bg(c->bg, 0);
                emit_fg(c->fg);
                buf_put(" ", 1);
                g_cx = x + 1;
                g_cy = y;
            }
            *p = *c;
        }
    }
    buf_flush();
}

/* Print one frame as plain text (used by --dump for debugging/eyeballing). */
void term_dump_text(const Frame *f, FILE *out)
{
    for (int y = 0; y < f->h; y++) {
        for (int x = 0; x < f->w; x++) {
            unsigned char ch = f->cells[y * f->w + x].ch;
            fputc(ch ? ch : ' ', out);
        }
        fputc('\n', out);
    }
}

int term_color_mode(void) { return g_color; }

/* Write one frame in the compact text format consumed by tools/mvframe2png.py. */
int term_export_frame(const Frame *f, const char *path, long time_ms)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "MVF 1\nsize %d %d\ntime %ld\nc\n", f->w, f->h, time_ms);
    for (int y = 0; y < f->h; y++) {
        for (int x = 0; x < f->w; x++) {
            unsigned char ch = f->cells[y * f->w + x].ch;
            fputc((ch && ch != '\n' && ch != '\r') ? ch : ' ', fp);
        }
        fputc('\n', fp);
    }
    fprintf(fp, "g\n");
    for (int y = 0; y < f->h; y++) {
        for (int x = 0; x < f->w; x++) {
            const Cell *c = &f->cells[y * f->w + x];
            fprintf(fp, "%02x%02x%02x%c", c->fg.r, c->fg.g, c->fg.b,
                    x + 1 == f->w ? '\n' : ' ');
        }
    }
    fprintf(fp, "b\n");
    for (int y = 0; y < f->h; y++) {
        for (int x = 0; x < f->w; x++) {
            const Cell *c = &f->cells[y * f->w + x];
            if (c->has_bg)
                fprintf(fp, "%02x%02x%02x%c", c->bg.r, c->bg.g, c->bg.b,
                        x + 1 == f->w ? '\n' : ' ');
            else
                fprintf(fp, "------%c", x + 1 == f->w ? '\n' : ' ');
        }
    }
    fclose(fp);
    return 0;
}