#ifndef GUARD_ARENA_TERRAIN_H
#define GUARD_ARENA_TERRAIN_H
#include "global.h"
extern u8 gArenaBiome;
void ArenaTerrain_Init(void);
void ArenaTerrain_Crater(s16 x,s16 y);
bool8 ArenaTerrain_ReserveProjectile(void);
void ArenaTerrain_Draw(bool8 paused,bool8 frozen);
#endif
