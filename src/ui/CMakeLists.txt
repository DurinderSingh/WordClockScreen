SET(SOURCES ui_comp_seconds.c
    ui_comp.c
    ui_Time.c
    ui_Day_Date_Month.c
    ui_Indoor_Weather.c
    ui_Outdoor_Weather.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_thermometer_icon_png.c
    ui_img_indoor_temperature_png.c
    ui_img_336602858.c
    ui_img_1641329436.c
    ui_img_10717917.c
    ui_img_761082266.c
    ui_img_1663658587.c
    ui_img_965326360.c
    ui_img_665285159.c
    ui_img_85079190.c
    ui_img_987655511.c
    ui_img_1533475108.c
    ui_img_97136411.c
    fonts/ui_font_Airstrike100.c
    fonts/ui_font_Chillax100.c
    fonts/ui_font_Chillax120.c
    fonts/ui_font_ChillaxReg130.c
    fonts/ui_font_ChillaxReg70full.c)

add_library(ui ${SOURCES})
