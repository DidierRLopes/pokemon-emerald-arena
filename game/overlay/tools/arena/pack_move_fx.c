// Original code-native effect artwork. Built offline: no trig, allocation or
// rasterization runs on the GBA. 8 facings, 4 phases, transparent 4bpp tiles.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const double directions[8][2] = {
    {0,1},{.7071,.7071},{1,0},{.7071,-.7071},
    {0,-1},{-.7071,-.7071},{-1,0},{-.7071,.7071}
};
static void frame(FILE *f, int visual, int direction, int phase, int size)
{
    uint8_t tiles[2048] = {0};
    double ux=directions[direction][0], uy=directions[direction][1];
    int x,y;
    for (y=0;y<size;y++) for(x=0;x<size;x++)
    {
        double dx=x-(size-1)*.5, dy=y-(size-1)*.5;
        double a=dx*ux+dy*uy, b=dx*-uy+dy*ux, r=hypot(dx,dy);
        unsigned color=0;
        if (visual==0) // crescent sweep, moving from one edge to the other
        {
            double angle=atan2(b,a), center=-.70+phase*.46;
            if (a>0 && fabs(angle-center)<.60 && r>23 && r<31)
                color=r>26 && r<29 ? 1 : 2;
            if (a>0 && fabs(angle-center)<.50 && r>18 && r<20) color=2;
        }
        else if (visual==1) // committed rush streaks behind the actor
        {
            if (a < -3 && a > -28+phase*2 &&
                (fabs(b)<1.2 || fabs(fabs(b)-6)<.9)) color=fabs(b)<1.2?1:2;
        }
        else if (visual==2) // cone origin lies 32 px behind this sprite center
        {
            double forward=a+32, radius=hypot(forward,b), ring=26+phase*12;
            if (forward>0 && forward>=fabs(b)*2 && fabs(radius-ring)<2)
                color=fabs(radius-ring)<.8?1:2;
            if (forward>3 && forward<14 && fabs(fabs(b)-3)<2 && phase<2) color=1;
        }
        else if (visual==3) // two feathered wing sweeps, expanding forward
        {
            double spread=9+phase*3, edge=15+fabs(b)*.35;
            if(a>4 && a<30 && fabs(b)<spread && fabs(a-edge)<2.3)color=1;
            if(a>6 && a<24 && fabs(b)>3 && fabs(b)<spread
                && fabs(fmod(fabs(b)+a*.45,5))<1.2)color=2;
        }
        else if (visual==4) // heavy palm/ground impact, short radial spikes
        {
            double rr=hypot(a-18,b), ring=6+phase*3;
            if(a>2 && fabs(rr-ring)<2)color=fabs(rr-ring)<.8?1:2;
            if(a>2 && rr>ring && rr<ring+6 &&
                (fabs(b)<1.2 || fabs(a-18)<1.2 || fabs(fabs(a-18)-fabs(b))<1.2))color=2;
        }
        else if (visual==5) // narrow pointed peck, no broad sweep
        {
            double tip=18+phase*3;
            if(a>7 && a<tip && fabs(b)<(tip-a)*.22)color=fabs(b)<1?1:2;
        }
        else if (visual==6) // three parallel claw cuts, staggered in time
        {
            double slash=a+b*.45-(13+phase*3);
            if(a>4 && a<30 && fabs(b)<15 &&
                (fabs(slash)<1.2 || fabs(slash-7)<1.2 || fabs(slash+7)<1.2))color=fabs(b)<10?1:2;
        }
        else if (visual==7) // revolving leaf/seed, not a recolored bullet
        {
            double turn=phase*1.5707963, xx=dx*cos(turn)-dy*sin(turn), yy=dx*sin(turn)+dy*cos(turn);
            if (fabs(xx+yy*.65)<2.8 && fabs(yy)<6) color=2;
            if (r<2.7) color=1;
            if (fabs(xx+yy*.65)<.6 && fabs(yy)<5) color=1;
        }
        else // water head and separated trailing droplets
        {
            if ((a-2)*(a-2)+b*b<15 || (a+4)*(a+4)*2+b*b<4) color=2;
            if ((a-2)*(a-2)+(b+1)*(b+1)<5) color=1;
            if (phase&1 && a<-5 && fabs(b)<1.5) color=1;
        }
        if (color)
        {
            size_t p=((y/8)*(size/8)+x/8)*32+(y%8)*4+(x%8)/2;
            tiles[p]|=color<<((x&1)*4);
        }
    }
    if (fwrite(tiles,1,(size_t)size*size/2,f)!=(size_t)size*size/2) exit(2);
}
int main(int argc,char **argv)
{
    FILE *f;int v,d,p;
    if(argc!=3) return 1;
    f=fopen(argv[1],"wb");if(!f)return 2;
    for(v=0;v<7;v++)for(d=0;d<8;d++)for(p=0;p<4;p++)frame(f,v,d,p,64);
    fclose(f);
    f=fopen(argv[2],"wb");if(!f)return 2;
    for(v=7;v<9;v++)for(d=0;d<8;d++)for(p=0;p<4;p++)frame(f,v,d,p,16);
    fclose(f);
    return 0;
}
