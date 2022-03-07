cmake_minimum_required(VERSION 3.5)

set(COMPONENT_REQUIRES nvs_flash lvgl_touch lvgl_tft lvgl esp_http_client json mdns lvgl_esp32_drivers)
set(COMPONENT_PRIV_REQUIRES)

file(GLOB_RECURSE INCLUDES "*.h" "lv_drivers/*.h" "lv_examples/*.h" "lvgl/*.h" "./*.h" "custom_themes/lv_theme_rep_panel_dark.h")
file(GLOB_RECURSE SOURCES "*.c" "lv_drivers/*.c" "lv_examples/*.c" "lvgl/*.c" "./*.c" "custom_themes/lv_theme_rep_panel_dark.c" "custom_themes/*.c" "images/*.c" fonts/*.c)

file(GLOB_RECURSE QOI "../externals/qoi/qoi.h")

set(COMPONENT_SRCS main.c ${INCLUDES} ${SOURCES} ${QOI})

set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
