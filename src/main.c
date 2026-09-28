/* main.c -- CLI, the timing loop and the offline modes (export / dump / list /
 * selftest) of the world.execute(me) ASCII music video.
 *
 * Live playback: the animation is driven by CLOCK_MONOTONIC, the mp3 (when a
 * player is available) is spawned at the same moment, and each frame is composed
 * from the scene table, the karaoke overlay and the keyword typography.
 */
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mv.h"
#include "term.h"
#include "audio.h"
#include "lyrics.h"
#include "scenes.h"
#include "sprites.h"

const char *PROG_NAME = "mv";

Config CFG = {
    .audio_path = NULL,
    .player = NULL,
    .no_audio = false,
    .loop = false,
    .offset_ms = 0,
    .start_ms = 0,
    .duration_ms = 0,
    .audio_lead_ms = 180,
    .fps = 30,
    .color_mode = -1,
    .unicode = false,
    .karaoke = true,
    .banner = true,
    .grain = true,
    .altscreen = true,
    .hud = true,
    .force_w = 0,
    .force_h = 0,
    .quiet = false,
    .export_dir = NULL,
    .export_from = 0,
    .export_to = 0,
    .export_step = 100,
    .export_w = 0,
    .export_h = 0,
    .dump_ms = -1,
    .list_scenes = false,
    .selftest = false,
};

static volatile sig_atomic_t g_quit = 0;
static volatile sig_atomic_t g_resize = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_quit = 1;
}

static void on_winch(int sig)
{
    (void)sig;
    g_resize = 1;
}

/* ------------------------------------------------------------------- usage */

static const char *VERSION = "1.0";

static void usage(FILE *out, const char *argv0)
{
    fprintf(out,
"world.execute (me); -- an ASCII-art music video that runs in your terminal.\n"
"\n"
"usage: %s [options]\n"
"\n"
"playback\n"
"  --audio PATH        mp3 to play (default: auto-detect the song next to the binary)\n"
"  --player NAME       force ffplay|mpg123|mpv|cvlc|afplay\n"
"  --no-audio          silent film\n"
"  --offset MS         slide the whole picture later (+) or earlier (-)\n"
"  --start TIME        jump in: 83 | 83s | 1:23\n"
"  --duration MS       stop after this long (default: the whole song)\n"
"  --loop              replay for ever\n"
"  --fps N             frames per second (default 30)\n"
"  --size WxH          force the canvas size instead of asking the terminal\n"
"  --color MODE        auto|truecolor|256|16|mono\n"
"  --contrast N        tone map strength, 0..1 (default 0.75; 0 = flat)\n"
"  --bg RRGGBB         canvas colour (default 030409, --bg default = use the\n"
"                      terminal's own background)\n"
"  --unicode           use block glyphs for the big typography\n"
"  --no-karaoke        hide the lyric line\n"
"  --no-banner         hide the big keyword typography\n"
"  --no-hud            hide the title / clock / beat strip\n"
"  --no-grain          turn off film grain\n"
"  --no-altscreen      draw on the normal screen buffer\n"
"  -q, --quiet         less chatter on stderr\n"
"\n"
"offline (no terminal needed)\n"
"  --list              print the scene table with its timings\n"
"  --dump MS           print one frame as plain text\n"
"  --export DIR        write frames as .mvf files for tools/mvpreview.py\n"
"  --export-from MS    first frame time (default 0)\n"
"  --export-to MS      last frame time (default 10000)\n"
"  --export-step MS    spacing between frames (default 100)\n"
"  --selftest          check the timeline, the sprites and every effect\n"
"  -h, --help          this text\n"
"  -V, --version       version\n"
"\n"
"examples\n"
"  make && ./start                        # full show, audio if possible\n"
"  ./start --no-audio --color 256         # safe mode for an old terminal\n"
"  ./start --start 1:03 --offset -80      # re-sync the picture to your ears\n",
            argv0);
}

