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

set(COMPONENT_SRCS
        "main.c"
        "reppanel.c"
        "reppanel.h"
        "request.c"
        "request.h"
        "duet_status_json.h"
        "custom_theme/lv_theme_rep_panel_light.h"
        "custom_theme/lv_theme_rep_panel_light.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
