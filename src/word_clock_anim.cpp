#include "word_clock_anim.h"

#define ROWS     13
#define COLS     13
#define NUM_LEDS 169

extern CRGB leds[NUM_LEDS];

float         animationSpeed   = 1.0f;
AnimationType currentAnimation = ANIM_FADE;

static CRGB oldState[NUM_LEDS];
static CRGB newState[NUM_LEDS];

// ─── Helpers ─────────────────────────────────────────────────
static int idx(int row, int col) {
    int physRow = 12 - row;
    if (physRow % 2 == 0) return physRow * 13 + col;
    else                  return physRow * 13 + (12 - col);
}

static void showDelay(int ms) {
    FastLED.show();
    delay((int)(ms / animationSpeed));
}

void anim_snapshotOld() { memcpy(oldState, leds, sizeof(CRGB) * NUM_LEDS); }
void anim_snapshotNew() { memcpy(newState, leds, sizeof(CRGB) * NUM_LEDS); }

// ════════════════════════════════════════════════════════════
//  1. FADE
// ════════════════════════════════════════════════════════════
static void anim_fade() {
    int steps = 20;
    for (int s = steps; s >= 0; s--) {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i].r = oldState[i].r * s / steps;
            leds[i].g = oldState[i].g * s / steps;
            leds[i].b = oldState[i].b * s / steps;
        }
        showDelay(20);
    }
    for (int s = 0; s <= steps; s++) {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i].r = newState[i].r * s / steps;
            leds[i].g = newState[i].g * s / steps;
            leds[i].b = newState[i].b * s / steps;
        }
        showDelay(20);
    }
}

// ════════════════════════════════════════════════════════════
//  2. WIPE LEFT→RIGHT
// ════════════════════════════════════════════════════════════
static void anim_wipeLR() {
    for (int col = 0; col < COLS; col++) {
        for (int row = 0; row < ROWS; row++)
            leds[idx(row, col)] = CRGB::Black;
        showDelay(40);
    }
    for (int col = 0; col < COLS; col++) {
        for (int row = 0; row < ROWS; row++)
            leds[idx(row, col)] = newState[idx(row, col)];
        showDelay(40);
    }
}

// ════════════════════════════════════════════════════════════
//  3. WIPE RIGHT→LEFT
// ════════════════════════════════════════════════════════════
static void anim_wipeRL() {
    for (int col = COLS - 1; col >= 0; col--) {
        for (int row = 0; row < ROWS; row++)
            leds[idx(row, col)] = CRGB::Black;
        showDelay(40);
    }
    for (int col = COLS - 1; col >= 0; col--) {
        for (int row = 0; row < ROWS; row++)
            leds[idx(row, col)] = newState[idx(row, col)];
        showDelay(40);
    }
}

// ════════════════════════════════════════════════════════════
//  4. RAIN
// ════════════════════════════════════════════════════════════
static void anim_rain() {
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++)
            leds[idx(row, col)] = CRGB::Black;
        showDelay(50);
    }
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (newState[idx(row, col)] != CRGB(0, 0, 0))
                leds[idx(row, col)] = newState[idx(row, col)];
        }
        showDelay(50);
    }
}

// ════════════════════════════════════════════════════════════
//  5. GRAVITY FALL
// ════════════════════════════════════════════════════════════
static void anim_gravity() {
    for (int offset = 0; offset <= ROWS; offset++) {
        FastLED.clear();
        for (int row = 0; row < ROWS; row++) {
            int targetRow = row + offset;
            if (targetRow < ROWS) {
                for (int col = 0; col < COLS; col++)
                    leds[idx(targetRow, col)] = oldState[idx(row, col)];
            }
        }
        showDelay(40);
    }
    FastLED.clear();
    for (int step = ROWS - 1; step >= 0; step--) {
        FastLED.clear();
        for (int row = 0; row < ROWS; row++) {
            int srcRow = row - (ROWS - 1 - step);
            if (srcRow >= 0 && srcRow < ROWS) {
                for (int col = 0; col < COLS; col++)
                    leds[idx(row, col)] = newState[idx(srcRow, col)];
            }
        }
        showDelay(40);
    }
}

// ════════════════════════════════════════════════════════════
//  6. GLITCH
// ════════════════════════════════════════════════════════════
static void anim_glitch() {
    for (int g = 0; g < 12; g++) {
        for (int i = 0; i < NUM_LEDS; i++) {
            if (random8() < 80)
                leds[i] = CRGB(random8(), random8(), random8());
            else
                leds[i] = CRGB::Black;
        }
        showDelay(30);
    }
    FastLED.clear();
    showDelay(80);
    memcpy(leds, newState, sizeof(CRGB) * NUM_LEDS);
}

