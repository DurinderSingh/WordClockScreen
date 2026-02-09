#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "ui/ui.h"
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ========== WIFI CREDENTIALS ==========
const char* ssid = "AirFiber-9WNmq1";        // ← CHANGE THIS
const char* password = "yevu5shae6HiT0Za"; // ← CHANGE THIS

// ========== NTP SETTINGS ==========
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;  // IST = UTC+5:30 = 19800 seconds
const int daylightOffset_sec = 0;

// ============= Weather Settings =============
String api_key = "12373532f7244e6095c50534260502";
String latitude = "30.3390";  // Najafgarh coords
String longitude = "76.5208";
// Weather animation
// Weather animation frames
static int current_anim_frame = 0;
static int total_anim_frames = 0;  // You'll set this based on weather

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

// ================================================= WEATHER API =============================================
#include <HTTPClient.h>
#include <ArduinoJson.h>

String weather_temp = "--";
String weather_humidity = "--";
String weather_description = "Loading...";
int weather_code = 0;

void fetch_weather() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not ready for weather");
        return;
    }
    
    HTTPClient http;
    String url = "http://api.open-meteo.com/v1/forecast?latitude=28.6139&longitude=76.9794&current=temperature_2m,relative_humidity_2m,weather_code&timezone=Asia/Kolkata";
    
    Serial.println("Fetching weather...");
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            weather_temp = String((int)doc["current"]["temperature_2m"].as<float>());
            weather_humidity = String((int)doc["current"]["relative_humidity_2m"].as<float>());
            weather_code = doc["current"]["weather_code"];
            
            Serial.print("Weather: ");
            Serial.print(weather_temp);
            Serial.print("°C, ");
            Serial.print(weather_humidity);
            Serial.print("%, Code: ");
            Serial.println(weather_code);
        }
    }
    http.end();
}

// ---------------------------------------------- Weather Description ----------------------------------------------

const char* get_weather_description(int code) {
    if (code == 0) return "Clear Sky";
    if (code <= 3) return "Partly Cloudy";
    if (code <= 48) return "Foggy";
    if (code <= 67) return "Rainy";
    if (code <= 77) return "Snowy";
    if (code <= 82) return "Rain Showers";
    if (code <= 86) return "Snow Showers";
    if (code >= 95) return "Thunderstorm";
    return "Unknown";
}


// ====================================================== WEATHER ANIMATIONS ===================================

void update_weather_animation() {
    if (current_screen != 3) return;
    if (!ui_WeatherAnimIcon) return;
    
    // Only animate on clear/sunny weather
    if (weather_code <= 1) {  // ← CHANGE BACK FROM if (true)
        current_anim_frame++;
        if (current_anim_frame >= 10) {
            current_anim_frame = 0;
        }
        
        switch(current_anim_frame) {
            case 0: lv_img_set_src(ui_WeatherAnimIcon, &ui_img_1641329436); break;
            case 1: lv_img_set_src(ui_WeatherAnimIcon, &ui_img_10717917); break;
            case 2: lv_img_set_src(ui_WeatherAnimIcon, &ui_img_761082266); break;
            case 3: lv_img_set_src(ui_WeatherAnimIcon, &ui_img_1663658587); break;
            case 4: lv_img_set_src(ui_WeatherAnimIcon, &ui_img_965326360); break;
            case 5: lv_img_set_src(ui_WeatherAnimIcon, &ui_img_665285159); break;
            case 6: lv_img_set_src(ui_WeatherAnimIcon, &ui_img_85079190); break;
            case 7: lv_img_set_src(ui_WeatherAnimIcon, &ui_img_987655511); break;
            case 8: lv_img_set_src(ui_WeatherAnimIcon, &ui_img_1533475108); break;
            case 9: lv_img_set_src(ui_WeatherAnimIcon, &ui_img_97136411); break;
        }
        
        lv_obj_invalidate(ui_WeatherAnimIcon);
        lv_refr_now(NULL);
    }
}




// #define MAX_RAIN_DROPS 30
// #define MAX_SNOW_FLAKES 20

// struct RainDrop {
//     int16_t x, y;
//     int16_t speed;
// };

// struct SnowFlake {
//     int16_t x, y;
//     int16_t speed;
//     int16_t drift;
// };

// RainDrop rain_drops[MAX_RAIN_DROPS];
// SnowFlake snow_flakes[MAX_SNOW_FLAKES];

// void init_rain() {
//     for (int i = 0; i < MAX_RAIN_DROPS; i++) {
//         rain_drops[i].x = random(0, 320);
//         rain_drops[i].y = random(-240, 0);
//         rain_drops[i].speed = random(8, 15);
//     }
// }

// void init_snow() {
//     for (int i = 0; i < MAX_SNOW_FLAKES; i++) {
//         snow_flakes[i].x = random(0, 320);
//         snow_flakes[i].y = random(-240, 0);
//         snow_flakes[i].speed = random(2, 5);
//         snow_flakes[i].drift = random(-2, 3);
//     }
// }

