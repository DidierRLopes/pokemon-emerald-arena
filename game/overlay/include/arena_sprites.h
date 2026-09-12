#ifndef GUARD_ARENA_SPRITES_H
#define GUARD_ARENA_SPRITES_H
#include "global.h"
#define ARENA_ANIM_IDLE 0
#define ARENA_ANIM_WALK 1
#define ARENA_ANIM_SHOOT 2
#define ARENA_ANIM_ATTACK 3
#define ARENA_ANIM_SPECIAL 4

struct ArenaSpriteAnimation
{
    const u8 *tiles;
    const u8 *durations;
    u16 frames, totalTicks, hitTick;
};
struct ArenaSpriteSet
{
    u16 species;
    const u16 *palette;
    struct ArenaSpriteAnimation animations[5];
};
const struct ArenaSpriteSet *ArenaSprites_Get(u16 species);
u8 ArenaSprites_Frame(const struct ArenaSpriteAnimation *anim, u16 tick);
#endif
