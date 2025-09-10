SET(SOURCES screens/ui_ScreenPageMain.c
    screens/ui_ScreenPageGear.c
    screens/ui_ScreenPageRpm.c
    screens/ui_ScreenPageSpeed.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_pngmainback_png.c
    fonts/ui_font_FontBabyGearNumSize48.c
    fonts/ui_font_FontBabySize108.c
    fonts/ui_font_FontBabySize180.c
    fonts/ui_font_FontBabySize200.c
    fonts/ui_font_FontBabySize56.c
    fonts/ui_font_FontTaikongSize32.c
    fonts/ui_font_FontTaikongSize40.c
    fonts/ui_font_FontTaikongSize48.c
    fonts/ui_font_FontTaikongSize56.c
    fonts/ui_font_FontTaikongSize108.c
    fonts/ui_font_FontTaikongSize72.c
    fonts/ui_font_FontTaikongSize64.c
    fonts/ui_font_FontTaikongSize128.c)

add_library(ui ${SOURCES})
