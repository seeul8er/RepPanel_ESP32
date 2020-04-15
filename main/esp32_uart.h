//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0
//

#ifndef REPPANEL_ESP32_ESP32_UART_H
#define REPPANEL_ESP32_ESP32_UART_H

#include "reppanel_request.h"

#define UART_DATA_BUFF_LEN  1024 * 3

void init_uart();
void reppanel_write_uart(char *buffer, int buffer_len);
bool reppanel_is_uart_connected();

void reppanel_read_response(uart_response_buff_t *receive_buff);
void reppanel_request_response(uart_response_buff_t *receive_buff, int seq_num);

#endif //REPPANEL_ESP32_ESP32_UART_H
