#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "ui/ui.h"
#include <time.h>

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

// --- CLOCK LOGIC ---
unsigned long last_tick = 0;
bool colon_visible = true;

void update_clock() {
    if (current_screen != 0) return;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

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

    // Blink colon
    colon_visible = !colon_visible;
    if (ui_LabelSeconds) {
        if (colon_visible) {
            lv_obj_clear_flag(ui_LabelSeconds, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ui_LabelSeconds, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Update Second Bar
    if (ui_SecondBar) {
        int bar_value = map(timeinfo.tm_sec, 0, 59, 0, 100);
        lv_bar_set_value(ui_SecondBar, bar_value, LV_ANIM_ON);
    }
}

// --- SCREEN SWITCHING ---
void switch_screen() {
    Serial.println(">>> SWITCHING SCREEN <<<");
    Serial.print("Current: ");
    Serial.println(current_screen);
    
    if (current_screen == 0) {
        lv_scr_load(ui_Day_Date_Month);
        current_screen = 1;
        
        if (ui_date) lv_label_set_text(ui_date, "07");
        if (ui_Month) lv_label_set_text(ui_Month, "FEB");
        if (ui_day) lv_label_set_text(ui_day, "FRI");
        
    } else if (current_screen == 1) {
        lv_scr_load(ui_Indoor_Weather);
        current_screen = 2;
        
    } else {
        lv_scr_load(ui_Time);
        current_screen = 0;
    }
    
    // Force immediate refresh
    lv_refr_now(NULL);
    
    Serial.print("New: ");
    Serial.println(current_screen);
}


void setup()
{
    delay(5000);
    Serial.begin(115200);
    while (!Serial) { delay(100); }
    delay(1000);
    
    Serial.println("\n=== ESP32-C3 STARTING ===");
    
    tft.init();
    tft.setRotation(1);
    tft.setBrightness(255);
    Serial.println("Display OK");

    lv_init();
    Serial.println("LVGL OK");

    disp = lv_display_create(320, 240);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, &tft);
    Serial.println("Display configured");

    ui_init();
    Serial.println("UI initialized");

    if (ui_SecondBar) {
        lv_obj_set_style_anim_duration(ui_SecondBar, 1000, 0);
    }

    struct timeval tv;
    tv.tv_sec = 1738961400;
    settimeofday(&tv, NULL);
    
    lv_scr_load(ui_Time);
    Serial.println("Clock screen loaded");
    
    Serial.println("\n=== SETUP COMPLETE ===\n");
    Serial.println("Screens will switch every 7 seconds\n");
}

void loop()
{
    static unsigned long last_screen_switch = 0;
    
    // Call timer handler multiple times per loop
    lv_timer_handler();
    
    // Update Clock every 1 second
    if (millis() - last_tick > 1000) {
        last_tick = millis();
        update_clock();
    }
    
    // Switch screens every 7 seconds using millis()
    if (millis() - last_screen_switch > 7000) {
        last_screen_switch = millis();
        switch_screen();
    }
    
    delay(5);
}

