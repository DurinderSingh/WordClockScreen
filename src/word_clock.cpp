#include "word_clock.h"
#include "word_clock_anim.h"
static int _lastHour = 0;
static int _lastMinute = 0;

CRGB leds[NUM_LEDS];

static int ledIndex(int row, int col)
{
    int physicalRow = 12 - row; // flip: row 0 (top) → physical row 12 (last)
    if (physicalRow % 2 == 0)
        return physicalRow * 13 + col; // even: L→R
    else
        return physicalRow * 13 + (12 - col); // odd : R→L
}

static void lightWord(int row, int startCol, int endCol, CRGB color)
{
    for (int c = startCol; c <= endCol; c++)
        leds[ledIndex(row, c)] = color;
}

// ──────────────────────────────── Greeting state ──────────────────────────────────────────
static bool greetingActive = false;
static bool greetingShown = false;
static int greetingRow = -1;
static int greetingStartCol = 0;
static int greetingEndCol = 0;
static CRGB greetingColor;
static unsigned long greetingStart_ms = 0;
#define GREETING_DURATION_MS 5000

static int lastGreetingCategory = -1;

static int getGreetingCategory(int h)
{
    if (h >= 5 && h <= 11)
        return 0; // morning
    if (h >= 12 && h <= 15)
        return 1; // afternoon
    if (h >= 16 && h <= 20)
        return 2; // evening
    if (h == 21 || h == 22 || h == 23 || h == 0 || h == 1)
        return 3; // night
    return -1;
}

static void triggerGreeting(int category)
{
    greetingActive = true;
    greetingStart_ms = millis();
    switch (category)
    {
    case 0:
        greetingRow = 10;
        greetingStartCol = 0;
        greetingEndCol = 11;
        greetingColor = COLOR_MORNING;
        break;
    case 1:
        greetingRow = 11;
        greetingStartCol = 0;
        greetingEndCol = 8;
        greetingColor = COLOR_AFTERNOON;
        break;
    case 2:
        greetingRow = 12;
        greetingStartCol = 0;
        greetingEndCol = 6;
        greetingColor = COLOR_EVENING;
        break;
    case 3:
        greetingRow = 12;
        greetingStartCol = 8;
        greetingEndCol = 12;
        greetingColor = COLOR_NIGHT;
        break;
    }
}

// ───────────────────────────────────────── Time display logic ─────────────────────────────────────────