static int parse_time_ms(const char *s)
{
    double v = 0;
    if (strchr(s, ':')) {
        int m = 0;
        if (sscanf(s, "%d:%lf", &m, &v) != 2) return -1;
        return (int)((m * 60.0 + v) * 1000.0 + 0.5);
    }
    size_t n = strlen(s);
    if (n && (s[n - 1] == 's' || s[n - 1] == 'S')) {
        if (sscanf(s, "%lf", &v) != 1) return -1;
        return (int)(v * 1000.0 + 0.5);
    }
    if (sscanf(s, "%lf", &v) != 1) return -1;
    return (int)(v + 0.5);   /* a bare number is milliseconds */
}

/* ------------------------------------------------------------- composition */

static void compose(Frame *out, Frame *tn, Frame *to, int t_ms)
{
    scene_render_at(out, tn, to, t_ms);
    int idx = scene_at(t_ms);
    Rgb c1 = idx >= 0 ? SCENES[idx].c1 : RGB(120, 160, 255);
    Rgb c2 = idx >= 0 ? SCENES[idx].c2 : RGB(235, 245, 255);

    /* Separation first: shadows down, highlights up. */
    fb_contrast(out, CFG.contrast);
    if (CFG.grain) fb_grain(out, t_ms / 33, 0.02, RGB(120, 128, 150));
    fb_scanlines(out, 0.05);
    fb_vignette(out, 0.12);

    if (CFG.banner) lyrics_draw_keywords(out, t_ms, c2, c1);
    if (CFG.karaoke) lyrics_draw(out, t_ms, c2, c1, 1);
    lyrics_draw_hud(out, t_ms, c2, CFG.hud);
}

/* ------------------------------------------------------------------ modes */

static int do_list(void)
{
    printf("world.execute (me); -- %d scenes, %d lyric lines, %d words, %.1fs\n\n",
           SCENE_COUNT, LYRIC_LINE_COUNT, LYRIC_WORD_COUNT, LYRIC_TOTAL_MS / 1000.0);
    scene_list(stdout);
    return 0;
}

static int do_dump(long t_ms)
{
    int w = CFG.force_w > 0 ? CFG.force_w : 80;
    int h = CFG.force_h > 0 ? CFG.force_h : 24;
    Frame f, tn, to;
    frame_alloc(&f, w, h);
    frame_alloc(&tn, w, h);
    frame_alloc(&to, w, h);
    compose(&f, &tn, &to, (int)t_ms);
    term_dump_text(&f, stdout);
    frame_free(&f);
    frame_free(&tn);
    frame_free(&to);
    return 0;
}

static int do_export(void)
{
    const char *dir = CFG.export_dir;
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "%s: cannot create %s: %s\n", PROG_NAME, dir, strerror(errno));
        return 1;
    }
    int w = CFG.force_w > 0 ? CFG.force_w : 80;
    int h = CFG.force_h > 0 ? CFG.force_h : 24;
    Frame f, tn, to;
    frame_alloc(&f, w, h);
    frame_alloc(&tn, w, h);
    frame_alloc(&to, w, h);
    CFG.force_w = w;
    CFG.force_h = h;

    int step = CFG.export_step > 0 ? CFG.export_step : 100;
    int from = CFG.export_from;
    int to_ms = CFG.export_to >= from ? CFG.export_to : from + 10000;
    int count = 0;
    for (int t = from; t <= to_ms; t += step) {
        char path[4096];
        snprintf(path, sizeof path, "%s/f%06d.mvf", dir, t);
        compose(&f, &tn, &to, t);
        if (term_export_frame(&f, path, t) != 0) {
            fprintf(stderr, "%s: cannot write %s: %s\n", PROG_NAME, path, strerror(errno));
            return 1;
        }
        count++;
        if (!CFG.quiet && (count % 20) == 0)
            fprintf(stderr, "\r%s: exported %d frames (%d ms)   ", PROG_NAME, count, t);
    }
    if (!CFG.quiet) fprintf(stderr, "\r%s: exported %d frames to %s\n", PROG_NAME, count, dir);
    frame_free(&f);
    frame_free(&tn);
    frame_free(&to);
    return 0;
}

