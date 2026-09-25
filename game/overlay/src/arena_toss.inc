// Seismic Toss presentation, included by realtime_arena.c like the capture code.
//
// The attacker grabs the target, leaps straight up until both leave the top
// of the screen, then dives back down head first with the target held
// beneath it and slams it into the ground, leaving a crater. When the throw
// will knock the target out, the leap cuts to the anime's view of the Earth
// from space: the pair circles the globe once, dives toward it, and the arena
// returns for the slam. Ground positions never move during the throw: only
// the drawn sprites leave the ground, the shadows stay where each body will
// land. Damage is the native LevelDamage script, resolved once at the grab
// (accuracy, Ghost immunity) and applied at the impact.

#define TOSS_GRAB 1
#define TOSS_LIFT 2
#define TOSS_AIR 3
#define TOSS_ORBIT 4
#define TOSS_EXIT 5
#define TOSS_DIVE 6
#define TOSS_LAND 7
#define TOSS_GRAB_FRAMES 10
#define TOSS_LIFT_FRAMES 30
#define TOSS_AIR_FRAMES 16
#define TOSS_ORBIT_FRAMES 104
#define TOSS_EXIT_FRAMES 18
#define TOSS_LAND_FRAMES 16
#define TOSS_SQUASH_FRAMES 4
#define TOSS_DIVE_SPEED 7
// Pixels above the ground at the top of the leap. Any arena position plus a
// 64 px sprite is completely above the screen at this height.
#define TOSS_PEAK 232
#define TOSS_HOLD_FORWARD 12
#define TOSS_HOLD_UP 6
// The attacker dives this far above the target it holds beneath itself.
#define TOSS_DIVE_GAP 14
// A sprite drawn upside down sits higher in its 64 px frame; shift it back.
#define TOSS_FLIP_SHIFT 10
#define TOSS_GLOBE_X 120
#define TOSS_GLOBE_Y 88
#define TOSS_ORBIT_RX 78
#define TOSS_ORBIT_RY 40
#define TOSS_SPACE_PALETTE 16

static const u32 sSpaceTiles[] = INCBIN_U32(".arena-dev/art/space.4bpp");
static const u16 sSpaceMap[] = INCBIN_U16(".arena-dev/art/space.bin");
static const u16 sSpacePalette[] = INCBIN_U16(".arena-dev/art/space.gbapal");
static const u16 sSpaceBackdrop = RGB_BLACK;

static void ApplyResolvedHit(u8 side,u16 move,s32 damage);
static u8 Facing(s32 dx, s32 dy);

static bool8 TossActive(void) { return sArena.toss.phase != 0; }

static bool8 TossAirborne(u8 side)
{
    return sArena.toss.phase && (sArena.toss.space || sArena.toss.height[side] > 4);
}

static const struct ArenaMoveProfile sTossImpactFx =
{
    MOVE_SEISMIC_TOSS, 0, ARENA_MOVE_MELEE, 3, 0, 6, 0, 0, 0, ARENA_VIS_SLAM, ARENA_MOVE_CREAM, 1
};

