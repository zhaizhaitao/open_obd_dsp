SET(SOURCES screens/ui_ScreenPageMain.c
    screens/ui_ScreenPageGear.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_pngmainback3_png.c
    images/ui_img_pngmainback2_png.c
    fonts/ui_font_FontBabyGearNumSize48.c
    fonts/ui_font_FontBabySize108.c
    fonts/ui_font_FontBabySize180.c
    fonts/ui_font_FontBabySize200.c
    fonts/ui_font_FontBabySize56.c
    fonts/ui_font_FontTaikongSize32.c
    fonts/ui_font_FontTaikongSize40.c)

add_library(ui ${SOURCES})