static int do_selftest(void)
{
    int fails = 0;
    scene_resolve();
    printf("scenes: %d, lyric lines: %d, words: %d\n", SCENE_COUNT, LYRIC_LINE_COUNT,
           LYRIC_WORD_COUNT);
    if (scene_start_ms(0) != 0) {
        printf("FAIL: first scene starts at %d ms, not 0\n", scene_start_ms(0));
        fails++;
    }
    if (scene_end_ms(SCENE_COUNT - 1) != LYRIC_TOTAL_MS) {
        printf("FAIL: last scene ends at %d ms, not %d\n", scene_end_ms(SCENE_COUNT - 1),
               LYRIC_TOTAL_MS);
        fails++;
    }
    for (int i = 0; i < SCENE_COUNT; i++) {
        if (i + 1 < SCENE_COUNT && scene_end_ms(i) != scene_start_ms(i + 1)) {
            printf("FAIL: scene %d ends at %d but scene %d starts at %d\n", i,
                   scene_end_ms(i), i + 1, scene_start_ms(i + 1));
            fails++;
        }
        if (scene_duration_ms(i) < 150) {
            printf("FAIL: scene %d is only %d ms long\n", i, scene_duration_ms(i));
            fails++;
        }
        if (SCENES[i].line < 0 || SCENES[i].line >= LYRIC_LINE_COUNT) {
            printf("FAIL: scene %d anchors to line %d\n", i, SCENES[i].line);
            fails++;
        }
        if (SCENES[i].fx == FX_SPRITE && !mv_sprite(SCENES[i].sprite)) {
            printf("FAIL: scene %d wants a missing sprite '%s'\n", i, SCENES[i].sprite);
            fails++;
        }
    }
    /* Every effect must survive being rendered at any size and moment. */
    const int sizes[][2] = { { 20, 8 }, { 80, 24 }, { 200, 50 } };
    for (int s = 0; s < 3; s++) {
        int w = sizes[s][0], h = sizes[s][1];
        Frame f, tn, to;
        CFG.force_w = w;
        CFG.force_h = h;
        frame_alloc(&f, w, h);
        frame_alloc(&tn, w, h);
        frame_alloc(&to, w, h);
        for (int i = 0; i < SCENE_COUNT; i++) {
            int marks[3] = { scene_start_ms(i) + 1,
                             (scene_start_ms(i) + scene_end_ms(i)) / 2,
                             scene_end_ms(i) - 1 };
            for (int k = 0; k < 3; k++) {
                if (marks[k] < 0) marks[k] = 0;
                compose(&f, &tn, &to, marks[k]);
                int ink = 0;
                for (int c = 0; c < w * h; c++) {
                    unsigned char ch = f.cells[c].ch;
                    if (ch && ch != ' ') ink++;
                }
                if (ink == 0) {
                    printf("FAIL: scene %d (%s) drew nothing at %dx%d t=%d\n",
                           i, fx_name(SCENES[i].fx), w, h, marks[k]);
                    fails++;
                } else if (ink * 100 < w * h) {
                    /* Only worth mentioning; a few scenes are deliberately sparse. */
                    if (!CFG.quiet)
                        printf("note: scene %d (%s) is sparse: %d/%d cells\n",
                               i, fx_name(SCENES[i].fx), ink, w * h);
                }
            }
        }
        frame_free(&f);
        frame_free(&tn);
        frame_free(&to);
    }
    CFG.force_w = CFG.force_h = 0;
    if (!mv_sprite("heart") || !mv_sprite("cat")) {
        printf("FAIL: sprite pack incomplete\n");
        fails++;
    }
    printf("selftest: %s (%d failures)\n", fails ? "FAILED" : "OK", fails);
    return fails ? 1 : 0;
}

/* -------------------------------------------------------------------- live */

static void frames_resize(Frame *a, Frame *b, Frame *c, Frame *d, int w, int h)
{
    frame_free(a); frame_free(b); frame_free(c); frame_free(d);
    frame_alloc(a, w, h);
    frame_alloc(b, w, h);
    frame_alloc(c, w, h);
    frame_alloc(d, w, h);
    frame_invalidate(d);
}

