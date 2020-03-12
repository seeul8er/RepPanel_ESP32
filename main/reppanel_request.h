//
// Created by cyber on 24.02.20.
//

#ifndef REPPANEL_ESP32_REPPANEL_REQUEST_H
#define REPPANEL_ESP32_REPPANEL_REQUEST_H

#define MAX_REQ_ADDR_LENGTH     512

void request_reprap_status_updates(lv_task_t *task);
void reprap_wifi_download(char *file);
void reprap_wifi_get_config();
void reprap_wifi_get_fileinfo(char *filename);
void reprap_wifi_get_filelist(char *directory);
void reprap_wifi_send_gcode(char *gcode);

#endif //REPPANEL_ESP32_REPPANEL_REQUEST_H
