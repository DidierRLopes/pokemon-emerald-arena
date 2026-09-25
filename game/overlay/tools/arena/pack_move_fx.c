// Code-native effects plus Kenney's CC0 trace (graphics/arena/cc0).
// Built offline: no trig, allocation or
// rasterization runs on the GBA. 8 facings, 4 phases, transparent 4bpp tiles.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#define RGB(r,g,b) ((r)|((g)<<5)|((b)<<10))
static unsigned char trace[512*512*4];
static unsigned sampleTrace(double x,double y)
{
    int ix=(int)(x*511),iy=(int)(y*511);
    unsigned alpha;
    if(ix<0||iy<0||ix>511||iy>511)return 0;
    alpha=trace[(iy*512+ix)*4+3];
    return alpha>180?1:alpha>55?2:0;
}
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
        else if (visual==7) // opposing jaws close on the target, not claw scratches
        {
            double gap=11-phase*3, forward=a-19;
            if(fabs(forward)<12 && fabs(fabs(b)-gap)<1.6)color=2;
            if(fabs(forward)<11 && fabs(b)<gap && fabs(b)>gap-4
                && fmod(forward+12,6)<3)color=1;
        }
        else if (visual==8) // Kenney CC0 trace, baked into a sharp green blade
        {
            double slash=a+b*.45-(12+phase*4);
            color=sampleTrace(.5+slash/14,.5+b/48);
            if(a>4 && fabs(b)<16 && fabs(slash+7)<.7)color=2;
        }
        else if (visual==9) // revolving leaf/seed, not a recolored bullet
        {
            double turn=phase*1.5707963, xx=dx*cos(turn)-dy*sin(turn), yy=dx*sin(turn)+dy*cos(turn);
            if (fabs(xx+yy*.65)<2.8 && fabs(yy)<6) color=2;
            if (r<2.7) color=1;
            if (fabs(xx+yy*.65)<.6 && fabs(yy)<5) color=1;
        }
        else if (visual==10) // elongated water jet with bright core
        {
            if(a>-7 && a<6 && fabs(b)<2.8-((a+7)/15))color=2;
            if(a>-6 && a<5 && fabs(b)<.9)color=1;
            if(a<-3 && fabs(b-(phase&1?4:-4))<1)color=2;
        }
        else if (visual==11) // hollow ghostly ring and drifting wisps
        {
            if (fabs(r-(4+phase%2))<1.4) color=2;
            if (fabs(r-4)<.7 && a>0) color=1;
            if (a<-3 && fabs(b-(phase-1.5))<.8) color=2;
        }
        else if (visual==12) // irregular acid glob, bright rim and small bubbles
        {
            if ((a-1)*(a-1)*.7+b*b<20) color=2;
            if ((a-2)*(a-2)+(b+2)*(b+2)<4) color=1;
            if ((a+5)*(a+5)+(b-(phase-1)*2)*(b-(phase-1)*2)<2) color=1;
        }
        else if(visual==13) // mud clod with two loose grains trailing behind
        {
            if(a*a*.8+b*b<20 && a<4)color=2;
            if((a-1)*(a-1)+(b+2)*(b+2)<3)color=1;
            if(a<-4 && a>-7 && fabs(b-(phase-1)*2)<1.2)color=2;
        }
        else if(visual==14) // faceted, rotating thrown stone
        {
            double qx=dx*cos(phase*.35)-dy*sin(phase*.35),qy=dx*sin(phase*.35)+dy*cos(phase*.35);
            if(fabs(qx)<5 && fabs(qy)<5 && fabs(qx)+fabs(qy)<7.5)color=2;
            if(qy<-1 && qx>-3 && qx<2 && fabs(qx)+fabs(qy)<6)color=1;
        }
        else if(visual==15) // small flame head and alternating tongues
        {
            if(a>-6 && a<4 && fabs(b)<(5-a)*.45)color=2;
            if(a>-3 && a<3 && fabs(b)<1.3)color=1;
            if(a<-3 && a>-7 && fabs(b-(phase&1?3:-3))<1.4)color=2;
        }
        else if(visual==16) // hollow rising bubbles, no solid acid glob
        {
            double rr=hypot(a-1,b),r2=hypot(a+4,b-3+(phase&1));
            if(fabs(rr-4)<1 || fabs(r2-2)<.8)color=2;
            if(fabs(rr-4)<.8 && b<-1)color=1;
        }
        else if(visual==17) // three curved wind strokes
        {
            if(a>-5 && a<5 && fabs(b-(a*a/12-3))<1)color=1;
            if(a>-6 && a<3 && fabs(b-(a*a/14+1))<1)color=2;
            if(a>-4 && a<3 && fabs(b-5)<.8)color=2;
        }
        else if(visual==18) // four-point spinning Swift star
        {
            double qx=dx*cos(phase*.4)-dy*sin(phase*.4),qy=dx*sin(phase*.4)+dy*cos(phase*.4);
            if(fabs(qx)*fabs(qy)<3 && r<7)color=2;
            if(fabs(qx)+fabs(qy)<3)color=1;
        }
        if (color)
        {
            if(visual==15)color+=2; // Shared fire feedback palette: yellow/red.
            size_t p=((y/8)*(size/8)+x/8)*32+(y%8)*4+(x%8)/2;
            tiles[p]|=color<<((x&1)*4);
        }
    }
    if (fwrite(tiles,1,(size_t)size*size/2,f)!=(size_t)size*size/2) exit(2);
}
// Seismic Toss crater, two 64x32 frames: fresh with loose debris, then
// settled. Terrain palette: 1 pit, 6 dark soil, 7 soil, 9 lit earth, 15 pale
// crest, 2/3 grey stones. Deterministic; nothing is computed on the GBA.
static void crater(FILE *f, int settled)
{
    uint8_t tiles[1024] = {0};
    uint8_t pixels[32][64] = {{0}};
    static const double cracks[7] = {-2.7, -2.05, -1.3, -0.5, 0.35, 1.25, 2.2};
    static const int pebbles[10][3] = {{-27,-5,3},{26,-7,2},{-20,11,2},{24,9,3},{-6,-13,3},
        {10,13,2},{-29,3,2},{29,1,3},{3,-14,2},{-14,-12,3}};
    int x, y, k, i;
    for (y = 0; y < 32; y++) for (x = 0; x < 64; x++)
    {
        double dx = x - 31.5, dy = y - 15.5, nx = dx / 24.0, ny = dy / 11.0, r = hypot(nx, ny);
        unsigned color = 0;
        if (r < 0.9)
        {
            color = 7;                                   /* bowl floor */
            if (dy > 1.0 && r > 0.5) color = 9;           /* lit lower wall */
            if (r < 0.66) color = 6;
            if (dy < 0 && r > 0.4) color = 6;             /* shadowed upper wall */
            if (r < 0.3 || (dy < -2.0 && r > 0.66)) color = 1;
            if (dy > 2.5 && r > 0.8) color = 7;           /* lower lip in the bowl */
        }
        else if (r < 1.18)
        {
            color = dy < -0.5 ? 9 : dy > 2.0 ? 6 : 7;     /* raised rim: lit crest, dark foot */
            if (dy < -3.0 && dx < 6 && r > 0.98 && r < 1.1) color = 15;
            if (dy > 4.0 && r > 1.1) color = 1;
        }
        pixels[y][x] = color;
    }
    for (k = 0; k < 7; k++)
    {
        double sN, len = settled ? 0.22 : 0.34;
        int step = 0;
        for (sN = 1.12; sN < 1.12 + len; sN += 0.015, step++)
        {
            int px = (int)floor(31.5 + cos(cracks[k]) * 24.0 * sN + 0.5);
            int py = (int)floor(15.5 + sin(cracks[k]) * 11.0 * sN + 0.5);
            int jitter = ((k * 5 + step / 6) % 3) - 1;    /* jagged, not straight */
            if (fabs(cos(cracks[k])) > 0.7) py += jitter; else px += jitter;
            if (px < 0 || py < 0 || px > 63 || py > 31 || pixels[py][px]) continue;
            pixels[py][px] = (step / 6) % 3 == 2 ? 6 : 1;
        }
    }
    for (i = 0; i < (settled ? 4 : 10); i++)
    {
        int px = 31 + pebbles[i][0], py = 15 + pebbles[i][1];
        if (px < 0 || py < 0 || px > 63 || py > 31 || pixels[py][px]) continue;
        pixels[py][px] = pebbles[i][2];
        if (px < 63 && !pixels[py][px + 1] && i % 2 == 0) pixels[py][px + 1] = 2;
    }
    for (y = 0; y < 32; y++) for (x = 0; x < 64; x++)
        if (pixels[y][x])
        {
            size_t p = ((y / 8) * 8 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
            tiles[p] |= pixels[y][x] << ((x & 1) * 4);
        }
    if (fwrite(tiles, 1, sizeof(tiles), f) != sizeof(tiles)) exit(2);
}
// Seismic Toss finisher backdrop, in the spirit of the anime cut: the Earth
// seen from space on a starry black sky. One 4bpp tileset (blank, 10x10 globe
// tiles, three star tiles), a 32x32 BG tilemap using BG palette slot 1 and
// tile numbers from 192 (the free VRAM window behind the arena stage), and a
// 16-colour palette. Deterministic; nothing is computed on the GBA.
#define SPACE_TILE_BASE 192
#define GLOBE_TILES 10
static const uint16_t spacePalette[16] = {
    RGB(0,0,0), RGB(2,4,13), RGB(5,10,23), RGB(8,19,8), RGB(4,11,5),
    RGB(12,20,30), RGB(15,23,31), RGB(30,31,31), RGB(29,29,27), RGB(15,15,18),
    RGB(20,25,12), RGB(10,15,28), RGB(24,28,31), RGB(6,7,16), RGB(3,5,9), RGB(31,31,31)
};
static double landNoise(double lon, double lat)
{
    return .45 * sin(lon * 1.7 + .4) * cos(lat * 2.1)
         + .35 * sin(lon * 3.1 + lat * 2.4 + 1.3)
         + .30 * cos(lon * 4.3 - lat * 1.6 + .7)
         + .28 * sin(lon * 2.3 - lat * 3.1 + 2.0)
         + .22 * sin(lon * 6.1 + lat * 5.2)
         + .18 * cos(lat * 7.3 + lon * .9)
         + .15 * sin(lon * 9.1 - lat * 2.2 + .3);
}
static void space(FILE *tiles, FILE *map, FILE *pal)
{
    static uint8_t px[GLOBE_TILES * 8][GLOBE_TILES * 8];
    static uint8_t tile[104][32];
    static uint16_t entries[32 * 32];
    static const int stars[][3] = {{3,2,0},{27,3,1},{7,17,2},{24,18,0},{2,10,1},{28,10,2},{13,1,1},
        {19,19,0},{9,4,2},{22,1,1},{1,16,0},{29,15,1},{16,18,2},{5,8,1},{25,6,0},{11,14,1},{21,15,2},{8,13,0}};
    int size = GLOBE_TILES * 8, x, y, t, i, n;
    double radius = size / 2.0 - 1.0, lx = -.55, ly = -.45, lz = .70;
    for (y = 0; y < size; y++) for (x = 0; x < size; x++)
    {
        double u = (x + .5 - size / 2.0) / radius, v = (y + .5 - size / 2.0) / radius, rr = u * u + v * v;
        unsigned color = 0;
        if (rr <= 1.0)
        {
            double z = sqrt(1.0 - rr), lon = atan2(u, z), lat = asin(v), lit = u * lx + v * ly + z * lz;
            int land = landNoise(lon, lat) > .24, ice = fabs(lat) > 1.22;
            if (ice) color = lit > .15 ? 7 : 12;
            else if (land) color = lit > .5 ? 10 : lit > -.1 ? 3 : 4;
            else color = lit > .72 ? 5 : lit > .12 ? 2 : lit > -.3 ? 1 : 13;
            if (rr > .93 && lit > .2) color = 6;          /* bright atmosphere rim */
            if (rr > .93 && lit <= .2) color = lit > -.3 ? 11 : 14;
        }
        else if (rr <= 1.12 && (u * lx + v * ly) > .15) color = 11;   /* faint glow toward the light */
        px[y][x] = color;
    }
    memset(tile, 0, sizeof(tile));
    for (t = 0; t < GLOBE_TILES * GLOBE_TILES; t++)
        for (y = 0; y < 8; y++) for (x = 0; x < 8; x++)
        {
            unsigned c = px[(t / GLOBE_TILES) * 8 + y][(t % GLOBE_TILES) * 8 + x];
            tile[1 + t][y * 4 + x / 2] |= c << ((x & 1) * 4);
        }
    n = 1 + GLOBE_TILES * GLOBE_TILES;
    /* three star tiles: a bright dot, a dim dot and a four-point twinkle */
    tile[n][3 * 4 + 1] |= 8 << 4; n++;
    tile[n][5 * 4 + 2] |= 9; n++;
    tile[n][3 * 4 + 1] |= 15 << 4; tile[n][2 * 4 + 1] |= 9 << 4; tile[n][4 * 4 + 1] |= 9 << 4;
    tile[n][3 * 4 + 1] |= 9; tile[n][3 * 4 + 2] |= 9 << 4; n++;
    if (fwrite(tile, 32, n, tiles) != (size_t)n) exit(2);
    for (i = 0; i < 32 * 32; i++) entries[i] = (1 << 12) | SPACE_TILE_BASE;
    for (i = 0; i < (int)(sizeof(stars) / sizeof(stars[0])); i++)
        entries[stars[i][1] * 32 + stars[i][0]] = (1 << 12) | (SPACE_TILE_BASE + 1 + GLOBE_TILES * GLOBE_TILES + stars[i][2]) | ((i & 1) << 10);
    for (t = 0; t < GLOBE_TILES * GLOBE_TILES; t++)
        entries[(6 + t / GLOBE_TILES) * 32 + 10 + t % GLOBE_TILES] = (1 << 12) | (SPACE_TILE_BASE + 1 + t);
    if (fwrite(entries, 2, 32 * 32, map) != 32 * 32) exit(2);
    if (fwrite(spacePalette, 2, 16, pal) != 16) exit(2);
}
int main(int argc,char **argv)
{
    FILE *f;int v,d,p;
    png_image image;
    if(argc!=7) return 1;
    memset(&image,0,sizeof(image));image.version=PNG_IMAGE_VERSION;
    if(!png_image_begin_read_from_file(&image,"graphics/arena/cc0/trace_01.png"))return 3;
    image.format=PNG_FORMAT_RGBA;
    if(image.width!=512||image.height!=512||!png_image_finish_read(&image,0,trace,0,0))return 3;
    png_image_free(&image);
    f=fopen(argv[1],"wb");if(!f)return 2;
    for(v=0;v<9;v++)for(d=0;d<8;d++)for(p=0;p<4;p++)frame(f,v,d,p,64);
    fclose(f);
    f=fopen(argv[2],"wb");if(!f)return 2;
    for(v=9;v<19;v++)for(d=0;d<8;d++)for(p=0;p<4;p++)frame(f,v,d,p,16);
    fclose(f);
    f=fopen(argv[3],"wb");if(!f)return 2;
    crater(f,0);crater(f,1);
    fclose(f);
    {
        FILE *tiles=fopen(argv[4],"wb"),*map=fopen(argv[5],"wb"),*pal=fopen(argv[6],"wb");
        if(!tiles||!map||!pal)return 2;
        space(tiles,map,pal);
        fclose(tiles);fclose(map);fclose(pal);
    }
    return 0;
}
