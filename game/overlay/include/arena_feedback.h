#ifndef GUARD_ARENA_FEEDBACK_H
#define GUARD_ARENA_FEEDBACK_H

#include "arena_number.h"

void ArenaFeedback_Init(void);
void ArenaFeedback_Destroy(void);
void ArenaFeedback_Update(bool8 paused, bool8 frozen);
void ArenaFeedback_Impact(u8 target, s16 x, s16 y, u16 damage, u8 kind);
void ArenaFeedback_Wall(s16 x, s16 y);
void ArenaFeedback_Dust(s16 x, s16 y, bool8 dash);
void ArenaFeedback_Drain(u8 side,s16 x,s16 y,s16 sourceX,s16 sourceY,u16 healing);

struct ArenaFeedbackTelemetry
{
    u32 impacts, particlesSpawned, wallImpacts;
    u16 hitstop, liveParticles;
    u8 numberLife[2], attackBuffer, dashBuffer;
};
extern struct ArenaFeedbackTelemetry gArenaFeedbackTelemetry;

#endif
