//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0
//

#include <driver/uart.h>
#include "driver/gpio.h"
#include <string.h>
#include <esp_log.h>
#include <sys/time.h>

#include "esp32_uart.h"
#include "reppanel.h"

#define TAG "ESP_UART"

#define UART_READ_TIMEOUT   20 // ms
#define UART_RESP_TIMEOUT   15000000 // us [1s]
#define MAX_NUM_TIMEOUTS    1 // max number of timeouts to tolerate till we switch to WiFi

const int uart_num = UART_NUM_2;
bool uart_inited = false;
static int timeout_cnt = 0;

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
    // Install UART driver using an event queue here
    ESP_ERROR_CHECK(uart_driver_install(uart_num, 1024*3, uart_buffer_size, 10, NULL, 0));
    ESP_LOGI(TAG, "UART%i init done", uart_num);
    uart_inited = true;
}

bool reppanel_is_uart_connected() {
    if (!uart_inited) {
        ESP_LOGW(TAG, "Uart not inited - skipping connection check");
        return false;
    }

    char comm[] = {"M408 S0\r\n"};
    if (uart_write_bytes(uart_num, (const char *) comm, strlen(comm)) != strlen(comm)) {
        ESP_LOGW(TAG, "Duet connection check - Could not push all bytes to write buffer!");
        return false;
    }
    int length = 0;
    uart_wait_tx_done(uart_num, UART_READ_TIMEOUT / portTICK_RATE_MS);
    ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_num, (size_t *) &length));
    uint8_t data = 0;
    while (length > 0 && data != '\n') {
        uart_read_bytes(uart_num, &data, 1, 20 / portTICK_RATE_MS);
        length++;
    }
    uart_flush(uart_num);
    ESP_LOGI(TAG, "Checked for connected UART- received %i bytes", length);
    return (length > 0);
}

void reppanel_write_uart(char *buffer, int buffer_len) {
    if (uart_inited) {
        if (uart_write_bytes(uart_num, (const char *) buffer, buffer_len) != buffer_len)
            ESP_LOGW(TAG, "Could not push all bytes to write buffer!");
        uart_write_bytes(uart_num, "\r\n", 2);
        uart_wait_tx_done(uart_num, UART_READ_TIMEOUT / portTICK_RATE_MS);
    }
}

/**
 * Read from UART - Timeout not really working!
 * @param receive_buff
 * @return
 */
int reppanel_read_uart(uart_response_buff_t *receive_buff) {
    if (uart_inited) {
        int length = 0;
        ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_num, (size_t *) &length));
        if (length <= UART_RESP_BUFF_SIZE)
            length = uart_read_bytes(uart_num, &receive_buff->buffer[receive_buff->buf_pos], length, UART_READ_TIMEOUT / portTICK_RATE_MS);
        else {
            ESP_LOGE(TAG, "UART response too big for buffer!");
            length = uart_read_bytes(uart_num, &receive_buff->buffer[receive_buff->buf_pos], UART_RESP_BUFF_SIZE, UART_READ_TIMEOUT / portTICK_RATE_MS);
        }
        receive_buff->buf_pos += length;
        ESP_LOGD(TAG, "Received %i", length);
        return length;
    } else {
        ESP_LOGW(TAG, "UART not inited - skipping read UART");
        return -1;
    }
}

//int readline(char readch, uart_response_buff_t *receive_buff) {
//    static int pos = 0;
//    int rpos;
//    if (readch > 0) {
//        switch (readch) {
//            case '\n': // Return on new-line
//                rpos = pos;
//                pos = 0;  // Reset position index ready for next time
//                receive_buff->buf_pos = rpos;
//                return rpos;
//            default:
//                if (pos < UART_RESP_BUFF_SIZE-1) {
//                    receive_buff->buffer[pos++] = readch;
//                    receive_buff->buffer[pos] = 0;
//                }
//        }
//    }
//    return 0;
//}

//bool reppanel_read_response(uart_response_buff_t *receive_buff) {
//    uint8_t data = 0;
//    while(1) {
//        int len = uart_read_bytes(uart_num, &data, 1, 20 / portTICK_RATE_MS);
//        if (len > 0 && readline(data, receive_buff) > 0) {
//            ESP_LOGI(TAG, "---> Response complete with %i bytes", receive_buff->buf_pos);
//            receive_buff->buffer[receive_buff->buf_pos] = '\0';   // replace new line with string end char to pars JSON
//            ESP_LOGI(TAG, "%s", receive_buff->buffer);
//            timeout_cnt = 0;
//            return true;
//        }
//    }
//}

void read_timeout() {
    timeout_cnt++;
    ESP_LOGW(TAG, "Read timeout");
    if (timeout_cnt >= MAX_NUM_TIMEOUTS) {
        ESP_LOGW(TAG, "Detected %i timeouts on UART. Switching to WiFi", timeout_cnt);
        rp_conn_stat = REPPANEL_WIFI_CONNECTED_DUET_DISCONNECTED;
        timeout_cnt = 0;
    }
}

/**
 * Read a response from Duet from UART and write it to the provided buffer
 * @param receive_buff Receiving buffer
 * @return False in case of read timeout or not inited UART, True in case of valid response
 */
bool reppanel_read_response(uart_response_buff_t *receive_buff) {
    if (!uart_inited) {
        ESP_LOGW(TAG, "Can not read response - Wait for UART init first");
        return false;
    }
    receive_buff->buf_pos = 0;

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    int64_t start_time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;

    reppanel_read_uart(receive_buff);
    // get complete response
    while (receive_buff->buffer[receive_buff->buf_pos - 1] != '\n' && receive_buff->buf_pos < UART_RESP_BUFF_SIZE) {
        reppanel_read_uart(receive_buff);
        gettimeofday(&tv_now, NULL);
        int64_t current_time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
        if (current_time_us - start_time_us > UART_RESP_TIMEOUT) {
            read_timeout();
            return false;
        }
    }
    timeout_cnt = 0;
    ESP_LOGI(TAG, "---> Response complete with %i bytes", receive_buff->buf_pos);
    receive_buff->buffer[receive_buff->buf_pos-1] = '\0';   // replace new line with string end char to pars JSON
    ESP_LOGI(TAG, "%s", receive_buff->buffer);
    return true;
}
