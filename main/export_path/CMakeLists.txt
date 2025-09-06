SET(SOURCES screens/ui_ScreenPageMain.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_pngmainback3_png.c
    images/ui_img_pngmainback2_png.c
    fonts/ui_font_FontBabyGearNumSize48.c
    fonts/ui_font_FontTaikongSize32.c)

add_library(ui ${SOURCES})
