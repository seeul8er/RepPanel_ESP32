cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS
        externals/lv_port_esp32/components/lvgl_esp32_drivers/lvgl_touch
        externals/lv_port_esp32/components/lvgl_esp32_drivers/lvgl_tft
        externals/lv_port_esp32/components/lvgl)

project(reppanel_esp32)
