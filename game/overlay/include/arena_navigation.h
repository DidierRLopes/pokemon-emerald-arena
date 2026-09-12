#ifndef GUARD_ARENA_NAVIGATION_H
#define GUARD_ARENA_NAVIGATION_H

#ifdef ARENA_NAV_HOST
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t s32;
#else
#include "global.h"
#endif

#define ARENA_MIN_X 12
#define ARENA_MAX_X 228
#define ARENA_MIN_Y 32
#define ARENA_MAX_Y 143
#define ARENA_BODY_RADIUS 10
#define ARENA_OBSTACLES 7
#define ARENA_CORNERS (ARENA_OBSTACLES * 4)

struct ArenaRect { s16 left, top, right, bottom; };
struct ArenaPoint { s16 x, y; };
extern const struct ArenaRect gArenaObstacles[ARENA_OBSTACLES];

void ArenaNav_Init(void);
void ArenaNav_SetObstacle(u8 index,u8 solid);
u8 ArenaNav_IsSolid(u8 index);
s16 ArenaNav_FirstObstacle(s16 x1,s16 y1,s16 x2,s16 y2,u8 radius);
u8 ArenaNav_CanStand(s16 x, s16 y);
u8 ArenaNav_LineClear(s16 x1, s16 y1, s16 x2, s16 y2, u8 radius);
u8 ArenaNav_NextWaypoint(s16 x, s16 y, s16 goalX, s16 goalY, struct ArenaPoint *next);
struct ArenaPoint ArenaNav_Corner(u8 index);
u16 ArenaNav_Distance(s16 x1, s16 y1, s16 x2, s16 y2);

#endif
