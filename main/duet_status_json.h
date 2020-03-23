//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef REPPANEL_ESP32_DUET_STATUS_JSON_H
#define REPPANEL_ESP32_DUET_STATUS_JSON_H

#define DUET_STATUS "status"
#define DUET_TEMPS  "temps"
#define DUET_TEMPS_BED  "bed"
#define DUET_TEMPS_CURRENT  "current"
#define DUET_TEMPS_BED_CURRENT  "current"
#define DUET_TEMPS_BED_STATE  "state"
#define DUET_TEMPS_BED_HEATER  "heater"
#define DUET_TOOLS              "tools"
#define DUET_TEMPS_TOOLS        DUET_TOOLS
#define DUET_MCU_TEMP   "mcutemp"
#define DUET_FIRM_NAME  "firmwareName"
#define DUET_FIRM_VER   "firmwareVersion"
#define DUET_TEMPS_ACTIVE   "active"
#define DUET_TEMPS_STANDBY   "standby"
#define DUET_ERR            "err"

#define REPRAP_FRAC_PRINTED "fractionPrinted"
#define REPRAP_CURR_LAYER "currentLayer"
#define REPRAP_JOB_DUR "printDuration"
#define REPRAP_TIMES_LEFT "timesLeft"
#define REPRAP_TIMES_LEFT_FILE "file"
#define REPRAP_SIMTIME "simulatedTime"
#define REPRAP_PRINTTIME "printTime"

#define REPRAP_STATUS_PROCESS_CONFIG    'C'
#define REPRAP_STATUS_IDLE              'I'
#define REPRAP_STATUS_BUSY              'B'
#define REPRAP_STATUS_PRINTING          'P'
#define REPRAP_STATUS_DECELERATING      'D'
#define REPRAP_STATUS_STOPPED           'S'
#define REPRAP_STATUS_RESUMING          'R'
#define REPRAP_STATUS_HALTED            'H'
#define REPRAP_STATUS_FLASHING          'F'
#define REPRAP_STATUS_CHANGINGTOOL      'T'
#define REPRAP_STATUS_SIMULATING        'M'

#endif //REPPANEL_ESP32_DUET_STATUS_JSON_H
