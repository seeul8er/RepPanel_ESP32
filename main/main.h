//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0
//

#ifndef REPPANEL_ESP32_MAIN_H
#define REPPANEL_ESP32_MAIN_H

#include "freertos/FreeRTOS.h"
#include <freertos/semphr.h>

// Creates a semaphore to handle concurrent call to lvgl stuff
// If you wish to call *any* lvgl function from other threads/tasks
// you should lock on the very same semaphore!
extern SemaphoreHandle_t xGuiSemaphore;

#endif //REPPANEL_ESP32_MAIN_H
