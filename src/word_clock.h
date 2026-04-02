#pragma once
#include <FastLED.h>

// ─── Hardware ───────────────────────────────────────────────
#define LED_PIN        2
#define NUM_LEDS       169
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB
#define BRIGHTNESS_MAX 255

extern CRGB leds[NUM_LEDS];

// ─── Word Colors (change freely) ────────────────────────────
#define COLOR_TIME      CRGB(0x388940)   // warm white-gold
#define COLOR_MORNING   CRGB(0xFFDC32)   // yellow
#define COLOR_AFTERNOON CRGB(0xFF8C00)   // orange
#define COLOR_EVENING   CRGB(0x9650FF)   // purple
#define COLOR_NIGHT     CRGB(0x1E50FF)   // blue
#define COLOR_AM        CRGB(0xC8C8FF)
#define COLOR_PM        CRGB(0xFFB464)


// ─── Public API ─────────────────────────────────────────────
void wordclock_init();
void wordclock_update(int hour24, int minute);  // call every minute
void wordclock_tick();                          // call every loop()
void wordclock_forceUpdate();

