#ifndef GUARD_ARENA_RENDER_H
#define GUARD_ARENA_RENDER_H
#include "global.h"
void ArenaRender_Reset(void);
void ArenaRender_Copy(const void *src,void *dest,u16 size);
void ArenaRender_Ready(void);
void ArenaRender_Flush(void);
#endif
