//
// Created by cyber on 25.02.20.
//

#ifndef REPPANEL_ESP32_DUET_STATUS_JSON_H
#define REPPANEL_ESP32_DUET_STATUS_JSON_H

#define DUET_STATUS "status"
#define DUET_TEMPS  "temps"
#define DUET_TEMPS_BED  "bed"
#define DUET_TEMPS_CURRENT  "current"
#define DUET_TEMPS_BED_CURRENT  "current"
#define DUET_TEMPS_BED_STATE  "state"

#define REPRAP_FRAC_PRINTED "fractionPrinted"

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

#endif //REPPANEL_ESP32_DUET_STATUS_JSON_H
