#ifndef GUARD_ARENA_PHYSICS_H
#define GUARD_ARENA_PHYSICS_H
#include "arena_navigation.h"
#define ARENA_FRAGMENTS 32
enum { ARENA_PROP_ROCK,ARENA_PROP_WOOD,ARENA_PROP_BUSH,ARENA_PROP_CRYSTAL,ARENA_PROP_POD };
struct ArenaProp
{
    u8 kind,hp,maxHp,flash,fuse,broken,revision,reserved;
};
struct ArenaFragment
{
    s32 x,y,z;
    s16 vx,vy,vz;
    u8 kind,life,age,bounces;
};
struct ArenaPhysicsTelemetry
{
    u32 impacts,broken,spawned,recycled,bounces,chains,blastSerial,steps;
    s16 blastX,blastY;
    u16 live,peak;
    u8 hp[ARENA_OBSTACLES],fuse[ARENA_OBSTACLES];
    u8 solidMask,reserved;
};
extern struct ArenaProp gArenaProps[ARENA_OBSTACLES];
extern struct ArenaFragment gArenaFragments[ARENA_FRAGMENTS];
extern struct ArenaPhysicsTelemetry gArenaPhysicsTelemetry;
void ArenaPhysics_Init(void);
// Environment strength is independent of Pokemon HP/XP; one event per cast.
void ArenaPhysics_Hit(u8 index,s16 impulseX,s16 impulseY,u8 strength);
void ArenaPhysics_Update(void);
#endif
