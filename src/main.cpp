#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "ui/ui.h"
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "credentials.h"

// ========== WIFI CREDENTIALS ==========
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD; // ← CHANGE THIS


// ========== NTP SETTINGS ==========
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;  // IST = UTC+5:30 = 19800 seconds
const int daylightOffset_sec = 0;


// ============= Weather Settings =============
String api_key = WEATHER_API_KEY;
String weather_location = WEATHER_LOCATION;
int is_day = 1;



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


// ================================================ Fetch Weather =============================================


int weather_code = 0;
void fetch_weather() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not ready for weather");
        return;
    }
    
    HTTPClient http;
    // Ambala, Haryana
    String url = "http://api.weatherapi.com/v1/current.json?key=" + String(api_key) + "&q=Ambala,Haryana,India&aqi=no";
    
    Serial.println("Fetching weather...");
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            weather_temp = String((int)doc["current"]["temp_c"].as<float>());
            weather_humidity = String((int)doc["current"]["humidity"].as<float>());
            weather_code = doc["current"]["condition"]["code"];
            is_day = doc["current"]["is_day"];
            
            Serial.print("Ambala Weather: ");
            Serial.print(weather_temp);
            Serial.print("°C, ");
            Serial.print(weather_humidity);
            Serial.print("%, Code: ");
            Serial.print(weather_code);
            Serial.print(", Day: ");
            Serial.println(is_day);
        }
    }
    http.end();
}




// ---------------------------------------------- Weather Description ----------------------------------------------

// NOTE: This project is using WeatherAPI.com everywhere

const char* get_weather_description(int code) {
    if (code == 1000) return "Clear";
    if (code >= 1003 && code <= 1009) return "Cloudy";
    if (code >= 1063 && code <= 1246) return "Rainy";
    if (code == 1030 || code == 1135 || code == 1147) return "Foggy";
    if (code >= 1066 && code <= 1258) return "Snowy";
    if (code == 1087 || code >= 1273) return "Stormy";
    return "Clear";
}




// ================================================ Weather Animations ================================================


// ===============================================Show Weather Icons ==================================================


void show_weather_icon() {
    // Hide ALL icons first
    if (ui_SunIcon) lv_obj_add_flag(ui_SunIcon, LV_OBJ_FLAG_HIDDEN);
    if (ui_MoonIcon) lv_obj_add_flag(ui_MoonIcon, LV_OBJ_FLAG_HIDDEN);
    if (ui_CloudIcon) lv_obj_add_flag(ui_CloudIcon, LV_OBJ_FLAG_HIDDEN);
    if (ui_RainIcon) lv_obj_add_flag(ui_RainIcon, LV_OBJ_FLAG_HIDDEN);
    if (ui_FogIcon) lv_obj_add_flag(ui_FogIcon, LV_OBJ_FLAG_HIDDEN);
    if (ui_CloudyNightIcon) lv_obj_add_flag(ui_CloudyNightIcon, LV_OBJ_FLAG_HIDDEN);
    
    // Show correct icon based on weather_code and is_day
    if (weather_code == 1000) {  // Clear sky
        if (is_day == 1) {
            if (ui_SunIcon) {
                lv_obj_clear_flag(ui_SunIcon, LV_OBJ_FLAG_HIDDEN);
                RotatingSun_Animation(ui_SunIcon, 0);
            }
        } else {
            if (ui_MoonIcon) lv_obj_clear_flag(ui_MoonIcon, LV_OBJ_FLAG_HIDDEN);
        }
        
    } else if (weather_code >= 1003 && weather_code <= 1009) {  // Cloudy
        if (is_day == 1) {
            if (ui_CloudIcon) lv_obj_clear_flag(ui_CloudIcon, LV_OBJ_FLAG_HIDDEN);
        } else {
            if (ui_CloudyNightIcon) lv_obj_clear_flag(ui_CloudyNightIcon, LV_OBJ_FLAG_HIDDEN);
        }
        
    } else if (weather_code >= 1063 && weather_code <= 1246) {  // Rain/Storm
        if (ui_RainIcon) lv_obj_clear_flag(ui_RainIcon, LV_OBJ_FLAG_HIDDEN);
        
    } else if (weather_code == 1030 || weather_code == 1135 || weather_code == 1147) {  // Fog
        if (ui_FogIcon) lv_obj_clear_flag(ui_FogIcon, LV_OBJ_FLAG_HIDDEN);
    }
}


