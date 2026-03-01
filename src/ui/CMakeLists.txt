SET(SOURCES ui_Time.c
    ui_Day_Date_Month.c
    ui_Indoor_Weather.c
    ui_Outdoor_Weather.c
    ui_AQIHumidity.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_thermometer_icon_png.c
    ui_img_902301562.c
    ui_img_336602858.c
    ui_img_sun_png.c
    ui_img_497089268.c
    ui_img_cloud_png.c
    ui_img_storm_png.c
    ui_img_fog_png.c
    ui_img_cloudynight_png.c
    ui_img_390541528.c
    ui_img_humidity1_png.c
    fonts/ui_font_Chillax100.c
    fonts/ui_font_Chillax120.c
    fonts/ui_font_ChillaxReg130.c
    fonts/ui_font_ChillaxReg70full.c
    fonts/ui_font_DateFont.c
    fonts/ui_font_Jersy.c
    fonts/ui_font_Mythical.c
    fonts/ui_font_TimeFont.c)

add_library(ui ${SOURCES})
