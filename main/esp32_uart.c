//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0
//

#include <driver/uart.h>
#include "driver/gpio.h"
#include <string.h>
#include <esp_log.h>

#include "esp32_uart.h"

#define TAG "ESP_UART"

const int uart_num = UART_NUM_2;


void init_uart() {
    uart_config_t uart_config = {
            .baud_rate = 57600,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, GPIO_NUM_17, GPIO_NUM_16, GPIO_NUM_18, GPIO_NUM_19));
    // Setup UART buffered IO with event queue
    const int uart_buffer_size = UART_DATA_BUFF_LEN;
    QueueHandle_t uart_queue;
    // Install UART driver using an event queue here
    ESP_ERROR_CHECK(uart_driver_install(uart_num, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));
}

bool reppanel_is_uart_connected() {
    char comm[] = {"M408 S0"};
    if (uart_write_bytes(uart_num, (const char *) comm, strlen(comm)) != strlen(comm))
        ESP_LOGW(TAG, "Ping - Could not push all bytes to write buffer!");
    int length = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_num, (size_t *) &length));
    return (length > 0);
}

void reppanel_write_uart(char *buffer, int buffer_len) {
    if (uart_write_bytes(uart_num, (const char *) buffer, buffer_len) != buffer_len)
        ESP_LOGW(TAG, "Could not push all bytes to write buffer!");
}

int reppanel_read_uart(uint8_t *data_buff) {
    int length = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_num, (size_t *) &length));
    length = uart_read_bytes(uart_num, data_buff, length, 25);
    return length;
}
