#include "global.h"
#include "arena_render.h"

// Arena-only upload batch. Large PMD/attack frames use DMA instead of the
// native dialogue sprite path's BIOS CpuCopy16. Commit after OAM is complete;
// VBlank never consumes a half-built frame. No heap and no ROM-sized buffers.
#define COPIES 32
struct Copy {const void *src;void *dest;u16 size;};
static EWRAM_DATA struct Copy sCopies[COPIES]={};
static EWRAM_DATA volatile u8 sCount=0,sReady=0;
EWRAM_DATA u32 gArenaUploadTelemetry[3]={}; // bytes, peak bytes/VBlank, overflow
void ArenaRender_Reset(void)
{sCount=sReady=0;memset(gArenaUploadTelemetry,0,sizeof(gArenaUploadTelemetry));}
void ArenaRender_Copy(const void *src,void *dest,u16 size)
{
    u32 i;
    for(i=0;i<sCount;i++)if(sCopies[i].dest==dest)break;
    if(i==COPIES){gArenaUploadTelemetry[2]++;return;}
    sCopies[i].src=src;sCopies[i].dest=dest;sCopies[i].size=size;
    if(i==sCount)sCount++;
}
void ArenaRender_Ready(void){sReady=TRUE;}
void ArenaRender_Flush(void)
{
    u32 i,bytes=0;
    if(!sReady)return;
    for(i=0;i<sCount;i++)
    {
        DmaCopy16(3,sCopies[i].src,sCopies[i].dest,sCopies[i].size);
        bytes+=sCopies[i].size;
    }
    gArenaUploadTelemetry[0]+=bytes;
    if(bytes>gArenaUploadTelemetry[1])gArenaUploadTelemetry[1]=bytes;
    sReady=FALSE;sCount=0;
}