// Where each body is drawn this frame, derived only from the phase timer and
// the unchanged ground positions, so pausing or hitstop cannot drift anything.
static void TossLayout(void)
{
    struct ArenaToss *t = &sArena.toss;
    u8 a = t->attacker, v = a ^ 1;
    struct ArenaBody *attacker = &sArena.bodies[a], *victim = &sArena.bodies[v];
    s16 ax = attacker->x / Q, ay = attacker->y / Q, vx = victim->x / Q, vy = victim->y / Q;
    s16 hx = ax + attacker->attackX * TOSS_HOLD_FORWARD / Q, hy = ay - TOSS_HOLD_UP;
    u32 i;
    for (i = 0; i < 2; i++)
    {
        t->scaleX[i] = t->scaleY[i] = 256; t->rotation[i] = 0; t->priority[i] = 0;
        t->animation[i] = ARENA_ANIM_IDLE; t->hold[i] = FALSE;
    }
    t->direction[a] = attacker->shotFacing;
    t->direction[v] = FACE_DOWN;
    t->drawX[a] = ax; t->drawY[a] = ay - t->height[a];
    t->drawX[v] = vx; t->drawY[v] = vy - t->height[v];
    switch (t->phase)
    {
    case TOSS_GRAB:
        t->animation[a] = ARENA_ANIM_ATTACK; t->hold[a] = TRUE;
        t->direction[v] = victim->facing;
        t->drawX[v] = t->holdX + (hx - t->holdX) * t->timer / TOSS_GRAB_FRAMES;
        t->drawY[v] = t->holdY + (hy - t->holdY) * t->timer / TOSS_GRAB_FRAMES;
        break;
    case TOSS_LIFT:
    case TOSS_AIR:
        t->animation[a] = ARENA_ANIM_ATTACK; t->hold[a] = TRUE;
        t->drawX[v] = hx; t->drawY[v] = hy - t->height[v];
        break;
    case TOSS_ORBIT:
    {
        // Once around the globe, starting at the near bottom. The far half
        // passes behind the planet, smaller; the near half in front.
        u8 index = (64 + 256 * t->timer / TOSS_ORBIT_FRAMES) & 0xFF;
        s16 depth = Sin(index, 256), scale = 220 + depth * 36 / 256;
        s16 x = TOSS_GLOBE_X + Cos(index, TOSS_ORBIT_RX), y = TOSS_GLOBE_Y + Sin(index, TOSS_ORBIT_RY);
        t->drawX[a] = x; t->drawY[a] = y;
        t->drawX[v] = x; t->drawY[v] = y + 14 * scale / 256;
        t->scaleX[a] = t->scaleY[a] = t->scaleX[v] = t->scaleY[v] = scale;
        t->priority[a] = t->priority[v] = depth < 0 ? 3 : 0;
        t->animation[a] = ARENA_ANIM_WALK;
        t->direction[a] = Facing(-Sin(index, 256), Cos(index, 256));
        break;
    }
    case TOSS_EXIT:
    {
        // Dive toward the planet, shrinking into it, before the cut back.
        s16 p = t->timer * 256 / TOSS_EXIT_FRAMES, scale = 256 - 200 * p / 256;
        s16 y = TOSS_GLOBE_Y + TOSS_ORBIT_RY - (TOSS_ORBIT_RY - 6) * p / 256;
        t->drawX[a] = t->drawX[v] = TOSS_GLOBE_X;
        t->drawY[a] = y - TOSS_DIVE_GAP * scale / 256;
        t->drawY[v] = y + TOSS_FLIP_SHIFT * scale / 256;
        t->scaleX[a] = t->scaleY[a] = t->scaleX[v] = t->scaleY[v] = scale;
        t->rotation[v] = 0x8000;
        t->animation[a] = ARENA_ANIM_ATTACK; t->hold[a] = TRUE; t->direction[a] = FACE_DOWN;
        break;
    }
    case TOSS_DIVE:
        // Both descend above the target's own spot, the attacker head first.
        t->drawX[a] = vx; t->drawY[a] = vy - t->height[a];
        t->drawX[v] = vx; t->drawY[v] = vy - t->height[v] + TOSS_FLIP_SHIFT;
        t->rotation[v] = 0x8000;
        t->animation[a] = ARENA_ANIM_ATTACK; t->hold[a] = TRUE; t->direction[a] = FACE_DOWN;
        break;
    case TOSS_LAND:
    {
        // The target is down; the attacker hops from above it to its own spot.
        s16 p = t->timer * 256 / TOSS_LAND_FRAMES;
        t->drawX[a] = vx + (ax - vx) * p / 256;
        t->drawY[a] = vy + (ay - vy) * p / 256 - TOSS_DIVE_GAP * (256 - p) / 256 - Sin(p / 2, 6);
        t->direction[a] = Facing(vx - ax, vy - ay);
        if (t->timer < TOSS_SQUASH_FRAMES) { t->scaleX[v] = 300; t->scaleY[v] = 176; }
        break;
    }
    }
}

