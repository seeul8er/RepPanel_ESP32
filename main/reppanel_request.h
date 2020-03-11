//
// Created by cyber on 24.02.20.
//

#ifndef REPPANEL_ESP32_REPPANEL_REQUEST_H
#define REPPANEL_ESP32_REPPANEL_REQUEST_H

#define PRINTER_NAME            "CyberPrint"
#define MAX_REQ_ADDR_LENGTH     1024

void request_reprap_status_updates(lv_task_t *task);

#endif //REPPANEL_ESP32_REPPANEL_REQUEST_H