static void show_once(int *quit_by_signal)
{
    Audio audio = { -1, 0, NULL };
    char *song = NULL;
    int total = CFG.duration_ms > 0 ? CFG.duration_ms : LYRIC_TOTAL_MS;

    if (!CFG.no_audio) {
        song = audio_find_file(PROG_NAME, CFG.audio_path);
        if (song && !CFG.quiet)
            fprintf(stderr, "%s: audio %s\n", PROG_NAME, song);
        if (song) {
            if (audio_start(&audio, song, CFG.player, CFG.audio_lead_ms,
                            CFG.start_ms + (CFG.offset_ms < 0 ? -CFG.offset_ms : 0),
                            CFG.quiet) == 0 && !CFG.quiet)
                fprintf(stderr, "%s: playing with %s\n", PROG_NAME, audio.player);
        }
        free(song);
    }

    term_init();
    term_set_title("world.execute (me); - ASCII MV");
    int w = mv_term_width(), h = mv_term_height();
    Frame f, tn, to, prev;
    frame_alloc(&f, w, h);
    frame_alloc(&tn, w, h);
    frame_alloc(&to, w, h);
    frame_alloc(&prev, w, h);
    frame_invalidate(&prev);

    double t0 = mv_now();
    long frame = 0;
    while (!g_quit) {
        if (g_resize) {
            g_resize = 0;
            int nw = 0, nh = 0;
            if (term_poll_size(&nw, &nh)) {
                frames_resize(&f, &tn, &to, &prev, nw, nh);
                w = nw; h = nh;
            }
        }
        double el = mv_now() - t0;
        long t_ms = CFG.start_ms + (long)(el * 1000.0) + CFG.offset_ms;
        if (t_ms < 0) t_ms = 0;
        if (t_ms >= total) break;

        compose(&f, &tn, &to, (int)t_ms);
        term_present(&f, &prev);

        frame++;
        double target = t0 + (double)frame / (double)CFG.fps;
        double now = mv_now();
        if (target > now) mv_sleep(target - now);
    }
    frame_free(&f);
    frame_free(&tn);
    frame_free(&to);
    frame_free(&prev);
    audio_stop(&audio);
    if (quit_by_signal) *quit_by_signal = g_quit ? 1 : 0;
}

/* -------------------------------------------------------------------- main */

