#include "arena_navigation.h"
#ifdef ARENA_NAV_HOST
#define EWRAM_DATA
#endif

// Rendering, movement, projectiles and AI all use these exact solid rectangles.
const struct ArenaRect gArenaObstacles[ARENA_OBSTACLES] =
{
    {108, 69, 133, 86},
    {60, 39, 81, 56},
    {169, 99, 190, 117},
    {24, 90, 42, 106},
    {28, 56, 45, 69},
    {201, 46, 213, 62},
    {202, 80, 218, 94}
};

#define NODES (ARENA_CORNERS + 2)
#define INF 0xFFFF
static EWRAM_DATA struct ArenaPoint sCorners[ARENA_CORNERS] = {};
static EWRAM_DATA u16 sEdges[ARENA_CORNERS][ARENA_CORNERS] = {};
static EWRAM_DATA u8 sBlockers[ARENA_CORNERS][ARENA_CORNERS] = {};
static EWRAM_DATA u8 sCornerBlockers[ARENA_CORNERS] = {};
static EWRAM_DATA u8 sCornerInBounds[ARENA_CORNERS] = {};
static EWRAM_DATA u8 sSolidMask = 0;

static s32 Abs(s32 n) { return n < 0 ? -n : n; }
static u8 IntersectsRect(s16 x1,s16 y1,s16 x2,s16 y2,u8 index,u8 radius)
{
    const struct ArenaRect *r=&gArenaObstacles[index];
    s32 l=r->left-radius, right=r->right+radius, t=r->top-radius, b=r->bottom+radius;
    s32 dx=x2-x1,dy=y2-y1,sx=x1+x2-l-right,sy=y1+y2-t-b;
    // Segment/AABB separating axes: exact, inclusive, no division or floats.
    // Degenerate segments (CanStand) take the same inexpensive path.
    if(Abs(sx)>right-l+Abs(dx)||Abs(sy)>b-t+Abs(dy))return 0;
    return Abs(dx*sy-dy*sx)<=Abs(dx)*(b-t)+Abs(dy)*(right-l);
}

static u8 Blockers(s16 x1,s16 y1,s16 x2,s16 y2,u8 radius)
{
    u8 i,mask=0;
    for(i=0;i<ARENA_OBSTACLES;i++)
        if(IntersectsRect(x1,y1,x2,y2,i,radius))mask|=1<<i;
    return mask;
}

s16 ArenaNav_FirstObstacle(s16 x1, s16 y1, s16 x2, s16 y2, u8 radius)
{
    // All products fit signed 32 bits in the 240x160 GBA screen.
    u16 i;
    s16 found=-1;
    s32 best=0x7FFFFFFF;
    for (i = 0; i < ARENA_OBSTACLES; i++)
    {
        s16 l = gArenaObstacles[i].left - radius;
        s16 r = gArenaObstacles[i].right + radius;
        s16 t = gArenaObstacles[i].top - radius;
        s16 b = gArenaObstacles[i].bottom + radius;
        s32 distance;
        if(!(sSolidMask&(1<<i)))continue;
        if (IntersectsRect(x1,y1,x2,y2,i,radius))
        {
            s32 dx=x1<l?l-x1:x1>r?x1-r:0;
            s32 dy=y1<t?t-y1:y1>b?y1-b:0;
            distance=dx*dx+dy*dy;
            if(distance<best){best=distance;found=i;}
        }
    }
    return found;
}

u8 ArenaNav_LineClear(s16 x1,s16 y1,s16 x2,s16 y2,u8 radius)
{
    u8 i;
    // Visibility queries do not need closest-hit sorting or all intersections.
    for(i=0;i<ARENA_OBSTACLES;i++)
        if((sSolidMask&(1<<i))&&IntersectsRect(x1,y1,x2,y2,i,radius))return 0;
    return 1;
}
u8 ArenaNav_IsSolid(u8 index){return index<ARENA_OBSTACLES&&!!(sSolidMask&(1<<index));}
void ArenaNav_SetObstacle(u8 index,u8 solid)
{
    if(index>=ARENA_OBSTACLES)return;
    if(solid)sSolidMask|=1<<index;else sSolidMask&=~(1<<index);
    // Every static edge already knows its blocker bitset. Breaking a prop is
    // O(1), never a synchronous visibility-graph rebuild during an explosion.
}

u8 ArenaNav_CanStand(s16 x, s16 y)
{
    return x >= ARENA_MIN_X && x <= ARENA_MAX_X && y >= ARENA_MIN_Y && y <= ARENA_MAX_Y
        && ArenaNav_LineClear(x, y, x, y, ARENA_BODY_RADIUS);
}

u16 ArenaNav_Distance(s16 x1, s16 y1, s16 x2, s16 y2)
{
    s32 dx = Abs(x1 - x2), dy = Abs(y1 - y2);
    return dx > dy ? dx + dy / 2 : dy + dx / 2;
}