// Layer toggles reach the hardware on the current frame, palette and OAM
// updates one frame later. Each cut therefore blanks every sprite first,
// swaps layers and palettes over a black frame, and reveals the new scene
// once its palette and sprites have landed.
static void TossSpaceStep(u8 step)
{
    struct ArenaToss *t = &sArena.toss;
    switch (step)
    {
    case 0: t->space = TRUE; t->blackout = TRUE; LoadPalette(&sSpaceBackdrop, 0, sizeof(u16)); break;
    case 1: HideBg(1); HideBg(0); LoadPalette(sSpacePalette, TOSS_SPACE_PALETTE, sizeof(u16) * 16); break;
    case 2: ShowBg(2); break;
    case 3: t->blackout = FALSE; break;
    case 4: t->blackout = TRUE; break;
    case 5: HideBg(2); LoadPalette(&sForestPalette[TOSS_SPACE_PALETTE], TOSS_SPACE_PALETTE, sizeof(u16) * 16); break;
    case 6: t->blackout = FALSE; break;
    case 7: ShowBg(1); ShowBg(0); LoadPalette(sArenaPalette, 0, sizeof(u16)); t->space = FALSE; break;
    }
}

static void TossBegin(u8 side)
{
    struct ArenaToss *t = &sArena.toss;
    struct ArenaBody *body = &sArena.bodies[side], *target = &sArena.bodies[side ^ 1];
    s32 damage = RealtimeArena_ResolveDamage(side, side ^ 1, MOVE_SEISMIC_TOSS);
    if (damage <= 0)
    {
        // Native accuracy or type immunity failed: the swing simply whiffs.
        ApplyResolvedHit(side, MOVE_SEISMIC_TOSS, damage);
        return;
    }
    memset(t, 0, sizeof(*t));
    t->phase = TOSS_GRAB;
    t->attacker = side;
    t->damage = damage;
    t->fatal = damage >= gBattleMons[side ^ 1].hp;
    t->holdX = target->x / Q;
    t->holdY = target->y / Q;
    // Both bodies are committed to the throw: no pending shots, actions,
    // dashes or knockback survive the grab. The enemy AI re-plans afterwards.
    body->shotTimer = body->actionLife = 0;
    target->shotTimer = target->actionLife = 0;
    body->dash = target->dash = 0;
    body->moving = target->moving = FALSE;
    body->knockX = body->knockY = target->knockX = target->knockY = 0;
    target->facing = Facing(body->x - target->x, body->y - target->y);
    if (sCapture.state == ARENA_CAPTURE_AIM)
    {
        gArenaCaptureTelemetry.cancelled++;
        CaptureReset();
    }
    sArena.aiState = AI_RECOVER;
    sArena.attackBuffer = sArena.dashBuffer = 0;
    TossLayout();
    PlaySE(SE_M_VITAL_THROW2);
}

static void TossImpact(void)
{
    struct ArenaToss *t = &sArena.toss;
    struct ArenaBody *victim = &sArena.bodies[t->attacker ^ 1];
    s16 x = victim->x / Q, y = victim->y / Q;
    ApplyResolvedHit(t->attacker, MOVE_SEISMIC_TOSS, t->damage);
    // Slammed straight down, never pushed away from the crater.
    victim->knockX = victim->knockY = 0;
    ArenaTerrain_Crater(x, y + 8);
    ArenaFeedback_Dust(x - 7, y + 2, TRUE);
    ArenaFeedback_Dust(x + 7, y + 2, TRUE);
    ArenaFeedback_Wall(x, y - 2);
    sArena.hitstop = 6;
    t->shake = 14;
    PlaySE(SE_M_EARTHQUAKE);
    // A fainting target keeps the native result waiting until the attacker
    // has landed; the classic scripts then continue behind the arena image.
    if (sArena.resultTimer) sArena.resultTimer += TOSS_LAND_FRAMES;
}

