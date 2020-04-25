//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/semphr.h>

#include "esp_freertos_hooks.h"
#include "esp_system.h"
#include "lvgl/lvgl.h"

#include "reppanel.h"
#include "esp32_settings.h"

#include "touch_driver.h"
#include "esp32_wifi.h"
#include "reppanel_request.h"
#include "lvgl_driver.h"
#include "esp32_uart.h"

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

//Creates a semaphore to handle concurrent call to lvgl stuff
//If you wish to call *any* lvgl function from other threads/tasks
//you should lock on the very same semaphore!
SemaphoreHandle_t xGuiSemaphore;

static void IRAM_ATTR lv_tick_task(void *arg);

void guiTask();

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {
    //If you want to use a task to create the graphic, you NEED to create a Pinned task
    //Otherwise there can be problem such as memory corruption and so on
    xTaskCreatePinnedToCore(guiTask, "gui", 512 * 11, NULL, 0, NULL, 1);

    TaskHandle_t printer_status_task_handle = NULL;
    xTaskCreate(request_reprap_status_updates, "Printer Status Update Task", 1024 * 11, NULL,
                tskIDLE_PRIORITY, &printer_status_task_handle);
    configASSERT(printer_status_task_handle);
}

static void IRAM_ATTR lv_tick_task(void *arg) {
    (void) arg;
    lv_tick_inc(portTICK_RATE_MS);
}

void guiTask() {
    /* Inspect our own high water mark on entering the task. */
//    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

    xGuiSemaphore = xSemaphoreCreateMutex();
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

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &lv_tick_task,
            /* name is optional, but may help identify the timer when debugging */
            .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    //On ESP32 it's better to create a periodic task instead of esp_register_freertos_tick_hook
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10 * 1000)); //10ms (expressed as microseconds)


    init_reprap_buffers();

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
    init_uart();

//    int c = 0;
    while (1) {
        vTaskDelay(1);
        //Try to lock the semaphore, if success, call lvgl stuff
        if (xSemaphoreTake(xGuiSemaphore, (TickType_t) 10) == pdTRUE) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
//        uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
//        if (c%50000 == 0) {
//            c = 0;
//            ESP_LOGI(TAG, "%i free bytes" , uxHighWaterMark*4);
//        } else c++;
    }
    //A task should NEVER return
    vTaskDelete(NULL);
}
