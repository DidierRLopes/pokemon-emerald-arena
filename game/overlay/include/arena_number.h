#ifndef GUARD_ARENA_NUMBER_H
#define GUARD_ARENA_NUMBER_H
#ifdef ARENA_NUMBER_HOST
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#else
#include "global.h"
#endif

#define ARENA_FEEDBACK_DAMAGE 0
#define ARENA_FEEDBACK_MISS 1
#define ARENA_FEEDBACK_IMMUNE 2
#define ARENA_FEEDBACK_HEAL 3
#define ARENA_FEEDBACK_DEFENSE 4
#define ARENA_FEEDBACK_STAT 5
#define ARENA_FEEDBACK_BURN 6
#define ARENA_FEEDBACK_CRIT 7

// Four 8x8 4bpp tiles; colors 0 transparent, 1 foreground, 2 outline.
void ArenaNumber_Render(u32 *tiles, u16 damage, u8 kind);
#endif
