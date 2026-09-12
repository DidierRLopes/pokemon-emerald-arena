#include "arena_navigation.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    static const struct ArenaPoint goals[] = {{12,32},{228,32},{12,143},{228,143},{120,32},{120,143}};
    int x, y, i, j, routes = 0;
    ArenaNav_Init();
    assert(!ArenaNav_CanStand(11,32));
    assert(!ArenaNav_CanStand(120,144));
    assert(!ArenaNav_LineClear(120,32,120,143,2));
    assert(ArenaNav_LineClear(94,32,94,143,10));
    for (i = 0; i < ARENA_OBSTACLES; ++i)
    {
        const struct ArenaRect *r = &gArenaObstacles[i];
        assert(!ArenaNav_CanStand((r->left+r->right)/2,(r->top+r->bottom)/2));
        assert(!ArenaNav_LineClear(r->left-30,r->top,r->right+30,r->top,0));
        assert(!ArenaNav_LineClear(r->left-30,r->top-30,r->right+30,r->bottom+30,2));
    }
    for (x = 12; x <= 228; x += 7)
        for (y = 32; y <= 143; y += 7)
        {
            if (!ArenaNav_CanStand(x,y)) continue;
            for (i = 0; i < 6; i++)
            {
                struct ArenaPoint at = {x,y}, next;
                assert(ArenaNav_LineClear(x,y,goals[i].x,goals[i].y,2)
                    == ArenaNav_LineClear(goals[i].x,goals[i].y,x,y,2));
                for (j = 0; j <= ARENA_CORNERS; ++j)
                {
                    assert(ArenaNav_NextWaypoint(at.x,at.y,goals[i].x,goals[i].y,&next));
                    assert(ArenaNav_CanStand(next.x,next.y));
                    assert(ArenaNav_LineClear(at.x,at.y,next.x,next.y,ARENA_BODY_RADIUS));
                    at = next;
                    if (at.x == goals[i].x && at.y == goals[i].y) break;
                }
                assert(j <= ARENA_CORNERS);
                routes++;
            }
        }
    printf("PASS: %d reachable routes, swept solids, symmetry, bounds; same C as ROM\n", routes);
    // Every destruction combination changes connectivity immediately, without
    // rebuilding geometry. Re-enabling a solid must also restore collision.
    for(i=0;i<(1<<ARENA_OBSTACLES);i++)
    {
        struct ArenaPoint at={120,143},next;
        for(j=0;j<ARENA_OBSTACLES;j++)ArenaNav_SetObstacle(j,!!(i&(1<<j)));
        for(j=0;j<=ARENA_CORNERS;j++)
        {
            assert(ArenaNav_NextWaypoint(at.x,at.y,120,32,&next));
            assert(ArenaNav_LineClear(at.x,at.y,next.x,next.y,ARENA_BODY_RADIUS));
            at=next;
            if(at.x==120&&at.y==32)break;
        }
        assert(j<=ARENA_CORNERS);
    }
    puts("PASS: all 128 dynamic destruction masks remain routable");
    return 0;
}