// ============================================ DYNAMIC BACKGROUNDS ============================================

void set_time_background() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    int hour = timeinfo.tm_hour;
    
    if (hour >= 5 && hour < 8) {
        lv_obj_set_style_bg_color(ui_Time, lv_color_hex(0xFFB347), 0);
    } else if (hour >= 8 && hour < 17) {
        lv_obj_set_style_bg_color(ui_Time, lv_color_hex(0x87CEEB), 0);
    } else if (hour >= 17 && hour < 20) {
        lv_obj_set_style_bg_color(ui_Time, lv_color_hex(0xFF6B35), 0);
    } else {
        lv_obj_set_style_bg_color(ui_Time, lv_color_hex(0x1A1A2E), 0);
    }
}
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void set_weather_background() {
    if (weather_code == 1000) {
        if (is_day == 1) {
            lv_obj_set_style_bg_color(ui_Outdoor_Weather, lv_color_hex(0x87CEEB), 0);
        } else {
            lv_obj_set_style_bg_color(ui_Outdoor_Weather, lv_color_hex(0x0F1C2E), 0);
        }
    } else if (weather_code >= 1003 && weather_code <= 1009) {
        lv_obj_set_style_bg_color(ui_Outdoor_Weather, lv_color_hex(0x95A5A6), 0);
    } else if (weather_code >= 1063 && weather_code <= 1246) {
        lv_obj_set_style_bg_color(ui_Outdoor_Weather, lv_color_hex(0x34495E), 0);
    } else if (weather_code == 1030 || weather_code == 1135 || weather_code == 1147) {
        lv_obj_set_style_bg_color(ui_Outdoor_Weather, lv_color_hex(0xBDC3C7), 0);
    }
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void set_weather_text_colors() {
    lv_color_t text_color;
    
    // Determine text color based on background
    if (weather_code == 1000 && is_day == 1) {
        // Sunny blue bg (0x87CEEB) = light, use BLACK text
        text_color = lv_color_hex(0x000000);
    } else if (weather_code >= 1003 && weather_code <= 1009 && is_day == 0) {
        // Cloudy night = medium grey, use WHITE text
        text_color = lv_color_hex(0xFFFFFF);
    } else if (weather_code >= 1003 && weather_code <= 1009 && is_day == 1) {
        // Cloudy day grey (0x95A5A6) = medium, use BLACK text
        text_color = lv_color_hex(0x000000);
    } else if (weather_code >= 1063 && weather_code <= 1246) {
        // Rain dark grey (0x34495E) = dark, use WHITE text
        text_color = lv_color_hex(0xFFFFFF);
    } else if (weather_code == 1030 || weather_code == 1135 || weather_code == 1147) {
        // Fog light grey (0xBDC3C7) = light, use BLACK text
        text_color = lv_color_hex(0x000000);
    } else if (weather_code == 1000 && is_day == 0) {
        // Clear night dark (0x0F1C2E) = dark, use WHITE text
        text_color = lv_color_hex(0xFFFFFF);
    } else {
        // Default = WHITE for safety
        text_color = lv_color_hex(0xFFFFFF);
    }
    
    // Apply to all text labels
    if (ui_WeatherDesc) lv_obj_set_style_text_color(ui_WeatherDesc, text_color, 0);
    if (ui_OutdoorTemp) lv_obj_set_style_text_color(ui_OutdoorTemp, text_color, 0);
    if (ui_Celcious) lv_obj_set_style_text_color(ui_Celcious, text_color, 0);
    if (ui_OutdoorHumidity) lv_obj_set_style_text_color(ui_OutdoorHumidity, text_color, 0);
}


// ==================================================== SCREEN SWITCHING ===================================================


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
        
        // Update weather labels
        if (ui_OutdoorTemp) {
            String temp_str = weather_temp;
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
        
        set_weather_background();
        set_weather_text_colors();
        
        lv_refr_now(NULL);
        delay(50);
        
        show_weather_icon();
        
    } else {
        lv_scr_load(ui_Time);
        current_screen = 0;
        
        set_time_background();
        
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
    static bool wifi_connected = false;
    
    // LVGL tick and handler - ONCE only
    lv_tick_inc(5);
    lv_timer_handler();
    
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
            delay(100);  // Reduced from 500ms
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
    
    delay(5);  // Single delay at end
}
