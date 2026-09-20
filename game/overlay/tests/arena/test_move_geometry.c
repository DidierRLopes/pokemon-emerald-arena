#include "arena_moves.h"
#include "constants/moves.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    int x,y,d,checks=0;
    const struct ArenaMoveProfile*p=ArenaMoves_Get(MOVE_POUND);
    assert(p&&p->kind==ARENA_MOVE_MELEE);
    assert(ArenaMoves_Get(MOVE_QUICK_ATTACK)->speed>ArenaMoves_Get(MOVE_TACKLE)->speed);
    assert(ArenaMoves_Get(MOVE_ABSORB)->kind==ARENA_MOVE_PROJECTILE);
    assert(!ArenaMoves_Get(MOVE_THUNDER));
    assert(ArenaMoves_Get(MOVE_FLAMETHROWER)->visual==ARENA_VIS_EMBER); // Native burn is tested separately.
    assert(ArenaMoves_Get(MOVE_WING_ATTACK)->visual!=p->visual);
    assert(ArenaMoves_Get(MOVE_SLAM)->windup>p->windup);
    assert(ArenaMoves_Get(MOVE_PECK)->cone==2);
    assert(ArenaMoves_Get(MOVE_SCRATCH)->range<p->range);
    assert(!ArenaMoves_InCone(0,-100,0,-256,p->range,1));
    assert(ArenaMoves_InCone(0,-30,0,-256,p->range,1));
    assert(!ArenaMoves_InCone(0,30,0,-256,p->range,1));
    assert(!ArenaMoves_InCone(30,0,0,-256,p->range,1));
    assert(ArenaMoves_SegmentHit(0,0,60,0,30,0,4));
    assert(!ArenaMoves_SegmentHit(0,0,60,0,30,8,4));
    for(d=0;d<2;d++)for(x=-240;x<=240;x+=4)for(y=-160;y<=160;y+=4)
    {
        int hit=ArenaMoves_InCone(x,y,0,d?256:-256,64,2);
        if(hit){assert(x*x+y*y<=64*64);assert(d?y>0:y<0);}
        checks++;
    }
    printf("PASS: %d cone cases, range/direction, swept collision and explicit profiles\n",checks);
    return 0;
}
