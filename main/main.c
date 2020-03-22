#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <nvs_flash.h>

#include "esp_freertos_hooks.h"
#include "esp_system.h"
#include "lvgl/lvgl.h"

#include "reppanel.h"
#include "esp32_settings.h"

#include "touch_driver.h"
#include "esp32_wifi.h"
#include "reppanel_request.h"
#include "lvgl_driver.h"

#define TAG "Main"

double reprap_chamber_temp_buff[NUM_TEMPS_BUFF] = {0};
int reprap_chamber_temp_curr_pos = 0;
double reprap_babysteps_amount = 0.05;
double reprap_extruder_amounts[NUM_TEMPS_BUFF];
double reprap_extruder_feedrates[NUM_TEMPS_BUFF];
double reprap_move_feedrate = 6000;
double reprap_mcu_temp = 0;
char reprap_firmware_name[100];
char reprap_firmware_version[5];
int num_tools = 0;
reprap_tool_t reprap_tools[MAX_NUM_TOOLS];
reprap_bed_t reprap_bed;
reprap_tool_poss_temps_t reprap_tool_poss_temps;
reprap_bed_poss_temps_t reprap_bed_poss_temps;

static void IRAM_ATTR lv_tick_task(void);

void app_main() {
    lv_init();
    lvgl_driver_init();
    static lv_color_t buf1[DISP_BUF_SIZE];
    static lv_color_t buf2[DISP_BUF_SIZE];
    static lv_disp_buf_t disp_buf;
    lv_disp_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

#if CONFIG_LVGL_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);
#endif

    esp_register_freertos_tick_hook(lv_tick_task);

    init_reprap_buffers();

    static uint32_t user_data = 10;
    lv_task_t *request_printer_status_task = lv_task_create(request_reprap_status_updates, 750, LV_TASK_PRIO_LOW, &user_data);
    lv_task_ready(request_printer_status_task);

    lv_task_t *get_ext_printer_status_task = lv_task_create(request_reprap_ext_status_updates, 5000, LV_TASK_PRIO_LOW, NULL);
    lv_task_ready(get_ext_printer_status_task);

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    read_settings_nvs();
    rep_panel_ui_create();

    wifi_init_sta();
    while (1) {
        vTaskDelay(1);
        lv_task_handler();
    }
}

static void IRAM_ATTR lv_tick_task(void) {
    lv_tick_inc(portTICK_RATE_MS);
}
