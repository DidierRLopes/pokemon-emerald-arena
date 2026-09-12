// Asset format compiler: PNG RGBA pixels -> GBA 4bpp tiles, without scaling,
// dithering, painted frames or loss of opaque pixels. libpng simplified API.
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Sheet { png_image image; unsigned char *rgba; int fw, fh, frames; const char *output; };
static uint16_t palette[16];
static int colors = 1;
static void fail(const char *s) { fprintf(stderr, "%s\n",s); exit(1); }
static unsigned color_index(const unsigned char *p)
{
    uint16_t color;
    int i;
    if (p[3] == 0) return 0;
    if (p[3] != 255) fail("Partial alpha: importer refuses to silently alter original pixels");
    color = (p[0] >> 3) | ((p[1] >> 3) << 5) | ((p[2] >> 3) << 10);
    for (i = 1; i < colors; ++i) if (palette[i] == color) return i;
    if (colors == 16) fail("More than 15 opaque colors: choose a deliberate palette conversion");
    palette[colors] = color;
    return colors++;
}
static void write_bytes(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path,"wb");
    if (!f || fwrite(data,1,size,f) != size) fail("Cannot write compiled sprite asset");
    fclose(f);
}
int main(int argc, char **argv)
{
    struct Sheet sheets[8] = {0};
    unsigned char pal_bytes[32];
    int count, i, dir, frame, x, y;
    if (argc < 7 || (argc-2)%5 || (argc-2)/5 > 8) fail("palette [output png width height frames]...");
    count = (argc-2)/5;
    for (i = 0; i < count; ++i)
    {
        struct Sheet *s = &sheets[i];
        s->output = argv[2+i*5];
        s->fw = atoi(argv[4+i*5]); s->fh = atoi(argv[5+i*5]); s->frames = atoi(argv[6+i*5]);
        if (s->fw < 1 || s->fh < 1 || s->frames < 1 || s->frames > 128) fail("Invalid frame geometry");
        s->image.version = PNG_IMAGE_VERSION;
        if (!png_image_begin_read_from_file(&s->image,argv[3+i*5])) fail("Cannot read source PNG");
        s->image.format = PNG_FORMAT_RGBA;
        if (s->image.width != (unsigned)(s->fw*s->frames) || s->image.height != (unsigned)(s->fh*8)) fail("Expected exactly 8 directions and XML frame count");
        s->rgba = malloc(PNG_IMAGE_SIZE(s->image));
        if (!s->rgba || !png_image_finish_read(&s->image,NULL,s->rgba,0,NULL)) fail("Cannot decode source PNG");
        for (size_t n = 0; n < PNG_IMAGE_SIZE(s->image); n += 4) color_index(s->rgba+n);
    }
    for (i = 0; i < count; ++i)
    {
        struct Sheet *s = &sheets[i];
        size_t size = (size_t)s->frames * 8 * 2048;
        unsigned char *tiles = calloc(1,size);
        if (!tiles) fail("Allocation failed");
        for (dir = 0; dir < 8; ++dir)
            for (frame = 0; frame < s->frames; ++frame)
                for (y = 0; y < s->fh; ++y)
                    for (x = 0; x < s->fw; ++x)
                    {
                        const unsigned char *pixel = s->rgba + ((dir*s->fh+y)*s->image.width + frame*s->fw+x)*4;
                        unsigned index = color_index(pixel);
                        int tx = x + 32 - s->fw/2, ty = y + 32 - s->fh/2;
                        size_t offset;
                        if (!index) continue;
                        if (tx < 0 || tx >= 64 || ty < 0 || ty >= 64) fail("Opaque pixels exceed 64x64: refusing to crop art");
                        offset = (dir*s->frames+frame)*2048 + (ty/8*8+tx/8)*32 + ty%8*4 + tx%8/2;
                        tiles[offset] |= index << ((tx&1)*4);
                    }
        write_bytes(s->output,tiles,size);
        free(tiles); free(s->rgba); png_image_free(&s->image);
    }
    for (i = 0; i < 16; ++i) { pal_bytes[i*2] = palette[i]&255; pal_bytes[i*2+1] = palette[i]>>8; }
    write_bytes(argv[1],pal_bytes,32);
    printf("Packed %d sheets, 8 directions, %d opaque colors, no clipped pixels\n",count,colors-1);
    return 0;
}