void ArenaNav_Init(void)
{
    u16 i, j;
    sSolidMask=(1<<ARENA_OBSTACLES)-1;
    for (i = 0; i < ARENA_CORNERS; i++)
    {
        const struct ArenaRect *r = &gArenaObstacles[i / 4];
        sCorners[i].x = (i & 1) ? r->right + ARENA_BODY_RADIUS + 2 : r->left - ARENA_BODY_RADIUS - 2;
        sCorners[i].y = (i & 2) ? r->bottom + ARENA_BODY_RADIUS + 2 : r->top - ARENA_BODY_RADIUS - 2;
        sCornerInBounds[i]=sCorners[i].x>=ARENA_MIN_X&&sCorners[i].x<=ARENA_MAX_X
            &&sCorners[i].y>=ARENA_MIN_Y&&sCorners[i].y<=ARENA_MAX_Y;
        sCornerBlockers[i]=Blockers(sCorners[i].x,sCorners[i].y,sCorners[i].x,sCorners[i].y,ARENA_BODY_RADIUS);
    }
    for (i = 0; i < ARENA_CORNERS; i++)
        for (j = i; j < ARENA_CORNERS; j++)
        {
            sEdges[j][i]=sEdges[i][j]=ArenaNav_Distance(sCorners[i].x,sCorners[i].y,sCorners[j].x,sCorners[j].y);
            sBlockers[j][i]=sBlockers[i][j]=Blockers(sCorners[i].x,sCorners[i].y,sCorners[j].x,sCorners[j].y,ARENA_BODY_RADIUS);
        }
}

struct ArenaPoint ArenaNav_Corner(u8 index) { return sCorners[index % ARENA_CORNERS]; }

u8 ArenaNav_NextWaypoint(s16 x, s16 y, s16 goalX, s16 goalY, struct ArenaPoint *next)
{
    struct ArenaPoint points[NODES];
    u16 dist[NODES], prev[NODES], visited[NODES], heuristic[NODES];
    u16 i, j, node, cost, best, edge;
    next->x = x; next->y = y;
    if (!ArenaNav_CanStand(x, y) || !ArenaNav_CanStand(goalX, goalY)) return 0;
    if (ArenaNav_LineClear(x,y,goalX,goalY,ARENA_BODY_RADIUS))
    {
        next->x = goalX; next->y = goalY; return 1;
    }
    for (i = 0; i < NODES; i++)
    {
        dist[i] = INF; visited[i] = 0; prev[i] = INF;
        if (i < ARENA_CORNERS) points[i] = sCorners[i];
        // Edge rocks can have expanded corners outside the walkable screen.
        // Exclude them also from dynamic start/goal edges, not just cached edges.
        if (i < ARENA_CORNERS && (!sCornerInBounds[i]||(sCornerBlockers[i]&sSolidMask))) visited[i] = 1;
    }
    points[ARENA_CORNERS].x = x; points[ARENA_CORNERS].y = y;
    points[ARENA_CORNERS+1].x = goalX; points[ARENA_CORNERS+1].y = goalY;
    for(i=0;i<NODES;i++)heuristic[i]=ArenaNav_Distance(points[i].x,points[i].y,goalX,goalY);
    dist[ARENA_CORNERS] = 0;
    // Bounded A*: thirty nodes, cached static edges, no heap. Each start/goal edge is
    // evaluated at most once because settled nodes are never visited again.
    for (i = 0; i < NODES; i++)
    {
        best = INF; node = INF;
        for (j = 0; j < NODES; j++)
            if (!visited[j] && dist[j] != INF && dist[j]+heuristic[j] < best)
            { best = dist[j]+heuristic[j]; node = j; }
        if (node == INF) return 0;
        if (node == NODES-1) break;
        visited[node] = 1;
        for (j = 0; j < NODES; j++)
        {
            if (visited[j]) continue;
            if (node < ARENA_CORNERS && j < ARENA_CORNERS)
                edge = (sBlockers[node][j]&sSolidMask)?INF:sEdges[node][j];
            else edge = ArenaNav_LineClear(points[node].x, points[node].y, points[j].x, points[j].y, ARENA_BODY_RADIUS)
                ? ArenaNav_Distance(points[node].x, points[node].y, points[j].x, points[j].y) : INF;
            if (edge == INF) continue;
            cost = dist[node] + edge;
            if (cost < dist[j]) { dist[j] = cost; prev[j] = node; }
        }
    }
    node = NODES-1;
    for (i = 0; i < NODES && prev[node] != ARENA_CORNERS; i++)
    {
        if (prev[node] == INF) return 0;
        node = prev[node];
    }
    if (i == NODES) return 0;
    *next = points[node];
    return 1;
}
