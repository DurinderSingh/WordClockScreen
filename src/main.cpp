#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "ui/ui.h"
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "credentials.h"
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_SHT31.h>
#include "word_clock.h"
#include "word_clock_anim.h"

// ========== WIFI CREDENTIALS ==========
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;


// ========== NTP SETTINGS ==========
const char* ntpServer         = "pool.ntp.org";
const long  gmtOffset_sec     = 19800;
const int   daylightOffset_sec = 0;


RTC_DS3231     rtc;
Adafruit_SHT31 sht30 = Adafruit_SHT31(&Wire);
bool rtc_ok = false;
bool sht_ok = false;
unsigned long lastClockUpdate = 0;


// ============= Weather Settings =============
String api_key          = WEATHER_API_KEY;
String weather_location = WEATHER_LOCATION;
int   is_day    = 1;
int   aqi_india = 0;
float aqi_pm25  = 0.0;


// ============= LDR Settings =============
#define LDR_PIN             0
#define MIN_LCD_BRIGHTNESS  10
#define MAX_LCD_BRIGHTNESS  255
#define MIN_LED_BRIGHTNESS  5
#define MAX_LED_BRIGHTNESS  180
#define LDR_SAMPLES         10


// ================= DISPLAY =================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI      _bus_instance;
    lgfx::Light_PWM    _light_instance;

public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host   = SPI2_HOST;
            cfg.spi_mode   = 0;
            cfg.freq_write = 20000000;
            cfg.pin_sclk   = 4;
            cfg.pin_mosi   = 6;
            cfg.pin_miso   = -1;
            cfg.pin_dc     = 10;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs       = 7;
            cfg.pin_rst      = 3;
            cfg.panel_width  = 240;
            cfg.panel_height = 320;
            cfg.invert       = true;
            cfg.rgb_order    = false;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 1;
            cfg.invert = false;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        setPanel(&_panel_instance);
    }
};

LGFX tft;


// ================= LVGL BUFFER =================
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;
static lv_color_t     buf[screenWidth * 40];
static lv_display_t  *disp;
static int            current_screen = 0;


void my_disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t *)color_p, w * h);
    tft.endWrite();
    lv_display_flush_ready(disp_drv);
}


// ========================================================= CLOCK ================================================
unsigned long last_tick = 0;
static char hours_buf[3]   = {0};
static char minutes_buf[3] = {0};


void update_clock() {
    if (current_screen != 0) return;
    int h, m;
    if (rtc_ok) {
        DateTime rtcNow = rtc.now();
        h = rtcNow.hour();
        m = rtcNow.minute();
    } else {
        time_t t;
        struct tm timeinfo;
        time(&t);
        localtime_r(&t, &timeinfo);
        h = timeinfo.tm_hour;
        m = timeinfo.tm_min;
    }
    snprintf(hours_buf,   sizeof(hours_buf),   "%02d", h);
    snprintf(minutes_buf, sizeof(minutes_buf), "%02d", m);
    if (ui_LabelHour)    lv_label_set_text(ui_LabelHour,    hours_buf);
    if (ui_LabelMinutes) lv_label_set_text(ui_LabelMinutes, minutes_buf);
}


// ========== WEATHER ==========
static char weather_temp[8]     = "--";
static char weather_humidity[8] = "--";
int         weather_code        = 0;


int calculate_india_aqi(float pm25) {
    if (pm25 <= 30)  return (pm25 * 50) / 30;
    else if (pm25 <= 60)  return 50  + ((pm25 - 30)  * 50)  / 30;
    else if (pm25 <= 90)  return 100 + ((pm25 - 60)  * 100) / 30;
    else if (pm25 <= 120) return 200 + ((pm25 - 90)  * 100) / 30;
    else if (pm25 <= 250) return 300 + ((pm25 - 120) * 100) / 130;
    else                  return 400 + ((pm25 - 250) * 100) / 130;
}