int main(int argc, char **argv)
{
    static const struct option opts[] = {
        { "audio",         required_argument, NULL, 1000 },
        { "player",        required_argument, NULL, 1001 },
        { "no-audio",      no_argument,       NULL, 1002 },
        { "offset",        required_argument, NULL, 1003 },
        { "start",         required_argument, NULL, 1004 },
        { "duration",      required_argument, NULL, 1005 },
        { "fps",           required_argument, NULL, 1006 },
        { "size",          required_argument, NULL, 1007 },
        { "color",         required_argument, NULL, 1008 },
        { "unicode",       no_argument,       NULL, 1009 },
        { "ascii",         no_argument,       NULL, 1010 },
        { "no-karaoke",    no_argument,       NULL, 1011 },
        { "no-banner",     no_argument,       NULL, 1012 },
        { "no-grain",      no_argument,       NULL, 1013 },
        { "no-altscreen",  no_argument,       NULL, 1014 },
        { "no-hud",        no_argument,       NULL, 1015 },
        { "loop",          no_argument,       NULL, 1016 },
        { "list",          no_argument,       NULL, 1017 },
        { "dump",          required_argument, NULL, 1018 },
        { "export",        required_argument, NULL, 1019 },
        { "export-from",   required_argument, NULL, 1020 },
        { "export-to",     required_argument, NULL, 1021 },
        { "export-step",   required_argument, NULL, 1022 },
        { "selftest",      no_argument,       NULL, 1023 },
        { "bg",            required_argument, NULL, 1024 },
        { "contrast",      required_argument, NULL, 1025 },
        { "quiet",         no_argument,       NULL, 'q' },
        { "help",          no_argument,       NULL, 'h' },
        { "version",       no_argument,       NULL, 'V' },
        { NULL, 0, NULL, 0 }
    };

    CFG.canvas_solid = true;
    CFG.canvas_bg = RGB(3, 4, 9);
    CFG.contrast = 0.75;

    int c;
    while ((c = getopt_long(argc, argv, "qhV", opts, NULL)) != -1) {
        switch (c) {
        case 1000: CFG.audio_path = optarg; break;
        case 1001: CFG.player = optarg; break;
        case 1002: CFG.no_audio = true; break;
        case 1003: CFG.offset_ms = atoi(optarg); break;
        case 1004: {
            int v = parse_time_ms(optarg);
            if (v < 0) { fprintf(stderr, "%s: bad --start '%s'\n", PROG_NAME, optarg); return 2; }
            CFG.start_ms = v;
            break;
        }
        case 1005: CFG.duration_ms = atoi(optarg); break;
        case 1006: CFG.fps = atoi(optarg); break;
        case 1007: {
            int w = 0, h = 0;
            if (sscanf(optarg, "%dx%d", &w, &h) != 2 || w < 10 || h < 5) {
                fprintf(stderr, "%s: bad --size '%s'\n", PROG_NAME, optarg);
                return 2;
            }
            CFG.force_w = w;
            CFG.force_h = h;
            break;
        }
        case 1008:
            if (!strcmp(optarg, "mono")) CFG.color_mode = COLOR_MONO;
            else if (!strcmp(optarg, "16")) CFG.color_mode = COLOR_16;
            else if (!strcmp(optarg, "256")) CFG.color_mode = COLOR_256;
            else if (!strcmp(optarg, "truecolor")) CFG.color_mode = COLOR_TRUECOLOR;
            else if (!strcmp(optarg, "auto")) CFG.color_mode = -1;
            else { fprintf(stderr, "%s: bad --color '%s'\n", PROG_NAME, optarg); return 2; }
            break;
        case 1009: CFG.unicode = true; break;
        case 1010: CFG.unicode = false; break;
        case 1011: CFG.karaoke = false; break;
        case 1012: CFG.banner = false; break;
        case 1013: CFG.grain = false; break;
        case 1014: CFG.altscreen = false; break;
        case 1015: CFG.hud = false; break;
        case 1016: CFG.loop = true; break;
        case 1017: CFG.list_scenes = true; break;
        case 1018: CFG.dump_ms = parse_time_ms(optarg); break;
        case 1019: CFG.export_dir = optarg; break;
        case 1020: CFG.export_from = atoi(optarg); break;
        case 1021: CFG.export_to = atoi(optarg); break;
        case 1022: CFG.export_step = atoi(optarg); break;
        case 1023: CFG.selftest = true; break;
        case 1024:
            if (!strcmp(optarg, "default") || !strcmp(optarg, "none")) {
                CFG.canvas_solid = false;
            } else {
                unsigned r = 0, g = 0, b = 0;
                char extra = 0;
                if (strlen(optarg) != 6 ||
                    sscanf(optarg, "%2x%2x%2x%c", &r, &g, &b, &extra) != 3) {
                    fprintf(stderr, "%s: bad --bg '%s' (want RRGGBB or default)\n",
                            PROG_NAME, optarg);
                    return 2;
                }
                CFG.canvas_solid = true;
                CFG.canvas_bg = RGB((int)r, (int)g, (int)b);
            }
            break;
        case 1025:
            CFG.contrast = atof(optarg);
            if (CFG.contrast < 0) CFG.contrast = 0;
            if (CFG.contrast > 1) CFG.contrast = 1;
            break;
        case 'q': CFG.quiet = true; break;
        case 'h': usage(stdout, argv[0]); return 0;
        case 'V': printf("%s %s\n", PROG_NAME, VERSION); return 0;
        default: usage(stderr, argv[0]); return 2;
        }
    }
    if (CFG.fps < 1) CFG.fps = 1;
    if (CFG.fps > 120) CFG.fps = 120;

    if (CFG.selftest) return do_selftest();
    if (CFG.list_scenes) return do_list();
    if (CFG.dump_ms >= 0) return do_dump(CFG.dump_ms);
    if (CFG.export_dir) return do_export();

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = on_winch;
    sigaction(SIGWINCH, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    if (!CFG.quiet) {
        fprintf(stderr,
                "%s: world.execute (me); -- %d scenes over %.1fs\n"
                "%s: %s, Ctrl-C to stop\n",
                PROG_NAME, SCENE_COUNT, LYRIC_TOTAL_MS / 1000.0,
                PROG_NAME, CFG.no_audio ? "silent" : "audio on");
    }

    atexit(term_restore);
    do {
        g_quit = 0;
        show_once(NULL);
    } while (CFG.loop && !g_quit);
    term_restore();
    return 0;
}