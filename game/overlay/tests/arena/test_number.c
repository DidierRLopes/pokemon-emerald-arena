#include "arena_number.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Independent pixel-by-pixel reference: preserve the approved appearance.
static const u8 glyphs[][5] = {
    {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
    {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7},
    {7,4,6,4,4},{2,5,7,5,5},{4,4,4,4,7},{2,5,5,5,2},{5,7,7,5,5},
    {6,5,5,5,6},{7,4,6,4,7},{0,0,7,0,0},{0,2,7,2,0},
    {5,7,7,5,5},{7,2,2,2,7},{3,4,2,1,6},{6,5,6,4,4},
    {7,2,2,2,2},{5,5,6,5,5},{3,4,4,4,3},{6,5,6,5,6},
    {6,5,6,5,5},{5,5,5,5,7}
};
static void reference(u32 *tiles, unsigned damage, unsigned kind)
{
    u8 ids[5],mask[8][32]={{0}};
    unsigned n=0,i,y,x,color;
    if(kind==ARENA_FEEDBACK_MISS){ids[0]=19;ids[1]=20;ids[2]=21;ids[3]=21;n=4;}
    else if(kind==ARENA_FEEDBACK_IMMUNE){ids[0]=14;ids[1]=13;n=2;}
    else if(kind==ARENA_FEEDBACK_DEFENSE){ids[0]=15;ids[1]=16;ids[2]=10;ids[3]=17;n=4;}
    else if(kind==ARENA_FEEDBACK_BURN){ids[0]=26;ids[1]=27;ids[2]=14;n=3;}
    else if(kind==ARENA_FEEDBACK_CRIT){ids[0]=25;ids[1]=27;ids[2]=20;ids[3]=23;ids[4]=18;n=5;}
    else if(kind==ARENA_FEEDBACK_STAT){
        const u8 names[8][3]={{11,23,24},{11,23,24},{15,16,10},{21,22,15},{21,22,11},{21,22,15},{11,25,25},{11,25,25}};
        memcpy(ids,names[damage&7],3);ids[3]=(damage&256)?18:17;n=4;
    }
    else {
        if(kind==ARENA_FEEDBACK_HEAL)ids[n++]=18;
        if(damage>999)damage=999;
        if(damage>=100)ids[n++]=damage/100;
        if(damage>=10)ids[n++]=(damage/10)%10;
        ids[n++]=damage%10;
    }
    memset(tiles,0,128);
    for(i=0;i<n;i++)for(y=0;y<5;y++)for(x=0;x<3;x++)
        if(glyphs[ids[i]][y]&(4>>x))mask[y+2][(32-(n*4-1))/2+i*4+x]=1;
    for(y=0;y<8;y++)for(x=0;x<32;x++) {
        color=mask[y][x]?1:((x&&mask[y][x-1])||(x<31&&mask[y][x+1])
            ||(y&&mask[y-1][x])||(y<7&&mask[y+1][x]))?2:0;
        tiles[(x/8)*8+y]|=(u32)color<<((x%8)*4);
    }
}
int main(void)
{
    u32 fast[32],slow[32];
    unsigned kind,damage,n=0;
    for(kind=0;kind<=7;kind++)for(damage=0;damage<=65535;damage++){
        memset(fast,0xCD,sizeof(fast));
        ArenaNumber_Render(fast,damage,kind);reference(slow,damage,kind);
        assert(!memcmp(fast,slow,sizeof(fast)));n++;
    }
    printf("PASS: %u exact 4bpp number/status renderings, all u16 values and kinds\n",n);
    return 0;
}
