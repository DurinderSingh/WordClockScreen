#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "ui/ui.h"
#include <WiFi.h>
#include <time.h>

// ========== WIFI CREDENTIALS ==========
const char* ssid = "AirFiber-9WNmq1";        // ← CHANGE THIS
const char* password = "yevu5shae6HiT0Za"; // ← CHANGE THIS

// ========== NTP SETTINGS ==========
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;  // IST = UTC+5:30 = 19800 seconds
const int daylightOffset_sec = 0;

// --- LOVYAN GFX SETUP ---
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI      _bus_instance;
    lgfx::Light_PWM    _light_instance;

public:
    LGFX(void) {
        { 
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;     
            cfg.spi_mode = 0;             
            cfg.freq_write = 40000000;    
            cfg.pin_sclk = 4;             
            cfg.pin_mosi = 6;             
            cfg.pin_miso = -1;            
            cfg.pin_dc   = 10;            
            _bus_instance.config(cfg);    
            _panel_instance.setBus(&_bus_instance);
        }
        { 
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 7;     
            cfg.pin_rst          = 3;     
            cfg.panel_width      = 240;   
            cfg.panel_height     = 320;   
            cfg.invert           = true;  
            cfg.rgb_order        = false;  
            _panel_instance.config(cfg);
        }
        { 
            auto cfg = _light_instance.config();
            cfg.pin_bl = 1;              
            cfg.invert = true;           
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        setPanel(&_panel_instance);
    }
};

LGFX tft;
static lv_color_t buf[240 * 10];
static lv_display_t *disp;

static int current_screen = 0;

/* Display flushing */
void my_disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t *)color_p, w * h);
    tft.endWrite();
    lv_display_flush_ready(disp_drv);
}

// ----------------------------------------------------- CLOCK LOGIC --------------------------------------
unsigned long last_tick = 0;
bool colon_visible = true;

// ----------------------------------------------------- Update Clock --------------------------------------

