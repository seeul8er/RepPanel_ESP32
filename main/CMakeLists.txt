set(COMPONENT_REQUIRES nvs_flash lvgl_touch lvgl_tft lvgl esp_http_client json mdns lvgl_esp32_drivers)
set(COMPONENT_PRIV_REQUIRES )

file(GLOB_RECURSE INCLUDES "*.h" "lv_drivers/*.h" "lv_examples/*.h"  "lvgl/*.h"  "./*.h" "custom_themes/lv_theme_rep_panel_light.h" )
file(GLOB_RECURSE SOURCES "*.c" "lv_drivers/*.c" "lv_examples/*.c"  "lvgl/*.c" "./*.c" "custom_themes/lv_theme_rep_panel_light.c" "custom_themes/*.c" "images/*.c" fonts/*.c)

set(COMPONENT_SRCS main.c ${INCLUDES} ${SOURCES})

set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
