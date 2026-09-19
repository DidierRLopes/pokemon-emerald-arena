#include "arena_moves.h"
#include "constants/moves.h"

// Native move identity and battle data are not duplicated here. These values
// describe ONLY the real-time action. Animation IDs match arena_sprites.h.
static const struct ArenaMoveProfile sProfiles[] =
{
    {MOVE_POUND,        0, ARENA_MOVE_MELEE,      3, 8, 6,26, 36, 0, ARENA_VIS_ARC,    ARENA_MOVE_CREAM, 1},
    {MOVE_TACKLE,     896, ARENA_MOVE_RUSH,       3,14,14,44, 50,32, ARENA_VIS_RUSH,   ARENA_MOVE_CREAM, 1},
    {MOVE_QUICK_ATTACK,1536,ARENA_MOVE_RUSH,      3, 5,14,44, 84,32, ARENA_VIS_RUSH,   ARENA_MOVE_WHITE, 1},
    {MOVE_ABSORB,     576, ARENA_MOVE_PROJECTILE, 2,16, 1,55,132,11, ARENA_VIS_ABSORB, ARENA_MOVE_GREEN, 1},
    {MOVE_LEER,         0, ARENA_MOVE_CONE,       4,18,12,62, 64, 0, ARENA_VIS_LEER,   ARENA_MOVE_PURPLE,2},
    {MOVE_WATER_GUN, 1024, ARENA_MOVE_PROJECTILE, 2,12, 1,36,160,10, ARENA_VIS_WATER,  ARENA_MOVE_BLUE,  1},
    {MOVE_WING_ATTACK, 0, ARENA_MOVE_MELEE,      3,10, 8,36, 42, 0, ARENA_VIS_WING,   ARENA_MOVE_WHITE, 1},
    {MOVE_SLAM,        0, ARENA_MOVE_MELEE,      3,16, 8,44, 40, 0, ARENA_VIS_SLAM,   ARENA_MOVE_CREAM, 1},
    {MOVE_PECK,        0, ARENA_MOVE_MELEE,      3, 6, 5,22, 30, 0, ARENA_VIS_PECK,   ARENA_MOVE_WHITE, 2},
    {MOVE_SCRATCH,     0, ARENA_MOVE_MELEE,      3, 6, 5,23, 32, 0, ARENA_VIS_CLAW,   ARENA_MOVE_CREAM, 1},
    {MOVE_LEAF_BLADE,  0, ARENA_MOVE_MELEE,      3,10, 7,39, 44, 0, ARENA_VIS_LEAF,   ARENA_MOVE_GREEN, 1},
    {MOVE_MACH_PUNCH,1408,ARENA_MOVE_RUSH,       3, 4,10,32, 54,27, ARENA_VIS_RUSH,   ARENA_MOVE_WHITE, 2},
    {MOVE_MEGA_DRAIN,640, ARENA_MOVE_PROJECTILE, 2,17, 1,62,140,12, ARENA_VIS_ABSORB, ARENA_MOVE_GREEN, 1},
    {MOVE_BITE,        0, ARENA_MOVE_MELEE,      3,10, 6,44, 35, 0, ARENA_VIS_BITE,   ARENA_MOVE_PURPLE,1},
    {MOVE_FAINT_ATTACK,0, ARENA_MOVE_MELEE,      3,12, 8,46, 42, 0, ARENA_VIS_ARC,    ARENA_MOVE_PURPLE,1},
    {MOVE_NIGHT_SHADE,640,ARENA_MOVE_PROJECTILE, 4,21, 1,72,150,12, ARENA_VIS_SHADE,  ARENA_MOVE_PURPLE,1},
    {MOVE_ACID,      768, ARENA_MOVE_PROJECTILE, 2,15, 1,49,144,11, ARENA_VIS_ACID,   ARENA_MOVE_PURPLE,1}
};

const struct ArenaMoveProfile *ArenaMoves_Get(u16 move)
{
    unsigned i;
    for (i = 0; i < sizeof(sProfiles)/sizeof(sProfiles[0]); i++)
        if (sProfiles[i].move == move) return &sProfiles[i];
    return 0;
}

bool8 ArenaMoves_InCone(s16 dx, s16 dy, s16 aimX, s16 aimY, u8 range, u8 cone)
{
    s32 dot = dx * aimX + dy * aimY;
    s32 cross = dx * aimY - dy * aimX;
    if (cross < 0) cross = -cross;
    return dx * dx + dy * dy <= range * range && dot > 0 && dot >= cross * cone;
}

bool8 ArenaMoves_SegmentHit(s16 x1, s16 y1, s16 x2, s16 y2,
                           s16 targetX, s16 targetY, u8 radius)
{
    // Integer closest-point distance. Used for fast bolts and rushes so a
    // target cannot be skipped between two hardware frames.
    s32 dx = x2-x1, dy = y2-y1, tx = targetX-x1, ty = targetY-y1;
    s32 length = dx*dx + dy*dy, along = tx*dx + ty*dy;
    if (length && along > 0)
    {
        if (along > length) along = length;
        tx -= dx * along / length;
        ty -= dy * along / length;
    }
    return tx*tx + ty*ty <= radius*radius;
}