void update_clock() {
    static int last_second = -1;  // Track last second to avoid duplicate updates
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    if (current_screen == 0) {
        // Update Hours
        char hours[3];
        sprintf(hours, "%02d", timeinfo.tm_hour);
        if (ui_LabelHour) {
            lv_label_set_text(ui_LabelHour, hours);
        }

        // Update Minutes
        char minutes[3];
        sprintf(minutes, "%02d", timeinfo.tm_min);
        if (ui_LabelMinutes) {
            lv_label_set_text(ui_LabelMinutes, minutes);
        }

        // Blink colon every second
        colon_visible = !colon_visible;
        if (ui_LabelSeconds) {
            if (colon_visible) {
                lv_obj_clear_flag(ui_LabelSeconds, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(ui_LabelSeconds, LV_OBJ_FLAG_HIDDEN);
            }
        }

        // Update Second Bar ONLY when second actually changes
        if (timeinfo.tm_sec != last_second) {
            last_second = timeinfo.tm_sec;
            
            if (ui_SecondBar) {
                int bar_value = timeinfo.tm_sec + 1;
                lv_bar_set_value(ui_SecondBar, bar_value, LV_ANIM_ON);
            }
        }
        
        // Debug
        Serial.print("Time: ");
        Serial.print(hours);
        Serial.print(colon_visible ? ":" : " ");
        Serial.print(minutes);
        Serial.print(":");
        Serial.print(timeinfo.tm_sec);
        Serial.print(" | Bar: ");
        Serial.println(timeinfo.tm_sec + 1);
        
        // Force screen refresh
        lv_refr_now(NULL);
    }
}


// ------------------------------------------------------- SCREEN SWITCHING -------------------------------------------
void switch_screen() {
    Serial.println(">>> SWITCHING SCREEN <<<");
    Serial.print("Current: ");
    Serial.println(current_screen);
    
    if (current_screen == 0) {
        lv_scr_load(ui_Day_Date_Month);
        current_screen = 1;
        
        // Update date labels with real time
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        char date[3], month[4], day[4];
        sprintf(date, "%02d", timeinfo.tm_mday);
        strftime(month, sizeof(month), "%b", &timeinfo);
        strftime(day, sizeof(day), "%a", &timeinfo);
        
        // Convert to uppercase
        for(int i = 0; month[i]; i++) month[i] = toupper(month[i]);
        for(int i = 0; day[i]; i++) day[i] = toupper(day[i]);
        
        if (ui_date) lv_label_set_text(ui_date, date);
        if (ui_Month) lv_label_set_text(ui_Month, month);
        if (ui_day) lv_label_set_text(ui_day, day);
        lv_refr_now(NULL);
        
    } else if (current_screen == 1) {
        lv_scr_load(ui_Indoor_Weather);
        current_screen = 2;
        lv_refr_now(NULL);
        
    } else {
    lv_scr_load(ui_Time);
    current_screen = 0;
    
    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Set bar to current second WITHOUT animation (instant)
    if (ui_SecondBar) {
        int bar_value = timeinfo.tm_sec + 1;
        lv_bar_set_value(ui_SecondBar, bar_value, LV_ANIM_OFF);
    }
    
    // Show colon immediately
    if (ui_LabelSeconds) {
        lv_obj_clear_flag(ui_LabelSeconds, LV_OBJ_FLAG_HIDDEN);
        colon_visible = true;
    }
    lv_refr_now(NULL);
    }
}

// ----------------------------------------------------- Void Setup ----------------------------------------------------------

// void setup()
// {
//     Serial.begin(115200);
//     delay(1000); // Wait for Serial to be ready
    
//     Serial.println("\n\n=================================");
//     Serial.println("=== ESP32-C3 WORD CLOCK STARTING ===");
//     Serial.println("=================================\n");
    
//     // ========== CONNECT TO WIFI FIRST ==========
//     Serial.println("========== WiFi Connection ==========");
//     Serial.print("SSID: ");
//     Serial.println(ssid);
//     Serial.print("Password length: ");
//     Serial.println(strlen(password));
    
//     WiFi.mode(WIFI_STA);
//     WiFi.begin(ssid, password);
    
//     Serial.print("Connecting to WiFi");
//     int wifi_attempts = 0;
//     while (WiFi.status() != WL_CONNECTED && wifi_attempts < 20) {
//         delay(500);
//         Serial.print(".");
//         wifi_attempts++;
        
//         // Detailed status every 10 attempts
//         if (wifi_attempts % 10 == 0) {
//             Serial.println();
//             Serial.print("Status: ");
//             switch(WiFi.status()) {
//                 case WL_IDLE_STATUS: Serial.print("IDLE"); break;
//                 case WL_NO_SSID_AVAIL: Serial.print("NO SSID - Check WiFi name!"); break;
//                 case WL_CONNECT_FAILED: Serial.print("FAILED - Check password!"); break;
//                 case WL_DISCONNECTED: Serial.print("DISCONNECTED"); break;
//                 default: Serial.print("UNKNOWN"); break;
//             }
//             Serial.print(" | Trying again");
//         }
//     }
//     Serial.println();
    
//     if (WiFi.status() == WL_CONNECTED) {
//         Serial.println("\n✓ WiFi CONNECTED!");
//         Serial.print("IP Address: ");
//         Serial.println(WiFi.localIP());
//         Serial.print("Signal Strength: ");
//         Serial.print(WiFi.RSSI());
//         Serial.println(" dBm");
        
//         // ========== GET TIME FROM NTP ==========
//         Serial.println("\n========== Time Sync ==========");
//         configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        
//         Serial.print("Syncing with NTP server");
//         struct tm timeinfo;
//         int ntp_attempts = 0;
//         while (!getLocalTime(&timeinfo) && ntp_attempts < 10) {
//             Serial.print(".");
//             delay(500);
//             ntp_attempts++;
//         }
//         Serial.println();
        
//         if (getLocalTime(&timeinfo)) {
//             Serial.println("✓ Time SYNCED!");
//             Serial.print("Current time: ");
//             Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S IST");
//         } else {
//             Serial.println("✗ Time sync FAILED - using default time");
//             struct timeval tv;
//             tv.tv_sec = 1739005800; // Feb 8, 2026 13:30 IST
//             settimeofday(&tv, NULL);
//         }
//     } else {
//         Serial.println("\n✗ WiFi CONNECTION FAILED!");
//         Serial.println("Using default time (no internet)");
        
//         struct timeval tv;
//         tv.tv_sec = 1739005800; // Feb 8, 2026 13:30 IST
//         settimeofday(&tv, NULL);
//     }
    
//     Serial.println("\n========== Display Init ==========");
//     tft.init();
//     tft.setRotation(1);
//     tft.setBrightness(255);
//     Serial.println("✓ Display OK");

//     lv_init();
//     Serial.println("✓ LVGL OK");

//     disp = lv_display_create(320, 240);
//     lv_display_set_flush_cb(disp, my_disp_flush);
//     lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
//     lv_display_set_user_data(disp, &tft);
//     Serial.println("✓ Display configured");

//     ui_init();
//     Serial.println("✓ UI initialized");

//     // Configure bar animation
//     if (ui_SecondBar) {
//         lv_obj_set_style_anim_duration(ui_SecondBar, 900, LV_PART_INDICATOR);
//     }
    
//     lv_scr_load(ui_Time);
//     lv_refr_now(NULL);  // ← ADD THIS LINE
//     Serial.println("✓ Clock screen loaded");
    
//     Serial.println("\n=================================");
//     Serial.println("=== SETUP COMPLETE ===");
//     Serial.println("=================================\n");
//     Serial.println("Screens switch every 7 seconds");
//     Serial.println("Clock updates every 1 second\n");
// }


void setup()
{
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n\n=================================");
    Serial.println("=== WORD CLOCK + LCD STARTING ===");
    Serial.println("=================================\n");
    
    // ========== 1. DISPLAY INIT (Instant) ==========
    Serial.println("========== Display Init ==========");
    tft.init();
    tft.setRotation(1);
    tft.setBrightness(255);
    Serial.println("✓ Display hardware OK");

    lv_init();
    Serial.println("✓ LVGL OK");

    disp = lv_display_create(320, 240);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, &tft);
    Serial.println("✓ Display configured");

    ui_init();
    Serial.println("✓ UI initialized");

    // Configure bar animation
    if (ui_SecondBar) {
        lv_obj_set_style_anim_duration(ui_SecondBar, 900, LV_PART_INDICATOR);
    }
    
    lv_scr_load(ui_Time);
    lv_refr_now(NULL);
    Serial.println("✓ LCD screen loaded");
    
    // ========== 2. WORD CLOCK LED INIT (Add when ready) ==========
    // FastLED.addLeds<WS2812B, 2, GRB>(leds, NUM_LEDS);
    // FastLED.setBrightness(128);
    // displayWordTime(0, 0);  // Show placeholder
    // Serial.println("✓ Word Clock LEDs Ready");
    
    // ========== 3. READ DS3231 RTC (Add when you have the module) ==========
    // Wire.begin(8, 9);  // SDA=GPIO8, SCL=GPIO9
    // if (rtc.begin()) {
    //     Serial.println("✓ DS3231 RTC found");
    //     DateTime now = rtc.now();
    //     
    //     // Set system time from DS3231
    //     struct timeval tv;
    //     tv.tv_sec = now.unixtime();
    //     settimeofday(&tv, NULL);
    //     
    //     Serial.print("Time from RTC: ");
    //     Serial.println(now.timestamp());
    // } else {
    //     Serial.println("✗ DS3231 not found - using default time");
    //     // Fallback time
    //     struct timeval tv;
    //     tv.tv_sec = 1739009400; // Feb 8, 2026 15:00 IST
    //     settimeofday(&tv, NULL);
    // }
    
    // FOR NOW: Just use NTP fallback time until you add DS3231
    struct timeval tv;
    tv.tv_sec = 1739009400; // Feb 8, 2026 15:00 IST
    settimeofday(&tv, NULL);
    Serial.println("✓ Using placeholder time (add DS3231 later)");
    
    // ========== 4. START WIFI (Non-blocking) ==========
    Serial.println("\n========== WiFi Connection ==========");
    Serial.print("SSID: ");
    Serial.println(ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("WiFi connection started (background)...");
    
    // ========== 5. DONE ==========
    Serial.println("\n=================================");
    Serial.println("=== SETUP COMPLETE ===");
    Serial.println("=================================");
    Serial.println("✓ LCD active");
    Serial.println("✓ WiFi connecting in background");
    Serial.println("✓ Screens switch every 7 seconds\n");
}


// ------------------------------------------------------- Void Loop ---------------------------------------------------

// void loop()
// {
//     static unsigned long last_screen_switch = 0;
    
//     // LVGL needs to run frequently
//     lv_timer_handler();
    
//     // Update Clock every 1 second
//     if (millis() - last_tick >= 1000) {
//         last_tick = millis();
//         update_clock();
//     }
    
//     // Switch screens every 7 seconds
//     if (millis() - last_screen_switch >= 7000) {
//         last_screen_switch = millis();
//         switch_screen();
//     }
    
//     delay(5);
// }


void loop()
{
    static unsigned long last_screen_switch = 0;
    static bool wifi_connected = false;
    
    lv_timer_handler();
    
    // ========== CHECK WIFI STATUS ==========
    if (!wifi_connected && WiFi.status() == WL_CONNECTED) {
        wifi_connected = true;
        Serial.println("\n✓✓✓ WiFi Connected! ✓✓✓");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        
        // Sync NTP
        Serial.println("Syncing NTP...");
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        
        struct tm timeinfo;
        int attempts = 0;
        while (!getLocalTime(&timeinfo) && attempts < 10) {
            delay(500);
            attempts++;
        }
        
        if (getLocalTime(&timeinfo)) {
            Serial.println("✓ NTP Synced!");
            Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S IST");
        }
    }
    // ========================================
    
    // Update Clock every 1 second
    if (millis() - last_tick >= 1000) {
        last_tick = millis();
        update_clock();
    }
    
    // Switch screens every 7 seconds
    if (millis() - last_screen_switch >= 7000) {
        last_screen_switch = millis();
        switch_screen();
    }
    
    delay(5);
}