static void TossEnd(void)
{
    struct ArenaToss *t = &sArena.toss;
    struct ArenaBody *attacker = &sArena.bodies[t->attacker], *victim = &sArena.bodies[t->attacker ^ 1];
    u32 i;
    if (t->space) { TossSpaceStep(5); TossSpaceStep(6); TossSpaceStep(7); }
    for (i = 0; i < 2; i++)
    {
        struct Sprite *sprite = &gSprites[sArena.bodies[i].sprite];
        sprite->oam.priority = 0;
        if (sArena.bodies[i].art) SetOamMatrix(sprite->oam.matrixNum, 0x100, 0, 0, 0x100);
    }
    t->phase = 0;
    t->height[0] = t->height[1] = 0;
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);
    // The attacker's recovery from Fire() is still pending; the victim needs a
    // moment to get up. Neither can act during the landing frames themselves.
    attacker->cooldown = max(attacker->cooldown, 20);
    victim->cooldown = max(victim->cooldown, 24);
    sArena.thinkTimer = max(1, sArena.reaction);
    sArena.hudDirty = TRUE;
}

static void TossTick(void)
{
    struct ArenaToss *t = &sArena.toss;
    u8 a = t->attacker, v = a ^ 1;
    struct ArenaBody *attacker = &sArena.bodies[a];
    s32 tm;
    if (!t->phase) return;
    if (t->shake) t->shake--;
    tm = ++t->timer;
    switch (t->phase)
    {
    case TOSS_GRAB:
        if (tm >= TOSS_GRAB_FRAMES) { t->phase = TOSS_LIFT; t->timer = 0; PlaySE(SE_M_FLY); }
        break;
    case TOSS_LIFT:
        // Accelerating leap: slow off the ground, fast past the top edge.
        t->height[a] = TOSS_PEAK * tm * tm / (TOSS_LIFT_FRAMES * TOSS_LIFT_FRAMES);
        t->height[v] = t->height[a] + TOSS_HOLD_UP;
        if (tm >= TOSS_LIFT_FRAMES)
        {
            t->timer = 0;
            if (t->fatal) { t->phase = TOSS_ORBIT; TossSpaceStep(0); PlaySE(SE_M_FLY); }
            else t->phase = TOSS_AIR;
        }
        break;
    case TOSS_AIR:
        if (tm >= TOSS_AIR_FRAMES) { t->phase = TOSS_DIVE; t->timer = 0; PlaySE(SE_FALL); }
        break;
    case TOSS_ORBIT:
        if (tm <= 3) TossSpaceStep(tm);
        if (tm == TOSS_ORBIT_FRAMES / 2) PlaySE(SE_M_FLY);
        if (tm >= TOSS_ORBIT_FRAMES) { t->phase = TOSS_EXIT; t->timer = 0; PlaySE(SE_M_SKY_UPPERCUT); }
        break;
    case TOSS_EXIT:
        if (tm == TOSS_EXIT_FRAMES - 1) TossSpaceStep(4);
        if (tm >= TOSS_EXIT_FRAMES)
        {
            TossSpaceStep(5);
            t->phase = TOSS_DIVE; t->timer = 0;
            t->height[v] = TOSS_PEAK; t->height[a] = TOSS_PEAK + TOSS_DIVE_GAP;
            PlaySE(SE_FALL);
        }
        break;
    case TOSS_DIVE:
        if (t->space && tm <= 2) TossSpaceStep(5 + tm);
        t->height[v] = max(0, TOSS_PEAK - TOSS_DIVE_SPEED * tm);
        t->height[a] = t->height[v] + TOSS_DIVE_GAP;
        if (!t->height[v])
        {
            t->phase = TOSS_LAND; t->timer = 0;
            TossImpact();
        }
        break;
    case TOSS_LAND:
        t->height[a] = 0;
        if (tm >= TOSS_LAND_FRAMES)
        {
            ArenaFeedback_Dust(attacker->x / Q, attacker->y / Q, TRUE);
            PlaySE(SE_LEDGE);
            TossEnd();
            return;
        }
        break;
    }
    TossLayout();
}