static void lightTime(int hour24, int minute)
{
    FastLED.clear();

    // IT IS — always
    lightWord(0, 0, 1, COLOR_TIME); // IT
    lightWord(0, 3, 4, COLOR_TIME); // IS

    // ────────────────────────────────────────── Determine phrase bucket ──────────────────────────
    bool almostFlag = false;
    int dispMinute = 0; // 0=oclock,5,10,15,20,25,30
    bool useTo = false;
    int hourOffset = 0;

    if (minute <= 2)
    {
        dispMinute = 0;
        useTo = false;
        hourOffset = 0;
    }
    else if (minute <= 4)
    {
        dispMinute = 5;
        useTo = false;
        hourOffset = 0;
        almostFlag = true;
    }
    else if (minute <= 7)
    {
        dispMinute = 5;
        useTo = false;
        hourOffset = 0;
    }
    else if (minute <= 9)
    {
        dispMinute = 10;
        useTo = false;
        hourOffset = 0;
        almostFlag = true;
    }
    else if (minute <= 12)
    {
        dispMinute = 10;
        useTo = false;
        hourOffset = 0;
    }
    else if (minute <= 14)
    {
        dispMinute = 15;
        useTo = false;
        hourOffset = 0;
        almostFlag = true;
    }
    else if (minute <= 17)
    {
        dispMinute = 15;
        useTo = false;
        hourOffset = 0;
    }
    else if (minute <= 19)
    {
        dispMinute = 20;
        useTo = false;
        hourOffset = 0;
        almostFlag = true;
    }
    else if (minute <= 22)
    {
        dispMinute = 20;
        useTo = false;
        hourOffset = 0;
    }
    else if (minute <= 24)
    {
        dispMinute = 25;
        useTo = false;
        hourOffset = 0;
        almostFlag = true;
    }
    else if (minute <= 27)
    {
        dispMinute = 25;
        useTo = false;
        hourOffset = 0;
    }
    else if (minute <= 29)
    {
        dispMinute = 30;
        useTo = false;
        hourOffset = 0;
        almostFlag = true;
    }
    else if (minute <= 32)
    {
        dispMinute = 30;
        useTo = false;
        hourOffset = 0;
    }
    else if (minute <= 34)
    {
        dispMinute = 25;
        useTo = true;
        hourOffset = 1;
        almostFlag = true;
    }
    else if (minute <= 37)
    {
        dispMinute = 25;
        useTo = true;
        hourOffset = 1;
    }
    else if (minute <= 39)
    {
        dispMinute = 20;
        useTo = true;
        hourOffset = 1;
        almostFlag = true;
    }
    else if (minute <= 42)
    {
        dispMinute = 20;
        useTo = true;
        hourOffset = 1;
    }
    else if (minute <= 44)
    {
        dispMinute = 15;
        useTo = true;
        hourOffset = 1;
        almostFlag = true;
    }
    else if (minute <= 47)
    {
        dispMinute = 15;
        useTo = true;
        hourOffset = 1;
    }
    else if (minute <= 49)
    {
        dispMinute = 10;
        useTo = true;
        hourOffset = 1;
        almostFlag = true;
    }
    else if (minute <= 52)
    {
        dispMinute = 10;
        useTo = true;
        hourOffset = 1;
    }
    else if (minute <= 54)
    {
        dispMinute = 5;
        useTo = true;
        hourOffset = 1;
        almostFlag = true;
    }
    else if (minute <= 57)
    {
        dispMinute = 5;
        useTo = true;
        hourOffset = 1;
    }
    else
    {
        dispMinute = 0;
        useTo = false;
        hourOffset = 1;
        almostFlag = true;
    }

    // ─────────────────────────────────────────────── ALMOST ──────────────────────────────────────────
    if (almostFlag)
        lightWord(0, 6, 11, COLOR_TIME); // ALMOST

    // ─────────────────────────────────────────────── Minute words ────────────────────────────────────
    if (dispMinute == 0)
    {
        // O'CLOCK
        lightWord(9, 2, 7, COLOR_TIME);
    }
    else if (dispMinute == 5)
    {
        lightWord(2, 6, 9, COLOR_TIME);  // FIVE
        lightWord(3, 5, 11, COLOR_TIME); // MINUTES
    }
    else if (dispMinute == 10)
    {
        lightWord(1, 10, 12, COLOR_TIME); // TEN
        lightWord(3, 5, 11, COLOR_TIME);  // MINUTES
    }
    else if (dispMinute == 15)
    {
        lightWord(1, 0, 0, COLOR_TIME); // A
        lightWord(1, 2, 8, COLOR_TIME); // QUARTER                // A QUARTER
    }
    else if (dispMinute == 20)
    {
        lightWord(2, 0, 5, COLOR_TIME);  // TWENTY
        lightWord(3, 5, 11, COLOR_TIME); // MINUTES
    }
    else if (dispMinute == 25)
    {
        lightWord(2, 0, 5, COLOR_TIME);  // TWENTY
        lightWord(2, 6, 9, COLOR_TIME);  // FIVE
        lightWord(3, 5, 11, COLOR_TIME); // MINUTES
    }
    else if (dispMinute == 30)
    {
        lightWord(3, 0, 3, COLOR_TIME); // HALF
    }

    // ─────────────────────────────────────────────── PAST or TO ──────────────────────────────────────
    if (dispMinute != 0)
    {
        if (!useTo)
            lightWord(4, 0, 3, COLOR_TIME); // PAST
        else
            lightWord(4, 6, 7, COLOR_TIME); // TO
    }

    // ─────────────────────────────────────────────── Hour word ───────────────────────────────────────
    int h = (hour24 + hourOffset) % 12;
    if (h == 0)
        h = 12;
    switch (h)
    {
    case 1:
        lightWord(4, 9, 11, COLOR_TIME);
        break;
    case 2:
        lightWord(5, 0, 2, COLOR_TIME);
        break;
    case 3:
        lightWord(5, 3, 7, COLOR_TIME);
        break;
    case 4:
        lightWord(5, 8, 11, COLOR_TIME);
        break;
    case 5:
        lightWord(6, 0, 3, COLOR_TIME);
        break;
    case 6:
        lightWord(6, 5, 7, COLOR_TIME);
        break;
    case 7:
        lightWord(6, 8, 12, COLOR_TIME);
        break;
    case 8:
        lightWord(7, 0, 4, COLOR_TIME);
        break;
    case 9:
        lightWord(7, 5, 8, COLOR_TIME);
        break;
    case 10:
        lightWord(7, 9, 11, COLOR_TIME);
        break;
    case 11:
        lightWord(8, 0, 5, COLOR_TIME);
        break;
    case 12:
        lightWord(8, 7, 12, COLOR_TIME);
        break;
    }

    // ─────────────────────────────────────────── AM / PM ─────────────────────────────────────────
    int displayHour = (hour24 + hourOffset) % 24;
    if (displayHour < 12)
        lightWord(9, 9, 10, COLOR_AM);
    else
        lightWord(9, 11, 12, COLOR_PM);

    FastLED.show();
}

// ─────────────────────────────────────────────── Public ──────────────────────────────────────────────────

void wordclock_init()
{
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS_MAX);
    FastLED.clear(true);
}

void wordclock_update(int hour24, int minute)
{
    _lastHour = hour24;
    _lastMinute = minute;

    // Snapshot old state BEFORE clearing
    anim_snapshotOld();

    int cat = getGreetingCategory(hour24);
    if (cat != lastGreetingCategory)
    {
        lastGreetingCategory = cat;
        greetingShown = false;
    }
    if (cat >= 0 && !greetingShown && minute == 0)
    {
        greetingShown = true;
        triggerGreeting(cat);
        FastLED.clear();
        lightWord(greetingRow, greetingStartCol, greetingEndCol, greetingColor);
        anim_snapshotNew();
        anim_play();
        return;
    }

    greetingActive = false;
    lightTime(hour24, minute); // this calls FastLED.show() — remove that call from lightTime()
    anim_snapshotNew();
    anim_play(); // this does the final FastLED.show()
}

void wordclock_forceUpdate()
{
    lightTime(_lastHour, _lastMinute);
    FastLED.show();
}

void wordclock_tick()
{
    if (!greetingActive)
        return;
    if (millis() - greetingStart_ms >= GREETING_DURATION_MS)
    {
        greetingActive = false;
        // Force immediate clock redraw after greeting ends
        extern void wordclock_forceUpdate();
        wordclock_forceUpdate();
    }
}
