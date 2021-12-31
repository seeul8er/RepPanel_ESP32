//
// Copyright (c) 2021 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <driver/gpio.h>
#include <esp_log.h>
#include "screen_saver.h"
#include "sdkconfig.h"
#include "src/lv_core/lv_disp.h"
#include "src/lv_objx/lv_cont.h"
#define TAG "SCREEN_SAVER"

bool screen_saver_active = false;

lv_obj_t *screen_saver;


void set_backlight(bool backlight_on) {
    if (CONFIG_LVGL_ENABLE_BACKLIGHT_CONTROL) {

#if (CONFIG_LVGL_BACKLIGHT_ACTIVE_LVL==1)
        uint8_t tmp = backlight_on ? 1 : 0;
#else
        uint8_t tmp = backlight_on ? 0 : 1;
#endif
        gpio_set_level(CONFIG_LVGL_DISP_PIN_BCKL, tmp);
    }
}

void activate_screen_saver() {
    if (!screen_saver_active) {
        ESP_LOGI(TAG, "Activating screen saver");
        set_backlight(false);
        screen_saver = lv_cont_create(lv_layer_top(), NULL);
        lv_cont_set_fit(screen_saver, LV_FIT_FILL);

        static lv_style_t style_screensaver;
        lv_style_copy(&style_screensaver, &lv_style_plain);
        style_screensaver.image.color = LV_COLOR_BLACK;
        lv_cont_set_style(screen_saver, LV_CONT_STYLE_MAIN, &style_screensaver);

        screen_saver_active = true;
    }
}

void deactivate_screen_saver() {
    if (screen_saver_active) {
        ESP_LOGI(TAG, "Deactivating screen saver");
        set_backlight(true);
        if (screen_saver) lv_obj_del_async(screen_saver);
        screen_saver_active = false;
    }
}