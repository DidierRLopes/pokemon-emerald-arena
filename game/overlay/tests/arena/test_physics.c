#include "arena_physics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void reset(void){ArenaNav_Init();ArenaPhysics_Init();}
int main(void)
{
    unsigned i,j;
    struct ArenaFragment replay[ARENA_FRAGMENTS];
    struct ArenaPhysicsTelemetry telemetry;
    reset();
    assert(!ArenaNav_CanStand(120,77));
    ArenaPhysics_Hit(0,0,-900,2);
    assert(gArenaProps[0].hp==1&&ArenaNav_IsSolid(0));
    ArenaPhysics_Hit(0,0,-900,2);
    assert(gArenaProps[0].broken&&ArenaNav_CanStand(120,77));
    assert(ArenaNav_LineClear(120,98,120,57,0));
    for(i=0;i<130;i++)ArenaPhysics_Update();
    assert(gArenaPhysicsTelemetry.bounces>0&&gArenaPhysicsTelemetry.live==0);
    reset();
    ArenaPhysics_Hit(6,300,0,1);
    assert(gArenaProps[6].fuse==12&&ArenaNav_IsSolid(6));
    for(i=0;i<11;i++)ArenaPhysics_Update();
    assert(!gArenaProps[6].broken);
    ArenaPhysics_Update();
    assert(gArenaProps[6].broken&&gArenaProps[5].broken&&gArenaProps[2].broken);
    assert(gArenaPhysicsTelemetry.chains==2&&gArenaPhysicsTelemetry.blastSerial==1);
    for(i=0;i<37;i++)ArenaPhysics_Update();
    memcpy(replay,gArenaFragments,sizeof(replay));
    telemetry=gArenaPhysicsTelemetry;
    reset();ArenaPhysics_Hit(6,300,0,1);
    for(i=0;i<49;i++)ArenaPhysics_Update();
    assert(!memcmp(replay,gArenaFragments,sizeof(replay)));
    assert(!memcmp(&telemetry,&gArenaPhysicsTelemetry,sizeof(telemetry)));
    reset();
    for(i=0;i<ARENA_OBSTACLES;i++)ArenaPhysics_Hit(i,800,-600,3);
    for(i=0;i<160;i++)
    {
        ArenaPhysics_Update();
        assert(gArenaPhysicsTelemetry.live<=ARENA_FRAGMENTS);
        for(j=0;j<ARENA_FRAGMENTS;j++)
        {
            const struct ArenaFragment*f=&gArenaFragments[j];
            assert(f->z>=0&&f->x>=4*256&&f->x<=235*256);
            assert(f->y>=22*256&&f->y<=155*256);
        }
    }
    assert(gArenaPhysicsTelemetry.broken==7&&gArenaPhysicsTelemetry.solidMask==0);
    assert(gArenaPhysicsTelemetry.recycled>0&&gArenaPhysicsTelemetry.live==0);
    reset();
    gArenaProps[0].reserved=1;ArenaNav_SetObstacle(0,0);
    ArenaPhysics_Hit(0,800,0,3);
    assert(!gArenaProps[0].broken&&gArenaProps[0].hp==3);
    ArenaPhysics_ShatterAt(0,70,100,800,0);
    assert(gArenaProps[0].broken&&!gArenaProps[0].reserved&&!ArenaNav_IsSolid(0));
    assert(gArenaPhysicsTelemetry.spawned==8&&gArenaPhysicsTelemetry.broken==1);
    for(i=0;i<8;i++)assert(gArenaFragments[i].x==70*256&&gArenaFragments[i].y==100*256);
    ArenaPhysics_ShatterAt(0,100,100,0,0);
    assert(gArenaPhysicsTelemetry.spawned==8&&gArenaPhysicsTelemetry.broken==1);
    puts("PASS: reserved stones ignore ground hits, shatter at real impact, consume once");
    puts("PASS: material strength, solid removal, rebound, delayed blast, chain, determinism, bounded pool and rest");
    return 0;
}
