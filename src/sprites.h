#ifndef MV_SPRITES_H
#define MV_SPRITES_H

/* One fixed ASCII-art sprite. rows is a NULL-terminated array of `h` strings,
 * every one exactly `w` bytes long (no trailing spaces trimmed).       */
typedef struct {
    const char       *name;
    const char *const *rows;
    int               w;
    int               h;
} MvSprite;

extern const MvSprite MV_SPRITES[];
extern const int MV_SPRITE_COUNT;

/* Returns NULL when the name is unknown. */
const MvSprite *mv_sprite(const char *name);

#endif /* MV_SPRITES_H */
