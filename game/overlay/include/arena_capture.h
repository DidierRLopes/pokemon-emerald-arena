#ifndef GUARD_ARENA_CAPTURE_H
#define GUARD_ARENA_CAPTURE_H

enum {
    ARENA_CAPTURE_IDLE, ARENA_CAPTURE_AIM, ARENA_CAPTURE_THROW,
    ARENA_CAPTURE_ABSORB, ARENA_CAPTURE_DROP, ARENA_CAPTURE_SHAKE,
    ARENA_CAPTURE_BREAK, ARENA_CAPTURE_CAUGHT, ARENA_CAPTURE_MISS
};
struct ArenaCaptureTelemetry {
    u32 throws, hits, misses, escaped, caught, cancelled, empty, full;
    u16 balls, age, species, hp;
    s16 aimX, aimY, ballX, ballY;
    u8 state, shakes, destination, ballSprite;
};
extern struct ArenaCaptureTelemetry gArenaCaptureTelemetry;
// Shared with the original battle scripts, including their RNG consumption.
u32 CalculateBallCatchOdds(u8 target, u16 item);
u8 CalculateBallCatchShakes(u32 odds, u16 item);
void RealtimeArena_FinishCapture(void);

#endif
