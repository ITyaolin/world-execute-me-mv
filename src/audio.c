/* audio.c -- find the mp3, play it with whatever is installed, and keep it in
 * sync with the animation clock.
 *
 * The MV never depends on audio: if no player (or no sound card) is available it
 * simply runs as a silent film.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "audio.h"

#define SONG_NAME "world.execute (me) ;.mp3"

typedef struct {
    const char *name;
    const char *argv[6];   /* NULL terminated, %s = file, %d = seek seconds */
    int         supports_seek;
} Player;

static const Player PLAYERS[] = {
    { "ffplay", { "ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet", NULL }, 1 },
    { "mpg123", { "mpg123", "-q", NULL },                                         0 },
    { "mpv",    { "mpv", "--no-video", "--really-quiet", NULL },                  1 },
    { "cvlc",   { "cvlc", "--intf", "dummy", "--play-and-exit", NULL },           1 },
    { "afplay", { "afplay", NULL },                                               0 },
};

static int which(const char *name)
{
    const char *path = getenv("PATH");
    if (!path) path = "/usr/bin:/bin:/usr/local/bin";
    char buf[4096];
    const char *p = path;
    while (*p) {
        const char *e = strchr(p, ':');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        if (n < sizeof buf - 2 - strlen(name)) {
            memcpy(buf, p, n);
            buf[n] = '/';
            strcpy(buf + n + 1, name);
            if (access(buf, X_OK) == 0) return 1;
        }
        if (!e) break;
        p = e + 1;
    }
    return 0;
}

static char *dir_name(const char *path)
{
    char *copy = strdup(path);
    if (!copy) return NULL;
    char *slash = strrchr(copy, '/');
    if (slash) *slash = 0;
    else { free(copy); return strdup("."); }
    return copy;
}

static char *join(const char *dir, const char *name)
{
    size_t n = strlen(dir) + strlen(name) + 2;
    char *out = malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "%s/%s", dir, name);
    return out;
}

static int is_file(const char *p)
{
    struct stat st;
    return p && stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

/* Pick the first *.mp3 in a directory (any name) as a fallback. */
static char *first_mp3(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) return NULL;
    struct dirent *e;
    char *found = NULL;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n > 4 && strcasecmp(e->d_name + n - 4, ".mp3") == 0) {
            found = join(dir, e->d_name);
            break;
        }
    }
    closedir(d);
    return found;
}

static char *exe_dir(const char *argv0)
{
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n > 0) {
        buf[n] = 0;
        return dir_name(buf);
    }
    if (argv0 && strchr(argv0, '/')) return dir_name(argv0);
    return strdup(".");
}

char *audio_find_file(const char *argv0, const char *explicit_path)
{
    if (explicit_path) {
        if (is_file(explicit_path)) return strdup(explicit_path);
        fprintf(stderr, "mv: audio file not found: %s\n", explicit_path);
        return NULL;
    }
    const char *env = getenv("MV_MP3");
    if (env && is_file(env)) return strdup(env);

    char *ed = exe_dir(argv0);
    char *cwd = getcwd(NULL, 0);
    char *dirs[5];
    int nd = 0;
    if (ed) dirs[nd++] = ed;
    if (cwd) dirs[nd++] = cwd;
    char *up = ed ? join(ed, "..") : NULL;
    if (up) dirs[nd++] = up;
    char *upc = cwd ? join(cwd, "..") : NULL;
    if (upc) dirs[nd++] = upc;

    char *result = NULL;
    for (int i = 0; i < nd && !result; i++) {
        char *cand = join(dirs[i], SONG_NAME);
        if (is_file(cand)) result = cand;
        else free(cand);
    }
    for (int i = 0; i < nd && !result; i++)
        result = first_mp3(dirs[i]);

    /* dirs[] owns every string above, so freeing them here is enough. */
    for (int i = 0; i < nd; i++) free(dirs[i]);
    return result;
}

