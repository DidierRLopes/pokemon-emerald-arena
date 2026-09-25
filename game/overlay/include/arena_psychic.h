#ifndef GUARD_ARENA_PSYCHIC_H
#define GUARD_ARENA_PSYCHIC_H
#include "global.h"
#include "arena_navigation.h"
struct ArenaPsychicRock {
    s32 x,y;
    s16 vx,vy;
    u8 state,side,order,age,height;
};
extern struct ArenaPsychicRock gArenaPsychicRocks[ARENA_OBSTACLES];
extern u32 gArenaPsychicTelemetry[8]; // casts, lifted, launched, impacts, contacts, blocked, missed, ticks
void ArenaPsychicFx_Init(bool8 enabled);
void ArenaPsychicFx_Impact(s16 x,s16 y);
void ArenaPsychicFx_Draw(bool8 paused,bool8 frozen);
void ArenaMoveFx_Psychic(u8 side,s16 x,s16 y,u8 frame,bool8 visible);
#endif
