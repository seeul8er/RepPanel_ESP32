//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef REPPANEL_ESP32_REPPANEL_REQUEST_H
#define REPPANEL_ESP32_REPPANEL_REQUEST_H

#define MAX_REQ_ADDR_LENGTH     512

void request_reprap_status_updates(void * pvParameters);
void reprap_wifi_download(char *file);
void reprap_wifi_get_config();
void reprap_wifi_get_fileinfo(char *filename);
void reprap_wifi_get_filelist(char *directory);
void reprap_wifi_send_gcode(char *gcode);
void request_filaments();
void request_macros();
void request_macros_async();
void request_jobs();
void request_jobs_async();
void request_fileinfo(char *file_name);
void reprap_send_gcode(char *gcode_command);

#endif //REPPANEL_ESP32_REPPANEL_REQUEST_H