// void update_rain() {
//     for (int i = 0; i < MAX_RAIN_DROPS; i++) {
//         rain_drops[i].y += rain_drops[i].speed;
        
//         // Reset if off screen
//         if (rain_drops[i].y > 240) {
//             rain_drops[i].y = -10;
//             rain_drops[i].x = random(0, 320);
//         }
//     }
// }

// void update_snow() {
//     for (int i = 0; i < MAX_SNOW_FLAKES; i++) {
//         snow_flakes[i].y += snow_flakes[i].speed;
//         snow_flakes[i].x += snow_flakes[i].drift;
        
//         // Wrap horizontally
//         if (snow_flakes[i].x < 0) snow_flakes[i].x = 320;
//         if (snow_flakes[i].x > 320) snow_flakes[i].x = 0;
        
//         // Reset if off screen
//         if (snow_flakes[i].y > 240) {
//             snow_flakes[i].y = -10;
//             snow_flakes[i].x = random(0, 320);
//         }
//     }
// }

// void draw_weather_animation() {
//     if (current_screen != 3) return;  // Only on outdoor weather screen
    
//     if (weather_code >= 51 && weather_code <= 67) {
//         // RAIN
//         for (int i = 0; i < MAX_RAIN_DROPS; i++) {
//             tft.drawFastVLine(rain_drops[i].x, rain_drops[i].y, 8, 0x5CFF);  // Light blue
//         }
//         update_rain();
        
//     } else if (weather_code >= 71 && weather_code <= 77) {
//         // SNOW
//         for (int i = 0; i < MAX_SNOW_FLAKES; i++) {
//             tft.fillCircle(snow_flakes[i].x, snow_flakes[i].y, 2, 0xFFFF);  // White
//         }
//         update_snow();
        
//     } else if (weather_code >= 95) {
//         // THUNDERSTORM (rain + occasional flash)
//         for (int i = 0; i < MAX_RAIN_DROPS; i++) {
//             tft.drawFastVLine(rain_drops[i].x, rain_drops[i].y, 10, 0xFFE0);  // Yellow tint
//         }
//         update_rain();
        
//         // Random lightning flash
//         if (random(0, 100) > 97) {
//             tft.fillScreen(0xFFFF);
//             delay(50);
//         }
//     }
// }


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

    } else if (current_screen == 2) {
    lv_scr_load(ui_Outdoor_Weather);
    current_screen = 3;
    
    // Update weather labels (your existing code)
    if (ui_OutdoorTemp) {
        String temp_str = weather_temp + "°C";
        lv_label_set_text(ui_OutdoorTemp, temp_str.c_str());
    }
    if (ui_OutdoorHumidity) {
        String hum_str = weather_humidity + "%";
        lv_label_set_text(ui_OutdoorHumidity, hum_str.c_str());
    }
    if (ui_WeatherDesc) {
        const char* desc = get_weather_description(weather_code);
        lv_label_set_text(ui_WeatherDesc, desc);
    }
    
    // ← INIT ANIMATIONS BASED ON WEATHER
    // if (weather_code >= 51 && weather_code <= 82) {
    //     init_rain();
    // } else if (weather_code >= 71 && weather_code <= 86) {
    //     init_snow();
    // } else if (weather_code >= 95) {
    //     init_rain();  // Thunderstorm uses rain
    // }
    
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
    

    // ======================================= 2. WORD CLOCK LED INIT (Add when ready) =========================================


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

// ----------------------------------------------------- Void Loop ----------------------------------------------------

void loop()
{
    static unsigned long last_screen_switch = 0;
    static unsigned long last_weather_fetch = 0;
     static unsigned long last_anim_update = 0;
    static bool wifi_connected = false;
    
    lv_timer_handler();
    
    // ========== ANIMATE WEATHER ICON (10 FPS) ==========
    if (current_screen == 3 && millis() - last_anim_update >= 100) {
        last_anim_update = millis();
        update_weather_animation();
    }

    // Check WiFi connection
    if (!wifi_connected && WiFi.status() == WL_CONNECTED) {
        wifi_connected = true;
        Serial.println("\n✓✓✓ WiFi Connected! ✓✓✓");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        
        // Sync NTP
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        struct tm timeinfo;
        int attempts = 0;
        while (!getLocalTime(&timeinfo) && attempts < 10) {
            delay(500);
            attempts++;
        }
        if (getLocalTime(&timeinfo)) {
            Serial.println("✓ NTP Synced!");
        }
        
        // Fetch weather immediately
        fetch_weather();
    }
    
    // Update weather every 10 minutes
    if (wifi_connected && millis() - last_weather_fetch >= 600000) {
        last_weather_fetch = millis();
        fetch_weather();
    }
    
    // Update clock every second
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