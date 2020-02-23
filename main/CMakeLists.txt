idf_component_register(SRCS "main.c" reppanel.c reppanel.h custom_theme/lv_theme_rep_panel_light.c custom_theme/lv_theme_rep_panel_light.h
                    INCLUDE_DIRS "."
                    REQUIRES lvgl_touch lvgl_tft lvgl)
