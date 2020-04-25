//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0
//

#include <driver/uart.h>
#include "driver/gpio.h"
#include <string.h>
#include <esp_log.h>

#include "esp32_uart.h"
#include "reppanel.h"

#define TAG "ESP_UART"

const int uart_num = UART_NUM_2;


void init_uart() {
    ESP_LOGI(TAG, "Initing UART%i ...", uart_num);
    uart_config_t uart_config = {
            .baud_rate = 57600,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, GPIO_NUM_17, GPIO_NUM_16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // Setup UART buffered IO with event queue
    const int uart_buffer_size = UART_DATA_BUFF_LEN;
    QueueHandle_t uart_queue;
    // Install UART driver using an event queue here
    ESP_ERROR_CHECK(uart_driver_install(uart_num, 1024*3, uart_buffer_size, 10, NULL, 0));
    ESP_LOGI(TAG, "UART%i init done", uart_num);
}

bool reppanel_is_uart_connected() {
    char comm[] = {"M408 S0\r\n"};
    if (uart_write_bytes(uart_num, (const char *) comm, strlen(comm)) != strlen(comm)) {
        ESP_LOGW(TAG, "Ping - Could not push all bytes to write buffer!");
        return false;
    }
    int length = 0;
    uart_wait_tx_done(uart_num, 100 / portTICK_RATE_MS);
    ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_num, (size_t *) &length));
    uart_flush(uart_num);
    ESP_LOGI(TAG, "Checked for connected UART- received %i bytes", length);
    return (length > 0);
}

void reppanel_write_uart(char *buffer, int buffer_len) {
    if (uart_write_bytes(uart_num, (const char *) buffer, buffer_len) != buffer_len)
        ESP_LOGW(TAG, "Could not push all bytes to write buffer!");
    uart_write_bytes(uart_num, "\r\n", 2);
    uart_wait_tx_done(uart_num, 100 / portTICK_RATE_MS);
}

int reppanel_read_uart(uart_response_buff_t *receive_buff) {
    int length = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_num, (size_t *) &length));
    if (length <= UART_RESP_BUFF_SIZE)
        length = uart_read_bytes(uart_num, &receive_buff->buffer[receive_buff->buf_pos], length, 20 / portTICK_RATE_MS);
    else {
        ESP_LOGE(TAG, "UART response too big for buffer!");
        length = uart_read_bytes(uart_num, &receive_buff->buffer[receive_buff->buf_pos], UART_RESP_BUFF_SIZE, 20 / portTICK_RATE_MS);
    }
    receive_buff->buf_pos += length;
    if (length > 0) ESP_LOGD(TAG, "Received %i bytes", length);
    return length;
}

void reppanel_read_response(uart_response_buff_t *receive_buff) {
    receive_buff->buf_pos = 0;
    reppanel_read_uart(receive_buff);
    // get complete response
    while (receive_buff->buffer[receive_buff->buf_pos - 1] != '\n' &&
           receive_buff->buf_pos < UART_RESP_BUFF_SIZE) {
        reppanel_read_uart(receive_buff);
    }
    ESP_LOGI(TAG, "---> Response complete with %i bytes", receive_buff->buf_pos);
    receive_buff->buffer[receive_buff->buf_pos-1] = '\0';   // replace new line with string end char to pars JSON
    ESP_LOGD(TAG, "%s", receive_buff->buffer);
}

void reppanel_request_response(uart_response_buff_t *receive_buff, int seq_num) {

}
