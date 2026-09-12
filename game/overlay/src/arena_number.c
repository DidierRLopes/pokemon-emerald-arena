#include "arena_number.h"

// Original 3x5 glyphs: digits, F A L O N D E - +.
static const u8 sGlyphs[][5] =
{
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
    {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7},
    {7,4,6,4,4}, {2,5,7,5,5}, {4,4,4,4,7}, {2,5,5,5,2}, {5,7,7,5,5},
    {6,5,5,5,6}, {7,4,6,4,7}, {0,0,7,0,0}, {0,2,7,2,0}
};
static const u8 sReverse[8] = {0,4,2,6,1,5,3,7};
// Expand eight occupancy bits into the low bit of eight 4bpp pixels.
// A small ROM table replaces 256 pixel-neighbor branches on every impact.
#define E(n) (((n)&1) | (((n)&2)<<3) | (((n)&4)<<6) | (((n)&8)<<9) \
    | (((n)&16)<<12) | (((n)&32)<<15) | (((n)&64)<<18) | (((u32)(n)&128)<<21))
#define R(n) E(n),E(n+1),E(n+2),E(n+3),E(n+4),E(n+5),E(n+6),E(n+7), \
    E(n+8),E(n+9),E(n+10),E(n+11),E(n+12),E(n+13),E(n+14),E(n+15)
static const u32 sExpand[256] =
{
    R(0),R(16),R(32),R(48),R(64),R(80),R(96),R(112),
    R(128),R(144),R(160),R(176),R(192),R(208),R(224),R(240)
};
#undef R
#undef E

void ArenaNumber_Render(u32 *tiles, u16 damage, u8 kind)
{
    u8 glyphs[5];
    u32 mask[8] = {0};
    u32 count = 0, i, row, x;
    if (kind == ARENA_FEEDBACK_MISS)
    {
        glyphs[0]=10; glyphs[1]=11; glyphs[2]=12; glyphs[3]=12; glyphs[4]=13; count=5;
    }
    else if (kind == ARENA_FEEDBACK_IMMUNE) { glyphs[0]=14; glyphs[1]=13; count=2; }
    else if (kind == ARENA_FEEDBACK_DEFENSE) { glyphs[0]=15;glyphs[1]=16;glyphs[2]=10;glyphs[3]=17;count=4; }
    else
    {
        if (kind == ARENA_FEEDBACK_HEAL) glyphs[count++]=18;
        if (damage > 999) damage=999;
        if (damage >= 100) glyphs[count++]=damage/100;
        if (damage >= 10) glyphs[count++]=(damage/10)%10;
        glyphs[count++]=damage%10;
    }
    x=(32-(count*4-1))/2;
    for (i=0;i<count;i++)
        for (row=0;row<5;row++)
            mask[row+2] |= (u32)sReverse[sGlyphs[glyphs[i]][row]] << (x+i*4);
    for (row=0;row<8;row++)
    {
        u32 foreground=mask[row];
        u32 outline=(foreground<<1)|(foreground>>1);
        if (row) outline |= mask[row-1];
        if (row<7) outline |= mask[row+1];
        outline &= ~foreground;
        for (i=0;i<4;i++)
        {
            tiles[i*8+row]=sExpand[foreground&255] | (sExpand[outline&255]<<1);
            foreground >>= 8; outline >>= 8;
        }
    }
}
