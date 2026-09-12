#include "arena_sprites.h"
#include "constants/species.h"
#include "../.arena-dev/pmd/sprites.inc"

const struct ArenaSpriteSet *ArenaSprites_Get(u16 species)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sPmdSets); i++)
        if (sPmdSets[i].species == species) return &sPmdSets[i];
    return NULL;
}

u8 ArenaSprites_Frame(const struct ArenaSpriteAnimation *anim, u16 tick)
{
    u32 i;
    tick %= anim->totalTicks;
    for (i = 0; i + 1 < anim->frames && tick >= anim->durations[i]; i++)
        tick -= anim->durations[i];
    return i;
}
