#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <nvs_flash.h>

#include "esp_freertos_hooks.h"
#include "esp_system.h"
#include "lvgl/lvgl.h"

#include "reppanel.h"
#include "esp32_settings.h"

#include "disp_spi.h"
#include "disp_driver.h"
#include "tp_spi.h"
#include "touch_driver.h"
#include "esp32_wifi.h"


#define TAG "Main"

// Detect the use of a shared SPI Bus and verify the user specified the same SPI bus for both touch and tft
#if (CONFIG_LVGL_TOUCH_CONTROLLER == 1 || CONFIG_LVGL_TOUCH_CONTROLLER == 3) && TP_SPI_MOSI == DISP_SPI_MOSI && TP_SPI_CLK == DISP_SPI_CLK
#if CONFIG_LVGL_TFT_DISPLAY_SPI_HSPI == 1
#define TFT_SPI_HOST HSPI_HOST
#else
#define TFT_SPI_HOST VSPI_HOST
#endif

#if CONFIG_LVGL_TOUCH_CONTROLLER_SPI_HSPI == 1
#define TOUCH_SPI_HOST HSPI_HOST
#else
#define TOUCH_SPI_HOST VSPI_HOST
#endif

#if TFT_SPI_HOST != TOUCH_SPI_HOST
#error You must specifiy the same SPI host for both display and input driver
#endif

#define SHARED_SPI_BUS
#endif

#ifdef SHARED_SPI_BUS
/* Example function that configure two spi devices (tft and touch controllers) into the same spi bus */
static void configure_shared_spi_bus(void);
#endif

static void IRAM_ATTR lv_tick_task(void);

void app_main() {
    lv_init();

    /* Interface and driver initialization */
#ifdef SHARED_SPI_BUS
    /* Configure one SPI bus for the two devices */
    configure_shared_spi_bus();

    /* Configure the drivers */
    disp_driver_init(false);
#if CONFIG_LVGL_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
    touch_driver_init(false);
#endif
#else
    /* Otherwise configure the SPI bus and devices separately inside the drivers*/
    disp_driver_init(true);
#if CONFIG_LVGL_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
    touch_driver_init(true);
#endif
#endif

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

    static uint32_t user_data = 10;
    // lv_task_t *request_task = lv_task_create(request_reprap_status_updates, 750, LV_TASK_PRIO_MID, &user_data);
    // lv_task_ready(request_task);

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
