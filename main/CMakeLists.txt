#idf_component_register(SRCS "main.c"
#        reppanel.c
#        reppanel.h
#        custom_theme/lv_theme_rep_panel_light.c
#        custom_theme/lv_theme_rep_panel_light.h
#        request.c
#        request.h
#        INCLUDE_DIRS "."
#        REQUIRES nvs_flash lvgl_touch lvgl_tft lvgl)

set(COMPONENT_REQUIRES nvs_flash lvgl_touch lvgl_tft lvgl esp_http_client json mdns)
set(COMPONENT_PRIV_REQUIRES )

file(GLOB_RECURSE INCLUDES "lv_drivers/*.h" "lv_examples/*.h"  "lvgl/*.h"  "./*.h" "custom_themes/lv_theme_rep_panel_light.h" )
file(GLOB_RECURSE SOURCES  "lv_drivers/*.c" "lv_examples/*.c"  "lvgl/*.c" "./*.c" "custom_themes/lv_theme_rep_panel_light.c" "custom_themes/*.c" "images/*.c" fonts/*.c)

#set(COMPONENT_SRCS
#        "main.c"
#        "reppanel.c"
#        "reppanel.h"
#        "request.c"
#        "request.h"
#        "reppanel_helper.c"
#        "reppanel_helper.h"
#        "reppanel_info.c"
#        "reppanel_info.h"
#        "reppanel_machine.c"
#        "reppanel_machine.h"
#        "reppanel_process.c"
#        "reppanel_process.h"
#        "reppanel_settings.h"
#        "duet_status_json.h"
#        "images/arrowgoleft.c"
#        "images/arrowgoright.c"
#        "images/chamber_tmp.c"
#        "images/consolebutton.c"
#        "images/mainmenubutton.c"
#        "custom_themes/lv_theme_rep_panel_light.h"
#        "custom_themes/lv_theme_rep_panel_light.c")
set(COMPONENT_SRCS main.c ${INCLUDES} ${SOURCES})

set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
