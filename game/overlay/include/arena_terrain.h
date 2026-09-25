#ifndef GUARD_ARENA_TERRAIN_H
#define GUARD_ARENA_TERRAIN_H
#include "global.h"
void ArenaTerrain_Init(void);
bool8 ArenaTerrain_ReserveProjectile(void);
void ArenaTerrain_Draw(bool8 paused,bool8 frozen);
// Ground decal left by a Seismic Toss slam; one at a time, kept until the exit.
void ArenaTerrain_Crater(s16 x,s16 y);
#endif
