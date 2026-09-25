// Delayed real-time native moves. One paid attack, one native damage decision.
static const u32 sDiveFrames[]=INCBIN_U32("graphics/arena/dig-fly/effect.4bpp");
static void DiveStart(u8 side,u16 move)
{
    struct ArenaDive*d=&sDive[side];
    struct ArenaBody*b=&sArena.bodies[side];
    d->active=TRUE;d->fly=move==MOVE_FLY;d->age=0;
    d->ox=d->sx=b->x/Q;d->oy=d->sy=b->y/Q;
    b->actionLife=b->shotTimer=b->dash=0;b->knockX=b->knockY=0;
    SpecialFx(side,d->fly?4:3,d->ox,d->oy-10);
    gArenaDiveTelemetry[d->fly?1:0]++;
    PlaySE(d->fly?SE_M_RAZOR_WIND:SE_M_DIG);
}
static void DiveTick(void)
{
    u32 side;
    for(side=0;side<2;side++)
    {
        struct ArenaDive*d=&sDive[side];
        struct ArenaBody*b=&sArena.bodies[side],*t=&sArena.bodies[side^1];
        u16 age,impact;
        if(!d->active)continue;
        if(!gBattleMons[side].hp||sArena.resultTimer){d->active=FALSE;sWarpFx[side].life=0;continue;}
        age=++d->age;impact=d->fly?128:100;
        // Track for a bounded approach, then commit: the opponent can escape.
        if(age>=16&&age<(d->fly?104:80))
        {
            s16 x=b->x/Q,y=b->y/Q,tx=DecoyAim(side^1,FALSE)/Q,ty=DecoyAim(side^1,TRUE)/Q;
            s32 dx=tx-x,dy=ty-y,len=max(Abs(dx),Abs(dy))+min(Abs(dx),Abs(dy))/2;
            s32 nx,ny;
            if(!side&&JOY_HELD(DPAD_ANY))
            {dx=((JOY_HELD(DPAD_RIGHT)?1:0)-(JOY_HELD(DPAD_LEFT)?1:0))*Q;dy=((JOY_HELD(DPAD_DOWN)?1:0)-(JOY_HELD(DPAD_UP)?1:0))*Q;len=max(Abs(dx),Abs(dy))+min(Abs(dx),Abs(dy))/2;}
            if(len)
            {
                nx=b->x+dx*(d->fly?640:512)/len;ny=b->y+dy*(d->fly?640:512)/len;
                // Travel may pass under/over cover. Remember a safe landing.
                b->x=Clamp(nx,ARENA_MIN_X*Q,ARENA_MAX_X*Q);
                b->y=Clamp(ny,ARENA_MIN_Y*Q,ARENA_MAX_Y*Q);
                if(ArenaNav_CanStand(b->x/Q,b->y/Q)){d->sx=b->x/Q;d->sy=b->y/Q;}
            }
            sArena.thinkTimer=1;sArena.goalTimer=0;
        }
        if(age==(d->fly?104:80)&&!ArenaNav_CanStand(b->x/Q,b->y/Q))
        {b->x=d->sx*Q;b->y=d->sy*Q;gArenaDiveTelemetry[5]++;}
        sWarpFx[side].x=b->x/Q;sWarpFx[side].y=b->y/Q-10;
        if(age==(d->fly?110:88))PlaySE(d->fly?SE_M_RAZOR_WIND:SE_M_DIG);
        if(age==impact)
        {
            s16 x=b->x/Q,y=b->y/Q;
            u32 i;
            gArenaDiveTelemetry[3]++;
            if(sDecoys[side^1].life&&ArenaNav_Distance(x,y,sDecoys[side^1].x,sDecoys[side^1].y)<30)
                DecoyBreak(side^1);
            else if(sCover[side^1].hp&&ArenaNav_Distance(x,y,sCover[side^1].x/Q,sCover[side^1].y/Q)<30)
                CoverHit(side^1,side,d->fly?MOVE_FLY:MOVE_DIG,b->attackX,b->attackY);
            else if(ArenaNav_Distance(x,y,t->x/Q,t->y/Q)<32&&!t->dash)
                ApplyMoveHit(side,d->fly?MOVE_FLY:MOVE_DIG);
            else{gArenaDiveTelemetry[4]++;ArenaFeedback_Impact(side,x,y,0,ARENA_FEEDBACK_MISS);}
            for(i=0;i<ARENA_OBSTACLES;i++)
            {
                const struct ArenaRect*r=&gArenaObstacles[i];
                s16 dx=(r->left+r->right)/2-x,dy=(r->top+r->bottom)/2-y;
                if(dx*dx+dy*dy<36*36&&!gArenaProps[i].reserved)ArenaPhysics_Hit(i,dx*24,dy*24,3);
            }
            ArenaFeedback_Dust(x,y,TRUE);ArenaFeedback_Wall(x,y);
            sArena.hitstop=4;PlaySE(SE_M_STRENGTH);
        }
        if(age>=impact+24)
        {d->active=FALSE;sWarpFx[side].life=d->fly?0:64;b->cooldown=max(b->cooldown,24);}
    }
}
static u8 DiveFrame(u8 side)
{
    struct ArenaDive*d=&sDive[side];u16 a=d->age;
    if(d->fly)return a<24?16+min(7,a/3):a<116?23:24+min(7,(a-116)/4);
    return a<24?min(7,a/3):a<88?7:8+min(7,(a-88)/4);
}
static void DiveDraw(void)
{
    u32 side;
    for(side=0;side<2;side++)
    {
        struct ArenaDive*d=&sDive[side];struct ArenaBody*b=&sArena.bodies[side];
        struct Sprite*m=&gSprites[b->sprite],*shadow=&gSprites[b->shadow];
        u16 a=d->age;u8 fx=sWarpFx[side].sprite;
        if(!d->active)
        {
            if(d->fly&&shadow->oam.affineMode!=ST_OAM_AFFINE_OFF)
            {shadow->oam.affineMode=ST_OAM_AFFINE_OFF;shadow->oam.matrixNum=0;CalcCenterToCornerVec(shadow,shadow->oam.shape,shadow->oam.size,shadow->oam.affineMode);}
            continue;
        }
        if(d->fly)
        {
            s16 h=a<20?a*6:a<110?120:a<128?(128-a)*120/18:0;
            m->y-=h;m->invisible=a>=20&&a<110;
            shadow->invisible=FALSE;
            shadow->x=b->x/Q;shadow->y=b->y/Q+8;
            if(fx<MAX_SPRITES)
            {
                gSprites[fx].invisible=a>=24&&a<116;
                gSprites[fx].oam.priority=0;
                shadow->oam.affineMode=ST_OAM_AFFINE_DOUBLE;
                shadow->oam.matrixNum=gSprites[fx].oam.matrixNum;
                shadow->affineAnimPaused=TRUE;shadow->affineAnimBeginning=FALSE;
                CalcCenterToCornerVec(shadow,shadow->oam.shape,shadow->oam.size,shadow->oam.affineMode);
            }
        }
        else
        {
            m->invisible=a>=10&&a<94;
            if(a<10)m->y+=a;
            else if(a<108)m->y-=Sin((u8)((a-94)*128/14),18);
            shadow->invisible=a>=10&&a<100;
            if(fx<MAX_SPRITES)gSprites[fx].oam.priority=a<94?0:1;
        }
        if(fx<MAX_SPRITES)
        {
            s16 scale=d->fly?(a>=24&&a<116?128:208):256;
            SetOamMatrix(gSprites[fx].oam.matrixNum,scale,0,0,scale);
        }
        if(!side)gSprites[sArena.aimSprite].invisible=TRUE;
    }
}
