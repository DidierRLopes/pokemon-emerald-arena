// Compile an approved background into native GBA mode-0 8bpp tiles.
// Technical resize/color conversion only; artwork is created by image_gen.
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned histogram[32768];
static uint16_t pixels[240*160], colors[240];
static unsigned char tiles[240*160], palette[512], map[2048];
static unsigned short lookup[32768];
static void die(const char *s) { fprintf(stderr,"%s\n",s); exit(1); }
static void output(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path,"wb");
    if (!f || fwrite(data,1,size,f) != size) die("Cannot write background asset");
    fclose(f);
}
static int distance(unsigned a, unsigned b)
{
    int r = (int)(a&31)-(int)(b&31), g = (int)((a>>5)&31)-(int)((b>>5)&31), v = (int)(a>>10)-(int)(b>>10);
    return r*r*2+g*g*3+v*v;
}
int main(int argc, char **argv)
{
    png_image image = {0};
    unsigned char *rgba;
    unsigned x,y,i,j,count=0,maxColor,used=0;
    if (argc != 5) die("pack-background source.png tiles.8bpp palette.gbapal tilemap.bin");
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&image,argv[1])) die("Cannot read background PNG");
    image.format = PNG_FORMAT_RGBA;
    if (image.width*2 != image.height*3) die("Background must have exact 3:2 aspect ratio");
    rgba = malloc(PNG_IMAGE_SIZE(image));
    if (!rgba || !png_image_finish_read(&image,NULL,rgba,0,NULL)) die("Cannot decode background PNG");
    for (y=0;y<160;y++) for (x=0;x<240;x++)
    {
        unsigned sx=(x*2+1)*image.width/480, sy=(y*2+1)*image.height/320;
        const unsigned char *p=rgba+(sy*image.width+sx)*4;
        unsigned c, r, g, b;
        // GBA tiled backgrounds have no alpha. Flatten the source's slightly
        // translucent outer pixels against a dark forest matte during encoding.
        r=(p[0]*p[3]+20*(255-p[3]))/255;
        g=(p[1]*p[3]+43*(255-p[3]))/255;
        b=(p[2]*p[3]+42*(255-p[3]))/255;
        c=(r>>3)|((g>>3)<<5)|((b>>3)<<10);
        pixels[y*240+x]=c; histogram[c]++;
    }
    // Dominant colors followed by exact nearest-color mapping. Background owns
    // banks 1..15; bank 0 belongs to the independent native HUD.
    for (i=0;i<240;i++)
    {
        maxColor=0;
        for(j=0;j<32768;j++) if(histogram[j]>histogram[maxColor]) maxColor=j;
        if(!histogram[maxColor]) break;
        colors[count++]=maxColor; histogram[maxColor]=0;
    }
    for(i=0;i<count;i++) { palette[(i+16)*2]=colors[i]&255; palette[(i+16)*2+1]=colors[i]>>8; }
    for(i=0;i<32768;i++)
    {
        int best=0x7fffffff; unsigned closest=0;
        for(j=0;j<count;j++) {int d=distance(i,colors[j]); if(d<best){best=d;closest=j;}}
        lookup[i]=closest+16;
    }
    for(y=0;y<160;y++) for(x=0;x<240;x++)
        tiles[(y/8*30+x/8)*64+(y%8)*8+x%8]=lookup[pixels[y*240+x]];
    for(y=0;y<20;y++) for(x=0;x<30;x++)
    {
        unsigned tile=y*30+x, offset=(y*32+x)*2;
        map[offset]=tile&255; map[offset+1]=tile>>8;
        used++;
    }
    output(argv[2],tiles,sizeof(tiles));output(argv[3],palette,sizeof(palette));output(argv[4],map,sizeof(map));
    printf("GBA background: %u tiles, %u colors, 38400 bytes, independent HUD palette\n",used,count);
    free(rgba);png_image_free(&image);
    return 0;
}
