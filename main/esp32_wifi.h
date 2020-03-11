//
// Created by cyber on 10.03.20.
//

#ifndef REPPANEL_ESP32_ESP32_WIFI_H
#define REPPANEL_ESP32_ESP32_WIFI_H

void wifi_init_sta();
void reconnect_wifi();
void get_connection_info(char txt_buffer[200]);
void get_avail_wifi_networks(char *aps);

#endif //REPPANEL_ESP32_ESP32_WIFI_H
