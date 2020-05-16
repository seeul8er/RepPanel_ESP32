//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef LV_THEME_REP_PANEL_DARK_H
#define LV_THEME_REP_PANEL_DARK_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <lvgl/lvgl.h>
#include "lv_conf.h"

#if LV_USE_THEME_REP_PANEL_DARK

/*********************
 *      DEFINES
 *********************/

#define REP_PANEL_DARK LV_COLOR_MAKE(0x12, 0x12, 0x12)
#define REP_PANEL_DARK_ACCENT LV_COLOR_MAKE(0xb9, 0xfa, 0x46)
#define REP_PANEL_DARK_ACCENT_ALT1   LV_COLOR_MAKE(0x4b, 0xb4, 0x5e)
#define REP_PANEL_DARK_ACCENT_ALT2   LV_COLOR_MAKE(0x27, 0x46, 0x46)
#define REP_PANEL_DARK_HIGHLIGHT LV_COLOR_MAKE(0x19, 0xae, 0xff)
#define REP_PANEL_DARK_HIGHLIGHT_DARK LV_COLOR_MAKE(0x0a, 0x51, 0xc4)
#define REP_PANEL_DARK_TEXT LV_COLOR_MAKE(0xe6, 0xe6, 0xe6)
#define REP_PANEL_DARK_ACCENT_GREY LV_COLOR_MAKE(0x20, 0x24, 0x25)
#define REP_PANEL_DARK_RED LV_COLOR_MAKE(0xcc, 0x23, 0x47)
#define REP_PANEL_DARK_DARK_RED LV_COLOR_MAKE(0x63, 0x09, 0x2c)
#define REP_PANEL_DARK_GREEN LV_COLOR_MAKE(0x4b, 0xb4, 0x5e)

#define REP_PANEL_DARK_ACCENT_STR "#b9fa46"
#define REP_PANEL_DARK_ACCENT_ALT1_STR "#4bb45e"
#define REP_PANEL_DARK_ACCENT_ALT2_STR   "#274646"
#define REP_PANEL_DARK_TEXT_STR "#e6e6e6"

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize the material theme
 * @param hue [0..360] hue value from HSV color space to define the theme's base color
 * @param font pointer to a font (NULL to use the default)
 * @return pointer to the initialized theme
 */
lv_theme_t *lv_theme_reppanel_dark_init(uint16_t hue, lv_font_t *font);

/**
 * Get a pointer to the theme
 * @return pointer to the theme
 */
lv_theme_t *lv_theme_get_reppanel_light(void);

/**********************
 *      MACROS
 **********************/

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_THEME_REP_PANEL_DARK_H*/
