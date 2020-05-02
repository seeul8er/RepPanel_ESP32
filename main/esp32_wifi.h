//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef REPPANEL_ESP32_ESP32_WIFI_H
#define REPPANEL_ESP32_ESP32_WIFI_H

void find_mdns_service(const char *service_name, const char *proto);
int resolve_mdns_host(const char *host_name, char *result_ip);

void wifi_init_sta();

void reconnect_wifi();

void get_connection_info(char txt_buffer[200]);

void get_avail_wifi_networks(char *aps);

#endif //REPPANEL_ESP32_ESP32_WIFI_H
