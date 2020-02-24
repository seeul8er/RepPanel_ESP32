/**
 * @file lv_theme_material.h
 *
 */

#ifndef LV_THEME_REP_PANEL_LIGHT_H
#define LV_THEME_REP_PANEL_LIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#ifdef LV_CONF_INCLUDE_SIMPLE

#include "lv_conf.h"

#else

#include <lvgl/lvgl.h>
#include "lv_conf.h"
#endif

#if LV_USE_THEME_REP_PANEL_LIGHT

/*********************
 *      DEFINES
 *********************/

#define REP_PANEL_DARK LV_COLOR_MAKE(0x2C, 0x2C, 0x2C) // 262626
#define REP_PANEL_HIGHLIGHT LV_COLOR_MAKE(0x19, 0xae, 0xff) // #19aeff

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
lv_theme_t *lv_theme_reppanel_light_init(uint16_t hue, lv_font_t *font);

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

#endif /*LV_THEME_REP_PANEL_LIGHT_H*/