// Animation choice for the render loop. Returns TRUE when the attacker's
// attack animation should hold its impact pose instead of playing through.
static bool8 TossPose(u8 side, u8 *animation, u8 *direction)
{
    struct ArenaToss *t = &sArena.toss;
    *animation = t->animation[side];
    *direction = t->direction[side];
    return t->hold[side];
}

// Screen placement of one body during the throw. Returns TRUE when the
// sprite is completely above the screen.
static bool8 TossPresent(u8 side, struct Sprite *sprite)
{
    struct ArenaToss *t = &sArena.toss;
    s16 y = t->drawY[side];
    sprite->x = t->drawX[side];
    sprite->oam.priority = t->priority[side];
    // Hardware OAM wraps vertical positions; keep fully hidden sprites hidden
    // instead of letting them reappear at the bottom of the screen.
    if (y < -32) { sprite->y = -32; return TRUE; }
    sprite->y = y;
    return FALSE;
}

static void TossAffine(u8 side, struct Sprite *sprite)
{
    struct ArenaToss *t = &sArena.toss;
    if (!sArena.bodies[side].art) return;
    if (t->scaleX[side] != 256 || t->scaleY[side] != 256 || t->rotation[side])
        SetOamMatrixRotationScaling(sprite->oam.matrixNum, t->scaleX[side], t->scaleY[side], t->rotation[side]);
    else
        SetOamMatrix(sprite->oam.matrixNum, 0x100, 0, 0, 0x100);
}

// In space only the two Pokémon exist. Every other object in this frame's
// OAM (props, shadows, particles, markers, trainers) is disabled in place.
static void TossHideOam(void)
{
    u32 i, k;
    if (!sArena.toss.space) return;
    for (i = 0; i < 128; i++)
    {
        struct OamData *oam = &gMain.oamBuffer[i];
        bool8 body = FALSE;
        if (oam->affineMode == ST_OAM_AFFINE_ERASE) continue;
        for (k = 0; k < 2 && !sArena.toss.blackout; k++)
            if (oam->shape == SPRITE_SHAPE(64x64) && oam->size == SPRITE_SIZE(64x64)
                && oam->tileNum == gSprites[sArena.bodies[k].sprite].oam.tileNum) body = TRUE;
        if (!body) oam->affineMode = ST_OAM_AFFINE_ERASE;
    }
}

// Landing marker, impact burst, ground tremor and the space scene's HUD.
// Called once per frame after the bodies have been placed.
static void TossDraw(void)
{
    struct ArenaToss *t = &sArena.toss;
    struct ArenaBody *victim;
    struct Sprite *marker = &gSprites[sArena.aimSprite];
    if (!t->phase) return;
    victim = &sArena.bodies[t->attacker ^ 1];
    if (t->space)
    {
        // The paused move picker lives on the HUD layer; otherwise space is
        // uninterrupted black.
        if (sArena.paused) ShowBg(0); else HideBg(0);
    }
    if (t->phase == TOSS_LIFT || t->phase == TOSS_AIR || t->phase == TOSS_DIVE)
    {
        marker->x = victim->x / Q;
        marker->y = victim->y / Q;
        marker->invisible = sArena.paused || (sArena.frame & 4) != 0;
        marker->oam.paletteNum = IndexOfSpritePaletteTag(SHOT_TAG + 1);
    }
    if (t->phase == TOSS_LAND && t->timer < TOSS_SQUASH_FRAMES + 2)
        ArenaMoveFx_Action(t->attacker, &sTossImpactFx, victim->x / Q, victim->y / Q - 14,
                           FACE_DOWN, t->timer, TRUE, sArena.paused);
    SetGpuReg(REG_OFFSET_BG1VOFS, t->shake && !sArena.paused ? ((t->shake & 1) ? (t->shake / 4 + 1) : (0x200 - t->shake / 4 - 1)) : 0);
}