void fetch_weather() {
    if (WiFi.status() != WL_CONNECTED) { Serial.println("✗ WiFi not ready"); return; }
    HTTPClient http;
    char url[200];
    snprintf(url, sizeof(url),
             "http://api.weatherapi.com/v1/current.json?key=%s&q=Ambala,Haryana,India&aqi=yes",
             api_key.c_str());
    Serial.println("→ HTTP GET starting...");
    http.setTimeout(5000);
    http.begin(url);
    unsigned long start = millis();
    int httpCode = http.GET();
    Serial.printf("← HTTP response: %d (%lums)\n", httpCode, millis() - start);
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        static JsonDocument doc;
        doc.clear();
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            float temp     = doc["current"]["temp_c"].as<float>();
            float humidity = doc["current"]["humidity"].as<float>();
            snprintf(weather_temp,     sizeof(weather_temp),     "%.1f", temp);
            snprintf(weather_humidity, sizeof(weather_humidity), "%d",   (int)humidity);
            weather_code = doc["current"]["condition"]["code"];
            is_day       = doc["current"]["is_day"];
            Serial.printf("Weather: %s°C, %s%%, Code: %d\n", weather_temp, weather_humidity, weather_code);
        } else {
            Serial.printf("✗ JSON error: %s\n", error.c_str());
        }
        if (doc["current"]["air_quality"]["pm2_5"]) {
            aqi_pm25  = doc["current"]["air_quality"]["pm2_5"];
            aqi_india = calculate_india_aqi(aqi_pm25);
            Serial.printf("AQI: %d\n", aqi_india);
        }
        Serial.println("✓ Weather parsed");
    } else {
        Serial.printf("✗ HTTP failed: %d\n", httpCode);
    }
    http.end();
}


const char* get_weather_description(int code) {
    if (code == 1000)                                    return "Clear";
    if (code >= 1003 && code <= 1009)                    return "Cloudy";
    if (code >= 1063 && code <= 1246)                    return "Rainy";
    if (code == 1030 || code == 1135 || code == 1147)    return "Foggy";
    if (code >= 1066 && code <= 1258)                    return "Snowy";
    if (code == 1087 || code >= 1273)                    return "Stormy";
    return "Clear";
}


