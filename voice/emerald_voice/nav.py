"""Paths around the arena's rocks, the same way the game's own AI finds them
(src/arena_navigation.c): the rocks are rectangles grown by the body radius,
their corners are the graph's nodes, and A* runs over the corners that see
each other. Rocks that were smashed (solid_mask) no longer block."""
import heapq
import math

from .state import BOUNDS, OBSTACLES

RADIUS = 10


def _segment_hits(x1, y1, x2, y2, rect, radius):
    left, top, right, bottom = rect[0] - radius, rect[1] - radius, rect[2] + radius, rect[3] + radius
    dx, dy = x2 - x1, y2 - y1
    sx, sy = x1 + x2 - left - right, y1 + y2 - top - bottom
    if abs(sx) > right - left + abs(dx) or abs(sy) > bottom - top + abs(dy):
        return False
    return abs(dx * sy - dy * sx) <= abs(dx) * (bottom - top) + abs(dy) * (right - left)


def _solid(mask, extra=()):
    return [r for i, r in enumerate(OBSTACLES) if mask & (1 << i)] + list(extra)


def body_rect(x, y):
    """The other Pokémon as an obstacle, for walks that aren't toward it."""
    return (x - 10, y - 10, x + 10, y + 10)


def line_clear(x1, y1, x2, y2, mask, radius=RADIUS, extra=()):
    return not any(_segment_hits(x1, y1, x2, y2, r, radius) for r in _solid(mask, extra))


def can_stand(x, y, mask, extra=()):
    return (BOUNDS[0] <= x <= BOUNDS[2] and BOUNDS[1] <= y <= BOUNDS[3]
            and line_clear(x, y, x, y, mask, extra=extra))


def toward(x, y, dx, dy, mask, distance=64, extra=()):
    """Where a walk of about `distance` px that way should end: past a rock
    in the way (the far side), or short of a wall, never inside anything."""
    k = 0.7071 if dx and dy else 1.0
    for d in list(range(distance, distance + 64, 6)) + list(range(distance - 8, 8, -8)):
        px, py = round(x + dx * d * k), round(y + dy * d * k)
        if can_stand(px, py, mask, extra):
            return px, py
    return nearest_standable(x + dx * distance * k, y + dy * distance * k, mask, extra)


def nearest_standable(x, y, mask, extra=()):
    """The closest point one can stand on, searching outward in rings."""
    x = min(max(x, BOUNDS[0]), BOUNDS[2])
    y = min(max(y, BOUNDS[1]), BOUNDS[3])
    if can_stand(x, y, mask, extra):
        return x, y
    for r in range(4, 80, 4):
        best = None
        for k in range(16):
            a = k * math.pi / 8
            px, py = round(x + r * math.cos(a)), round(y + r * math.sin(a))
            if can_stand(px, py, mask, extra):
                d = math.hypot(px - x, py - y)
                if best is None or d < best[0]:
                    best = (d, px, py)
        if best:
            return best[1], best[2]
    return x, y


def _corners(mask, extra=()):
    pts = []
    for r in _solid(mask, extra):
        for cx in (r[0] - RADIUS - 2, r[2] + RADIUS + 2):
            for cy in (r[1] - RADIUS - 2, r[3] + RADIUS + 2):
                if can_stand(cx, cy, mask, extra):
                    pts.append((cx, cy))
    return pts


def path(start, goal, mask, extra=()):
    """Waypoints from start to goal (goal included), avoiding rocks (and any
    `extra` rectangles). When the goal can't be stood on, heads for the
    nearest point that can."""
    goal = nearest_standable(*goal, mask, extra)
    if not can_stand(*start, mask, extra):
        return [goal]           # already overlapping an edge: just push out
    if line_clear(*start, *goal, mask, extra=extra):
        return [goal]
    nodes = [start, goal] + _corners(mask, extra)
    dist = {0: 0.0}
    prev = {}
    heap = [(math.dist(start, goal), 0)]
    seen = set()
    while heap:
        _, i = heapq.heappop(heap)
        if i in seen:
            continue
        seen.add(i)
        if i == 1:
            break
        for j in range(len(nodes)):
            if j in seen or not line_clear(*nodes[i], *nodes[j], mask, extra=extra):
                continue
            d = dist[i] + math.dist(nodes[i], nodes[j])
            if d < dist.get(j, 1e9):
                dist[j], prev[j] = d, i
                heapq.heappush(heap, (d + math.dist(nodes[j], goal), j))
    if 1 not in prev:
        return [goal]
    out, i = [], 1
    while i != 0:
        out.append(nodes[i])
        i = prev[i]
    return out[::-1]


def open_spot_away(me, rival, mask, want=120):
    """A standable spot far from the rival, preferring ones close to where
    the Pokémon already is (running away, not across the map)."""
    extra = [body_rect(*rival)]
    best = None
    for gx in range(BOUNDS[0] + 8, BOUNDS[2] - 7, 12):
        for gy in range(BOUNDS[1] + 6, BOUNDS[3] - 5, 12):
            if not can_stand(gx, gy, mask, extra):
                continue
            away = math.dist((gx, gy), rival)
            score = min(away, want) * 2 - math.dist((gx, gy), me) * 0.6
            if best is None or score > best[0]:
                best = (score, gx, gy)
    return (best[1], best[2]) if best else me