// ════════════════════════════════════════════════════════════
//  7. RIPPLE
// ════════════════════════════════════════════════════════════
static void anim_ripple() {
    int cx = 6, cy = 6;
    int maxDist = 12;
    for (int d = 0; d <= maxDist; d++) {
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                int dist = abs(row - cy) + abs(col - cx);
                if (dist <= d) leds[idx(row, col)] = CRGB::Black;
            }
        }
        showDelay(30);
    }
    for (int d = maxDist; d >= 0; d--) {
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                int dist = abs(row - cy) + abs(col - cx);
                if (dist >= d) leds[idx(row, col)] = newState[idx(row, col)];
            }
        }
        showDelay(30);
    }
}

// ════════════════════════════════════════════════════════════
//  8. CLOCK WIPE
// ════════════════════════════════════════════════════════════
static void anim_clockWipe() {
    int cx = 6, cy = 6;
    int steps = 36;
    for (int s = 0; s < steps; s++) {
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                float a = atan2(row - cy, col - cx);
                if (a < 0) a += 2 * PI;
                float sweepAngle = (s * 2 * PI / steps);
                if (a <= sweepAngle)
                    leds[idx(row, col)] = CRGB::Black;
            }
        }
        showDelay(25);
    }
    for (int s = 0; s < steps; s++) {
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                float a = atan2(row - cy, col - cx);
                if (a < 0) a += 2 * PI;
                float sweepAngle = (s * 2 * PI / steps);
                if (a <= sweepAngle)
                    leds[idx(row, col)] = newState[idx(row, col)];
            }
        }
        showDelay(25);
    }
}

// ════════════════════════════════════════════════════════════
//  9. SPARKLE
// ════════════════════════════════════════════════════════════
static void anim_sparkle() {
    bool cleared[NUM_LEDS] = {false};
    int remaining = NUM_LEDS;
    while (remaining > 0) {
        int n = random8(5, 15);
        for (int i = 0; i < n && remaining > 0; i++) {
            int pick = random16(NUM_LEDS);
            if (!cleared[pick]) {
                cleared[pick] = true;
                leds[pick] = CRGB::Black;
                remaining--;
            }
        }
        showDelay(20);
    }
    bool revealed[NUM_LEDS] = {false};
    remaining = NUM_LEDS;
    while (remaining > 0) {
        int n = random8(5, 15);
        for (int i = 0; i < n && remaining > 0; i++) {
            int pick = random16(NUM_LEDS);
            if (!revealed[pick]) {
                revealed[pick] = true;
                leds[pick] = newState[pick];
                remaining--;
            }
        }
        showDelay(20);
    }
}

// ════════════════════════════════════════════════════════════
//  10. HEARTBEAT
// ════════════════════════════════════════════════════════════
static void anim_heartbeat() {
    for (int s = 255; s >= 0; s -= 15) {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i].r = oldState[i].r * s / 255;
            leds[i].g = oldState[i].g * s / 255;
            leds[i].b = oldState[i].b * s / 255;
        }
        showDelay(15);
    }
    delay((int)(100 / animationSpeed));
    for (int s = 0; s <= 255; s += 15) {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i].r = newState[i].r * s / 255;
            leds[i].g = newState[i].g * s / 255;
            leds[i].b = newState[i].b * s / 255;
        }
        showDelay(15);
    }
}

// ════════════════════════════════════════════════════════════
//  11. TYPEWRITER
// ════════════════════════════════════════════════════════════
static void anim_typewriter() {
    FastLED.clear();
    showDelay(200);
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (newState[idx(row, col)] != CRGB(0, 0, 0)) {
                leds[idx(row, col)] = newState[idx(row, col)];
                showDelay(30);
            }
        }
    }
}

// ════════════════════════════════════════════════════════════
//  12. FIRE
// ════════════════════════════════════════════════════════════
static void anim_fire() {
    static byte heat[ROWS][COLS];
    for (int col = 0; col < COLS; col++) heat[ROWS-1][col] = 255;
    for (int frame = 0; frame < 40; frame++) {
        for (int row = 0; row < ROWS - 1; row++) {
            for (int col = 0; col < COLS; col++) {
                int avg = (heat[row+1][col] +
                           heat[row+1][(col + COLS - 1) % COLS] +
                           heat[row+1][(col + 1) % COLS]) / 3;
                heat[row][col] = (avg > 10) ? avg - random8(8) : 0;
            }
        }
        for (int col = 0; col < COLS; col++)
            heat[ROWS-1][col] = qadd8(heat[ROWS-1][col], random8(50, 100));
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                byte h = heat[row][col];
                CRGB color;
                if      (h > 200) color = CRGB(255, 255, h - 200);
                else if (h > 100) color = CRGB(255, (h - 100) * 2, 0);
                else              color = CRGB(h * 2, 0, 0);
                leds[idx(row, col)] = color;
            }
        }
        showDelay(30);
    }
    for (int s = 255; s >= 0; s -= 20) {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i].r = leds[i].r * s / 255;
            leds[i].g = leds[i].g * s / 255;
            leds[i].b = leds[i].b * s / 255;
        }
        showDelay(20);
    }
    memcpy(leds, newState, sizeof(CRGB) * NUM_LEDS);
}

