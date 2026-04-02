#pragma once
#include <FastLED.h>

enum AnimationType {
    ANIM_FADE         = 0,
    ANIM_WIPE_LR      = 1,
    ANIM_WIPE_RL      = 2,
    ANIM_RAIN         = 3,
    ANIM_GRAVITY      = 4,
    ANIM_GLITCH       = 5,
    ANIM_RIPPLE       = 6,
    ANIM_CLOCK_WIPE   = 7,
    ANIM_SPARKLE      = 8,
    ANIM_HEARTBEAT    = 9,
    ANIM_TYPEWRITER   = 10,
    ANIM_FIRE         = 11,
    ANIM_STARFIELD    = 12,
    ANIM_PIXEL_SHUFFLE= 13, 
    ANIM_BOUNCE       = 14,
    ANIM_COUNT        = 15
};

// Set from Blynk later
extern float         animationSpeed;
extern AnimationType currentAnimation;

void anim_snapshotOld();   // call BEFORE wordclock_update()
void anim_snapshotNew();   // call AFTER wordclock_update() computes new state
void anim_play();          // plays transition then sets leds[] to new state
