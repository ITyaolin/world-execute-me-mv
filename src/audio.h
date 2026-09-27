/* audio.h -- optional mp3 playback kept in step with the visual clock. */
#ifndef MV_AUDIO_H
#define MV_AUDIO_H

#include <sys/types.h>

typedef struct {
    pid_t       pid;
    int         active;
    const char *player;
} Audio;

/* Locate the song: explicit argument, $MV_MP3, then a few directories around the
 * executable looking for the expected filename or any *.mp3.  Returns a malloc'd
 * path or NULL. */
char *audio_find_file(const char *argv0, const char *explicit_path);

/* Spawn a player for `path`, wait `lead_ms`, and report whether it survived.
 * Returns 0 on success. */
int  audio_start(Audio *a, const char *path, const char *force_player, int lead_ms,
                 int seek_ms, int quiet);
void audio_stop(Audio *a);

#endif /* MV_AUDIO_H */