// ════════════════════════════════════════════════════════════
//  13. STARFIELD
// ════════════════════════════════════════════════════════════
static void anim_starfield() {
    FastLED.clear();
    for (int frame = 0; frame < 30; frame++) {
        FastLED.clear();
        for (int s = 0; s < 20; s++) {
            int row = random8(ROWS);
            int col = random8(COLS);
            byte brightness = random8(100, 255);
            leds[idx(row, col)] = CRGB(brightness, brightness, brightness);
        }
        showDelay(30);
    }
    FastLED.clear();
    showDelay(50);
    memcpy(leds, newState, sizeof(CRGB) * NUM_LEDS);
    FastLED.show();
}

// ════════════════════════════════════════════════════════════
//  14. PIXEL SHUFFLE
// ════════════════════════════════════════════════════════════
static void anim_pixelShuffle() {
    CRGB working[NUM_LEDS];
    memcpy(working, oldState, sizeof(CRGB) * NUM_LEDS);
    for (int frame = 0; frame < 25; frame++) {
        for (int i = 0; i < 30; i++) {
            int a = random16(NUM_LEDS);
            int b = random16(NUM_LEDS);
            CRGB tmp  = working[a];
            working[a] = working[b];
            working[b] = tmp;
        }
        memcpy(leds, working, sizeof(CRGB) * NUM_LEDS);
        showDelay(40);
    }
    for (int s = 255; s >= 0; s -= 25) {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i].r = working[i].r * s / 255;
            leds[i].g = working[i].g * s / 255;
            leds[i].b = working[i].b * s / 255;
        }
        showDelay(20);
    }
    memcpy(leds, newState, sizeof(CRGB) * NUM_LEDS);
}

// ════════════════════════════════════════════════════════════
//  15. BOUNCE
// ════════════════════════════════════════════════════════════
static void anim_bounce() {
    // ── Phase 1: old content slides DOWN and off screen ──
    for (int offset = 0; offset <= ROWS; offset++) {
        FastLED.clear();
        for (int row = 0; row < ROWS; row++) {
            int targetRow = row + offset;
            if (targetRow < ROWS) {
                for (int col = 0; col < COLS; col++)
                    leds[idx(targetRow, col)] = oldState[idx(row, col)];
            }
        }
        showDelay(30);
    }

    // ── Phase 2: new content drops in from top with bounce ──
    // offset: positive = content above screen, negative = content below final pos
    int bounceOffsets[] = { 13, 11, 9, 7, 5, 3, 1, 0, -3, 1, -2, 1, 0 };
    int bounceDelays[]  = { 20, 20, 20, 20, 20, 20, 20, 30, 40, 40, 40, 40, 50 };
    int steps = 13;

    for (int k = 0; k < steps; k++) {
        int offset = bounceOffsets[k];
        FastLED.clear();
        for (int row = 0; row < ROWS; row++) {
            int srcRow = row - offset;
            if (srcRow >= 0 && srcRow < ROWS) {
                for (int col = 0; col < COLS; col++)
                    leds[idx(row, col)] = newState[idx(srcRow, col)];
            }
        }
        showDelay(bounceDelays[k]);
    }

    memcpy(leds, newState, sizeof(CRGB) * NUM_LEDS);
    FastLED.show();
}

// ════════════════════════════════════════════════════════════
//  DISPATCHER
// ════════════════════════════════════════════════════════════
void anim_play() {
    switch (currentAnimation) {
        case ANIM_FADE:          anim_fade();         break;
        case ANIM_WIPE_LR:       anim_wipeLR();       break;
        case ANIM_WIPE_RL:       anim_wipeRL();       break;
        case ANIM_RAIN:          anim_rain();         break;
        case ANIM_GRAVITY:       anim_gravity();      break;
        case ANIM_GLITCH:        anim_glitch();       break;
        case ANIM_RIPPLE:        anim_ripple();       break;
        case ANIM_CLOCK_WIPE:    anim_clockWipe();    break;
        case ANIM_SPARKLE:       anim_sparkle();      break;
        case ANIM_HEARTBEAT:     anim_heartbeat();    break;
        case ANIM_TYPEWRITER:    anim_typewriter();   break;
        case ANIM_FIRE:          anim_fire();         break;
        case ANIM_STARFIELD:     anim_starfield();    break;
        case ANIM_PIXEL_SHUFFLE: anim_pixelShuffle(); break;
        case ANIM_BOUNCE:        anim_bounce();       break;
        default:                 anim_fade();         break;
    }
    memcpy(leds, newState, sizeof(CRGB) * NUM_LEDS);
    FastLED.show();
}
