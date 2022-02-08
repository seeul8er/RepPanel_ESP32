//
// Copyright (c) 2022 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include "reppanel_img_decoder.h"
#include "src/lv_draw/lv_img_decoder.h"

// https://docs.lvgl.io/6.1/overview/image.html?highlight=image%20decoder

/**
 * Get info about a PNG image
 * @param decoder pointer to the decoder where this function belongs
 * @param src can be file name or pointer to a C array
 * @param header store the info here
 * @return LV_RES_OK: no error; LV_RES_INV: can't get the info
 */
static lv_res_t decoder_info(lv_img_decoder_t * decoder, const void * src, lv_img_header_t * header)
{
    /*Check whether the type `src` is known by the decoder*/
    if(is_png(src) == false) return LV_RES_INV;

    header->cf = LV_IMG_CF_RAW_ALPHA;
    header->w = width;
    header->h = height;
}

/**
 * Open a PNG image and return the decided image
 * @param decoder pointer to the decoder where this function belongs
 * @param dsc pointer to a descriptor which describes this decoding session
 * @return LV_RES_OK: no error; LV_RES_INV: can't get the info
 */
static lv_res_t decoder_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{

    /*Check whether the type `src` is known by the decoder*/
    if(is_png(src) == false) return LV_RES_INV;

    /*Decode and store the image. If `dsc->img_data` is `NULL`, the `read_line` function will be called to get the image data line-by-line*/
    dsc->img_data = my_png_decoder(src);

    /*Change the color format if required. For PNG usually 'Raw' is fine*/
    dsc->header.cf = LV_IMG_CF_...

    /*Call a built in decoder function if required. It's not required if`my_png_decoder` opened the image in true color format.*/
    lv_res_t res = lv_img_decoder_built_in_open(decoder, dsc);

    return res;
}

/**
 * Decode `len` pixels starting from the given `x`, `y` coordinates and store them in `buf`.
 * Required only if the "open" function can't open the whole decoded pixel array. (dsc->img_data == NULL)
 * @param decoder pointer to the decoder the function associated with
 * @param dsc pointer to decoder descriptor
 * @param x start x coordinate
 * @param y start y coordinate
 * @param len number of pixels to decode
 * @param buf a buffer to store the decoded pixels
 * @return LV_RES_OK: ok; LV_RES_INV: failed
 */
lv_res_t decoder_built_in_read_line(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc, lv_coord_t x,
                                    lv_coord_t y, lv_coord_t len, uint8_t * buf)
{
    /*With PNG it's usually not required*/

    /*Copy `len` pixels from `x` and `y` coordinates in True color format to `buf` */

}

/**
 * Free the allocated resources
 * @param decoder pointer to the decoder where this function belongs
 * @param dsc pointer to a descriptor which describes this decoding session
 */
static void decoder_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    /*Free all allocated data*/

    /*Call the built-in close function if the built-in open/read_line was used*/
    lv_img_decoder_built_in_close(decoder, dsc);

}
