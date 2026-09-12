#ifndef GUARD_ARENA_LAB_H
#define GUARD_ARENA_LAB_H
#if ARENA_LAB
struct ArenaLabMailbox
{
    u32 magic;
    u32 command;
    u16 species;
    u8 level;
    u8 classic;
    u32 result;
    u32 completed;
    u16 teamSpecies[6];
    u8 teamLevels[6];
    u8 teamCount;
};
extern struct ArenaLabMailbox gArenaLabMailbox;
void ArenaLab_Tick(void);
#endif
#endif