void show_weather_icon() {
    if (ui_SunIcon)         lv_obj_add_flag(ui_SunIcon,         LV_OBJ_FLAG_HIDDEN);
    if (ui_MoonIcon)        lv_obj_add_flag(ui_MoonIcon,        LV_OBJ_FLAG_HIDDEN);
    if (ui_CloudIcon)       lv_obj_add_flag(ui_CloudIcon,       LV_OBJ_FLAG_HIDDEN);
    if (ui_RainIcon)        lv_obj_add_flag(ui_RainIcon,        LV_OBJ_FLAG_HIDDEN);
    if (ui_FogIcon)         lv_obj_add_flag(ui_FogIcon,         LV_OBJ_FLAG_HIDDEN);
    if (ui_CloudyNightIcon) lv_obj_add_flag(ui_CloudyNightIcon, LV_OBJ_FLAG_HIDDEN);
    if (ui_SunIcon)         lv_anim_del(ui_SunIcon, NULL);

    if (weather_code == 1000) {
        if (is_day == 1) {
            if (ui_SunIcon) { lv_obj_clear_flag(ui_SunIcon, LV_OBJ_FLAG_HIDDEN); RotatingSun_Animation(ui_SunIcon, 0); }
        } else {
            if (ui_MoonIcon) lv_obj_clear_flag(ui_MoonIcon, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (weather_code >= 1003 && weather_code <= 1009) {
        if (is_day == 1) {
            if (ui_CloudIcon) lv_obj_clear_flag(ui_CloudIcon, LV_OBJ_FLAG_HIDDEN);
        } else {
            if (ui_CloudyNightIcon) lv_obj_clear_flag(ui_CloudyNightIcon, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (weather_code >= 1063 && weather_code <= 1246) {
        if (ui_RainIcon) lv_obj_clear_flag(ui_RainIcon, LV_OBJ_FLAG_HIDDEN);
    } else if (weather_code == 1030 || weather_code == 1135 || weather_code == 1147) {
        if (ui_FogIcon) lv_obj_clear_flag(ui_FogIcon, LV_OBJ_FLAG_HIDDEN);
    }
}


void set_weather_background() {
    if (weather_code == 1000) {
        lv_obj_set_style_bg_color(ui_Outdoor_Weather,
            is_day ? lv_color_hex(0x87CEEB) : lv_color_hex(0x0F1C2E), 0);
    } else if (weather_code >= 1003 && weather_code <= 1009) {
        lv_obj_set_style_bg_color(ui_Outdoor_Weather, lv_color_hex(0x95A5A6), 0);
    } else if (weather_code >= 1063 && weather_code <= 1246) {
        lv_obj_set_style_bg_color(ui_Outdoor_Weather, lv_color_hex(0x34495E), 0);
    } else if (weather_code == 1030 || weather_code == 1135 || weather_code == 1147) {
        lv_obj_set_style_bg_color(ui_Outdoor_Weather, lv_color_hex(0xBDC3C7), 0);
    }
}


void set_weather_text_colors() {
    lv_color_t text_color;
    if      (weather_code == 1000 && is_day == 1)                                  text_color = lv_color_hex(0x000000);
    else if (weather_code == 1000 && is_day == 0)                                  text_color = lv_color_hex(0xFFFFFF);
    else if (weather_code >= 1003 && weather_code <= 1009 && is_day == 1)          text_color = lv_color_hex(0x000000);
    else if (weather_code >= 1003 && weather_code <= 1009 && is_day == 0)          text_color = lv_color_hex(0xFFFFFF);
    else if (weather_code >= 1063 && weather_code <= 1246)                         text_color = lv_color_hex(0xFFFFFF);
    else if (weather_code == 1030 || weather_code == 1135 || weather_code == 1147) text_color = lv_color_hex(0x000000);
    else                                                                            text_color = lv_color_hex(0xFFFFFF);

    if (ui_WeatherDesc)     lv_obj_set_style_text_color(ui_WeatherDesc,     text_color, 0);
    if (ui_OutdoorTemp)     lv_obj_set_style_text_color(ui_OutdoorTemp,     text_color, 0);
    if (ui_Celcious)        lv_obj_set_style_text_color(ui_Celcious,        text_color, 0);
}


// =================================================== Indoor Screen ===============================================
void update_indoor_screen() {
    if (!sht_ok) return;
    float temp     = sht30.readTemperature();
    float humidity = sht30.readHumidity();
    if (isnan(temp) || isnan(humidity)) { Serial.println("✗ SHT30 read failed"); return; }
    Serial.printf("SHT30: %.1f°C, %.1f%%\n", temp, humidity);

    static char indoor_temp_buf[8];
    snprintf(indoor_temp_buf, sizeof(indoor_temp_buf), "%d", (int)temp);
    if (ui_temp) lv_label_set_text_static(ui_temp, indoor_temp_buf);

    static char indoor_hum_buf[8];
    snprintf(indoor_hum_buf, sizeof(indoor_hum_buf), "%d", (int)humidity);
    if (ui_Humidity) lv_label_set_text_static(ui_Humidity, indoor_hum_buf);

    const char* temp_status;
    lv_color_t  temp_color;
    if      (temp < 16) { temp_status = "Too Cold";      temp_color = lv_color_hex(0xB9E8FF); }
    else if (temp < 18) { temp_status = "Cool";          temp_color = lv_color_hex(0x3399FF); }
    else if (temp < 21) { temp_status = "Slightly Cool"; temp_color = lv_color_hex(0x00CCCC); }
    else if (temp < 24) { temp_status = "Comfortable";   temp_color = lv_color_hex(0x00CC66); }
    else if (temp < 26) { temp_status = "Warm";          temp_color = lv_color_hex(0x99CC00); }
    else if (temp < 28) { temp_status = "Getting Hot";   temp_color = lv_color_hex(0xFFCC00); }
    else if (temp < 30) { temp_status = "Hot";           temp_color = lv_color_hex(0xFF9900); }
    else                { temp_status = "Too Hot";        temp_color = lv_color_hex(0xFF3300); }

    if (ui_Temp_Status) {
        lv_label_set_text(ui_Temp_Status, temp_status);
        lv_obj_set_style_text_color(ui_Temp_Status, temp_color, 0);
    }

    const char* hum_status;
    lv_color_t  hum_color;
    if      (humidity < 30) { hum_status = "Too Dry";   hum_color = lv_color_hex(0xFF9966); }
    else if (humidity < 40) { hum_status = "Low";       hum_color = lv_color_hex(0xFFCC66); }
    else if (humidity < 60) { hum_status = "Optimal";   hum_color = lv_color_hex(0x00CC66); }
    else if (humidity < 70) { hum_status = "Humid";     hum_color = lv_color_hex(0x66CCFF); }
    else                    { hum_status = "Too Humid"; hum_color = lv_color_hex(0xB9E8FF); }

    if (ui_Humidity_Status) {
        lv_label_set_text(ui_Humidity_Status, hum_status);
        lv_obj_set_style_text_color(ui_Humidity_Status, hum_color, 0);
    }
}


// ====================================================== SCREEN SWITCHING ======================================
static char date_buf[4]     = {0};
static char month_buf[8]    = {0};
static char day_buf[8]      = {0};
static char temp_buf[8]     = {0};
static char humidity_buf[8] = {0};
static char aqi_buf[8]      = {0};
static char desc_buf[16]    = {0};


void switch_screen() {
    if (current_screen == 0) {
        if (ui_Day_Date_Month == NULL) { Serial.println("! ui_Day_Date_Month NULL"); ui_init(); }
        lv_anim_del_all();
        lv_scr_load(ui_Day_Date_Month);
        current_screen = 1;

        time_t t;
        struct tm timeinfo;
        time(&t);
        localtime_r(&t, &timeinfo);

        snprintf(date_buf,  sizeof(date_buf),  "%02d", timeinfo.tm_mday);
        strftime(month_buf, sizeof(month_buf), "%b",   &timeinfo);
        strftime(day_buf,   sizeof(day_buf),   "%a",   &timeinfo);

        for (int i = 0; i < (int)sizeof(month_buf) && month_buf[i]; i++) month_buf[i] = toupper(month_buf[i]);
        for (int i = 0; i < (int)sizeof(day_buf)   && day_buf[i];   i++) day_buf[i]   = toupper(day_buf[i]);

        if (ui_date)  lv_label_set_text_static(ui_date,  date_buf);
        if (ui_Month) lv_label_set_text_static(ui_Month, month_buf);
        if (ui_day)   lv_label_set_text_static(ui_day,   day_buf);
        Serial.println("✓ Loaded Date Screen");

    } else if (current_screen == 1) {
        if (ui_Indoor_Weather == NULL) { Serial.println("! ui_Indoor_Weather NULL"); ui_init(); }
        lv_anim_del(lv_scr_act(), NULL);
        lv_scr_load(ui_Indoor_Weather);
        current_screen = 2;
        update_indoor_screen();
        Serial.println("✓ Loaded Indoor Screen");

    } else if (current_screen == 2) {
        if (ui_Outdoor_Weather == NULL) { Serial.println("! ui_Outdoor_Weather NULL"); ui_init(); }
        lv_anim_del_all();
        lv_scr_load(ui_Outdoor_Weather);
        current_screen = 3;

        snprintf(temp_buf,     sizeof(temp_buf),     "%s", weather_temp);
        snprintf(humidity_buf, sizeof(humidity_buf), "%s", weather_humidity);
        snprintf(desc_buf,     sizeof(desc_buf),     "%s", get_weather_description(weather_code));

        if (ui_OutdoorTemp) lv_label_set_text_static(ui_OutdoorTemp, temp_buf);
        if (ui_WeatherDesc) lv_label_set_text_static(ui_WeatherDesc, desc_buf);

        set_weather_background();
        set_weather_text_colors();
        show_weather_icon();
        Serial.println("✓ Loaded Outdoor Screen");

    } else if (current_screen == 3) {
        if (ui_AQIHumidity == NULL) { Serial.println("! ui_AQIHumidity NULL"); ui_init(); }
        lv_anim_del_all();
        lv_scr_load(ui_AQIHumidity);
        current_screen = 4;

        snprintf(humidity_buf, sizeof(humidity_buf), "%s", weather_humidity);
        snprintf(aqi_buf,      sizeof(aqi_buf),      "%d", aqi_india);

        if (ui_OutdoorHumidity) lv_label_set_text_static(ui_OutdoorHumidity, humidity_buf);
        if (ui_AQI)             lv_label_set_text_static(ui_AQI,             aqi_buf);
        Serial.println("✓ Loaded AQI Screen");

    } else {
        if (ui_Time == NULL) { Serial.println("! ui_Time NULL"); ui_init(); }
        lv_anim_del_all();
        lv_scr_load(ui_Time);
        current_screen = 0;
        Serial.println("✓ Loaded Time Screen");
    }
}


// =================================================== Brightness Control =======================================
int read_brightness() {
    static int samples[LDR_SAMPLES] = {0};
    static int idx = 0;
    samples[idx] = analogRead(LDR_PIN);
    idx = (idx + 1) % LDR_SAMPLES;
    int sum = 0;
    for (int i = 0; i < LDR_SAMPLES; i++) sum += samples[i];
    return sum / LDR_SAMPLES;
}


// ====================================================== SETUP ==================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(">>> System Booting...");

    tft.init();
    tft.setRotation(1);
    tft.setBrightness(255);
    Serial.println("✓ TFT Initialized");

    lv_init();
    Serial.println("✓ LVGL Initialized");

    disp = lv_display_create(screenWidth, screenHeight);
    if (disp == NULL) { Serial.println("❌ Failed to create display!"); while(1); }

    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, &tft);

    ui_init();

    wordclock_init();
    Serial.println("✓ Word Clock Initialized");
    currentAnimation = ANIM_TYPEWRITER;   // change to any ANIM_* value
    animationSpeed   = 1.0f;

    Wire.begin(8, 9);

    if (!rtc.begin(&Wire)) {
        Serial.println("✗ DS3231 not found — falling back to NTP only");
        rtc_ok = false;
    } else {
        rtc_ok = true;
        Serial.println("✓ DS3231 initialized");
    }

    if (!sht30.begin(0x44)) {
        Serial.println("✗ SHT30 not found");
        sht_ok = false;
    } else {
        sht_ok = true;
        Serial.println("✓ SHT30 initialized");
    }

    if (ui_Time == NULL) { Serial.println("❌ CRITICAL: ui_Time NULL!"); while(1); }
    Serial.println("✓ UI Initialized Successfully");

    lv_scr_load(ui_Time);

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = [](void*){ lv_tick_inc(5); },
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, 5000);
    Serial.println("✓ Hardware Timer Started");

    Serial.println(">>> Starting WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
}

// ========================================================= LOOP =========================================

void loop() {
    static unsigned long last_screen_switch = 0;
    static unsigned long last_weather_fetch = 0;
    static unsigned long last_ldr           = 0;
    static bool          wifi_connected     = false;
    static unsigned long last_debug         = 0;

    lv_timer_handler();

    unsigned long ms = millis();

    if (ms - last_debug >= 5000) {
        last_debug = ms;
        Serial.printf("Heap: %d | Screen: %d\n", ESP.getFreeHeap(), current_screen);
    }

    // ────────────────────────── Word Clock ──────────────────────────────────────────
    wordclock_tick();

    if (lastClockUpdate == 0 || ms - lastClockUpdate >= 60000) {
        lastClockUpdate = ms;
        if (rtc_ok) {
            DateTime rtcNow = rtc.now();
            wordclock_update(rtcNow.hour(), rtcNow.minute());
        } else {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                wordclock_update(timeinfo.tm_hour, timeinfo.tm_min);
            }
        }
    }

    // ────────────────────────── WiFi & NTP ──────────────────────────────────────────
    if (!wifi_connected && WiFi.status() == WL_CONNECTED) {
        wifi_connected = true;
        Serial.printf("✓ WiFi Connected | IP: %s\n", WiFi.localIP().toString().c_str());
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        struct tm timeinfo;
        int attempts = 0;
        while (!getLocalTime(&timeinfo) && attempts < 10) { delay(100); attempts++; }

        if (rtc_ok) {
            time_t epoch;
            time(&epoch);
            epoch += gmtOffset_sec;
            rtc.adjust(DateTime((uint32_t)epoch));
            Serial.println("✓ DS3231 synced with NTP");
        }

        last_weather_fetch = ms;
        fetch_weather();
    }

    if (wifi_connected && ms - last_weather_fetch >= 600000) {
        last_weather_fetch = ms;
        fetch_weather();
    }

    // ────────────────────────── LDR Brightness ──────────────────────────────────────
    if (ms - last_ldr >= 1000) {
        last_ldr = ms;
        int raw            = read_brightness();
        int lcd_brightness = map(raw, 0, 4095, MIN_LCD_BRIGHTNESS, MAX_LCD_BRIGHTNESS);
        int led_brightness = map(raw, 0, 4095, MIN_LED_BRIGHTNESS,  MAX_LED_BRIGHTNESS);
        tft.setBrightness(lcd_brightness);
        //FastLED.setBrightness(led_brightness);
    }

    // ────────────────────────── LCD Clock Update ────────────────────────────────────
    if (ms - last_tick >= 1000) {
        last_tick = ms;
        update_clock();
    }

    //───────────────────────── Screen Carousel ─────────────────────────────────────
    if (ms - last_screen_switch >= 7000) {
        last_screen_switch = ms;
        Serial.printf("→ Screen %d\n", current_screen + 1);
        switch_screen();
    }

    delay(5);
}
