#include "arena_physics.h"
#ifdef ARENA_NAV_HOST
#include <string.h>
#define EWRAM_DATA
#else
#include "global.h"
#endif
#define Q 256
EWRAM_DATA struct ArenaProp gArenaProps[ARENA_OBSTACLES]={};
EWRAM_DATA struct ArenaFragment gArenaFragments[ARENA_FRAGMENTS]={};
EWRAM_DATA struct ArenaPhysicsTelemetry gArenaPhysicsTelemetry={};
static EWRAM_DATA u32 sRandom=0;
static EWRAM_DATA u8 sCursor=0;
static const u8 sKinds[ARENA_OBSTACLES]={0,0,0,1,2,3,4};
static const u8 sStrength[ARENA_OBSTACLES]={3,2,2,1,1,2,1};
static s32 Abs(s32 n){return n<0?-n:n;}
static s32 Clamp(s32 n,s32 lo,s32 hi){return n<lo?lo:n>hi?hi:n;}
static u16 NextRandom(void){sRandom=sRandom*1664525+1013904223;return sRandom>>16;}
static void Fragment(u8 kind,s16 x,s16 y,s16 ix,s16 iy)
{
    u16 i;
    struct ArenaFragment *f;
    for(i=0;i<ARENA_FRAGMENTS;i++)if(!gArenaFragments[i].life)break;
    if(i==ARENA_FRAGMENTS){i=sCursor++%ARENA_FRAGMENTS;gArenaPhysicsTelemetry.recycled++;}
    f=&gArenaFragments[i];
    f->x=x*Q;f->y=y*Q;f->z=(2+(NextRandom()&3))*Q;
    f->vx=Clamp(ix/2+(s16)(NextRandom()&1023)-512,-1000,1000);
    f->vy=Clamp(iy/2+(s16)(NextRandom()&1023)-512,-900,900);
    f->vz=512+(NextRandom()&511);
    if(kind==ARENA_PROP_BUSH)f->vz/=2;
    f->kind=kind;f->life=100+(NextRandom()&31);f->age=0;f->bounces=0;
    gArenaPhysicsTelemetry.spawned++;
}
static void Break(u8 index,s16 ix,s16 iy)
{
    struct ArenaProp *p=&gArenaProps[index];
    const struct ArenaRect *r=&gArenaObstacles[index];
    s16 x=(r->left+r->right)/2,y=(r->top+r->bottom)/2;
    u16 i,count=p->kind==ARENA_PROP_POD?12:p->kind==ARENA_PROP_WOOD?6:8;
    p->broken=1;p->hp=0;p->fuse=0;p->revision++;
    ArenaNav_SetObstacle(index,0);
    gArenaPhysicsTelemetry.hp[index]=0;gArenaPhysicsTelemetry.fuse[index]=0;
    gArenaPhysicsTelemetry.solidMask&=~(1<<index);
    gArenaPhysicsTelemetry.broken++;
    for(i=0;i<count;i++)Fragment(p->kind,x,y,ix,iy);
    if(p->kind==ARENA_PROP_POD)
    {
        gArenaPhysicsTelemetry.blastSerial++;
        gArenaPhysicsTelemetry.blastX=x;gArenaPhysicsTelemetry.blastY=y;
        for(i=0;i<ARENA_OBSTACLES;i++)
        {
            const struct ArenaRect *other=&gArenaObstacles[i];
            s16 dx=(other->left+other->right)/2-x,dy=(other->top+other->bottom)/2-y;
            if(i!=index&&!gArenaProps[i].broken&&dx*dx+dy*dy<46*46)
            {
                gArenaPhysicsTelemetry.chains++;
                ArenaPhysics_Hit(i,dx*24,dy*24,2);
            }
        }
    }
}
void ArenaPhysics_Init(void)
{
    u16 i;
    memset(gArenaProps,0,sizeof(gArenaProps));
    memset(gArenaFragments,0,sizeof(gArenaFragments));
    memset(&gArenaPhysicsTelemetry,0,sizeof(gArenaPhysicsTelemetry));
    sRandom=0xA7E3D911;sCursor=0;
    for(i=0;i<ARENA_OBSTACLES;i++)
    {
        gArenaProps[i].kind=sKinds[i];
        gArenaProps[i].hp=gArenaProps[i].maxHp=sStrength[i];
        gArenaPhysicsTelemetry.hp[i]=sStrength[i];
    }
    gArenaPhysicsTelemetry.solidMask=(1<<ARENA_OBSTACLES)-1;
}
void ArenaPhysics_Hit(u8 index,s16 ix,s16 iy,u8 strength)
{
    struct ArenaProp *p;
    if(index>=ARENA_OBSTACLES||!strength)return;
    p=&gArenaProps[index];
    if(p->broken||p->fuse)return;
    p->flash=6;p->revision++;gArenaPhysicsTelemetry.impacts++;
    if(p->hp>strength)p->hp-=strength;
    else if(p->kind==ARENA_PROP_POD){p->hp=0;p->fuse=12;}
    else Break(index,ix,iy);
    gArenaPhysicsTelemetry.hp[index]=p->hp;
    gArenaPhysicsTelemetry.fuse[index]=p->fuse;
}
void ArenaPhysics_Update(void)
{
    u16 i;
    gArenaPhysicsTelemetry.steps++;
    gArenaPhysicsTelemetry.live=0;gArenaPhysicsTelemetry.solidMask=0;
    for(i=0;i<ARENA_OBSTACLES;i++)
    {
        struct ArenaProp *p=&gArenaProps[i];
        if(p->flash)p->flash--;
        if(p->fuse&&--p->fuse==0)Break(i,0,0);
    }
    for(i=0;i<ARENA_OBSTACLES;i++)
    {
        struct ArenaProp *p=&gArenaProps[i];
        gArenaPhysicsTelemetry.hp[i]=p->hp;gArenaPhysicsTelemetry.fuse[i]=p->fuse;
        if(ArenaNav_IsSolid(i))gArenaPhysicsTelemetry.solidMask|=1<<i;
    }
    for(i=0;i<ARENA_FRAGMENTS;i++)
    {
        struct ArenaFragment *f=&gArenaFragments[i];
        s32 nx,ny;
        if(!f->life)continue;
        f->life--;f->age++;
        nx=f->x+f->vx;ny=f->y+f->vy;
        if(f->vx&&(nx<4*Q||nx>235*Q||(f->z<7*Q&&!ArenaNav_LineClear(f->x/Q,f->y/Q,nx/Q,f->y/Q,1))))
        {f->vx=-f->vx/2;gArenaPhysicsTelemetry.bounces++;}
        else f->x=nx;
        if(f->vy&&(ny<22*Q||ny>155*Q||(f->z<7*Q&&!ArenaNav_LineClear(f->x/Q,f->y/Q,f->x/Q,ny/Q,1))))
        {f->vy=-f->vy/2;gArenaPhysicsTelemetry.bounces++;}
        else f->y=ny;
        f->z+=f->vz;
        f->vz-=f->kind==ARENA_PROP_BUSH?12:42;
        if(f->z<=0)
        {
            f->z=0;
            if(f->vz<-100&&f->bounces<3)
            {
                f->vz=-f->vz*(f->kind==ARENA_PROP_CRYSTAL?150:90)/256;
                f->bounces++;gArenaPhysicsTelemetry.bounces++;
            }
            else f->vz=0;
            f->vx=f->vx*180/256;f->vy=f->vy*180/256;
        }
        else {f->vx=f->vx*251/256;f->vy=f->vy*251/256;}
        if(Abs(f->vx)<6)f->vx=0;
        if(Abs(f->vy)<6)f->vy=0;
        if(f->life)gArenaPhysicsTelemetry.live++;
    }
    if(gArenaPhysicsTelemetry.live>gArenaPhysicsTelemetry.peak)gArenaPhysicsTelemetry.peak=gArenaPhysicsTelemetry.live;
}