int audio_start(Audio *a, const char *path, const char *force_player, int lead_ms,
                int seek_ms, int quiet)
{
    a->pid = -1;
    a->active = 0;
    a->player = NULL;
    if (!path) return -1;

    const Player *pl = NULL;
    if (force_player) {
        for (size_t i = 0; i < sizeof PLAYERS / sizeof PLAYERS[0]; i++)
            if (strcmp(PLAYERS[i].name, force_player) == 0) pl = &PLAYERS[i];
        if (!pl) {
            fprintf(stderr, "mv: unknown --player '%s'\n", force_player);
            return -1;
        }
        if (!which(pl->name)) {
            fprintf(stderr, "mv: player not found: %s\n", pl->name);
            return -1;
        }
    } else {
        for (size_t i = 0; i < sizeof PLAYERS / sizeof PLAYERS[0]; i++) {
            if (which(PLAYERS[i].name)) { pl = &PLAYERS[i]; break; }
        }
    }
    if (!pl) {
        if (!quiet) fprintf(stderr, "mv: no audio player found (silent film)\n");
        return -1;
    }
    if (seek_ms > 0 && !pl->supports_seek && !quiet)
        fprintf(stderr, "mv: %s cannot seek; audio will start from the top\n", pl->name);

    /* Build argv. */
    const char *args[12];
    int n = 0;
    char seekbuf[32];
    args[n++] = pl->name;
    for (int i = 1; pl->argv[i] && n < 9; i++) {
        if (strcmp(pl->argv[i], "--start") == 0) {   /* reserved marker */
            if (seek_ms > 0 && pl->supports_seek) {
                snprintf(seekbuf, sizeof seekbuf, "%.3f", seek_ms / 1000.0);
                args[n++] = seekbuf;
            }
            continue;
        }
        args[n++] = pl->argv[i];
    }
    if (seek_ms > 0 && pl->supports_seek) {
        if (strcmp(pl->name, "ffplay") == 0) {
            args[n++] = "-ss";
            snprintf(seekbuf, sizeof seekbuf, "%.3f", seek_ms / 1000.0);
            args[n++] = seekbuf;
        } else if (strcmp(pl->name, "mpv") == 0) {
            snprintf(seekbuf, sizeof seekbuf, "--start=%.3f", seek_ms / 1000.0);
            args[n++] = seekbuf;
        }
    }
    if (strcmp(pl->name, "ffplay") == 0) args[n++] = "-i";
    args[n++] = path;
    args[n] = NULL;

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "mv: fork failed: %s\n", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > 2) close(devnull);
        }
        execvp(args[0], (char *const *)args);
        _exit(127);
    }
    a->pid = pid;
    a->player = pl->name;

    /* Give it a moment, then make sure it did not die instantly (missing sound
     * card, unsupported file, ...). */
    int waited = lead_ms > 0 ? lead_ms : 0;
    if (waited > 0) {
        struct timespec ts = { waited / 1000, (long)(waited % 1000) * 1000000L };
        nanosleep(&ts, NULL);
    }
    int status = 0;
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == pid) {
        if (!quiet)
            fprintf(stderr, "mv: %s exited immediately (status %d); continuing silent\n",
                    pl->name, status);
        a->pid = -1;
        return -1;
    }
    a->active = 1;
    return 0;
}

void audio_stop(Audio *a)
{
    if (!a->active || a->pid <= 0) return;
    kill(a->pid, SIGTERM);
    for (int i = 0; i < 20; i++) {
        int status = 0;
        pid_t r = waitpid(a->pid, &status, WNOHANG);
        if (r == a->pid || (r < 0 && errno == ECHILD)) {
            a->active = 0;
            return;
        }
        struct timespec ts = { 0, 20 * 1000000L };
        nanosleep(&ts, NULL);
    }
    kill(a->pid, SIGKILL);
    waitpid(a->pid, NULL, 0);
    a->active = 0;
}