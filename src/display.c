
// SPDX-License-Identiefier: MIT
// SPDX-CopyRightText: 2025 Julian Scheffers

#include "asp/display.h"
#include <machine/endian.h>
#include <string.h>
#include <sys/unistd.h>
#include "asp/err.h"
#include "bsp/display.h"
#include "err.h"
#include "hal/lcd_types.h"

// Display framebuffer returned by `asp_disp_get_fb`.
uint8_t*   asp_disp_fb      = NULL;
// PAX buffer returned by `asp_disp_get_pax_buf`.
pax_buf_t* asp_disp_pax_buf = NULL;

// Set display brightness.
asp_err_t asp_disp_set_brightness(uint8_t percent) {
    return asp_esp_err_conv(bsp_display_set_backlight_brightness(percent));
}

// Get current display brightness.
asp_err_t asp_disp_get_brightness(uint8_t* percent_out) {
    return asp_esp_err_conv(bsp_display_get_backlight_brightness(percent_out));
}

// Get display parameters of the main display.
asp_err_t asp_disp_get_params(asp_disp_params_t* params_out) {
    if (!params_out) {
        return ASP_ERR_PARAM;
    }
    usleep(0);
    size_t                     h_res, v_res;
    bsp_display_color_format_t color_fmt;
    bsp_display_endianness_t   data_endian;
    ASP_RETURN_ON_ERR(asp_esp_err_conv(bsp_display_get_parameters(&h_res, &v_res, &color_fmt, &data_endian)));
    bsp_display_rotation_t rotation = bsp_display_get_default_rotation();
    switch (rotation) {
        default:
            return ASP_ERR_UNSUPPORTED;
        case BSP_DISPLAY_ROTATION_0:
            params_out->rotation = ASP_DISP_ROTATION_0;
            break;
        case BSP_DISPLAY_ROTATION_90:
            params_out->rotation = ASP_DISP_ROTATION_90;
            break;
        case BSP_DISPLAY_ROTATION_180:
            params_out->rotation = ASP_DISP_ROTATION_180;
            break;
        case BSP_DISPLAY_ROTATION_270:
            params_out->rotation = ASP_DISP_ROTATION_270;
            break;
    }
    switch (color_fmt) {
        default:
            return ASP_ERR_UNSUPPORTED;
        case BSP_DISPLAY_COLOR_FORMAT_1_GREY:
            params_out->pixfmt = ASP_PIXFMT_1_GREY;
            break;
        case BSP_DISPLAY_COLOR_FORMAT_2_GREY:
            params_out->pixfmt = ASP_PIXFMT_2_GREY;
            break;
        case BSP_DISPLAY_COLOR_FORMAT_4_GREY:
            params_out->pixfmt = ASP_PIXFMT_4_GREY;
            break;
        case BSP_DISPLAY_COLOR_FORMAT_8_GREY:
            params_out->pixfmt = ASP_PIXFMT_8_GREY;
            break;
        case BSP_DISPLAY_COLOR_FORMAT_8_332RGB:
            params_out->pixfmt = ASP_PIXFMT_8_332RGB;
            break;
        case BSP_DISPLAY_COLOR_FORMAT_16_565RGB:
            params_out->pixfmt = ASP_PIXFMT_16_565RGB;
            break;
        case BSP_DISPLAY_COLOR_FORMAT_24_888RGB:
            params_out->pixfmt = ASP_PIXFMT_24_888RGB;
            break;
    }
    params_out->width         = h_res;
    params_out->height        = v_res;
    params_out->little_endian = data_endian == BSP_DISPLAY_ENDIAN_LITTLE;
    return ASP_OK;
}

// Get existing framebuffer of the main display.
// If the framebuffer exists, it is written to `*fb_out`.
asp_err_t asp_disp_get_fb(uint8_t** fb_out) {
    if (!fb_out) {
        return ASP_ERR_PARAM;
    }
    *fb_out = asp_disp_fb;
    return ASP_OK;
}

// Get existing PAX buffer of the main display.
// If the PAX buffer exists, it is written to `*fb_out`.
asp_err_t asp_disp_get_pax_buf(pax_buf_t** buf_out) {
    if (!buf_out) {
        return ASP_ERR_PARAM;
    }
    *buf_out = asp_disp_pax_buf;
    return ASP_OK;
}

// Initialize a PAX buffer that matches the main display's format.
asp_err_t asp_disp_init_pax_buf(pax_buf_t* buf) {
    if (!buf) {
        return ASP_ERR_PARAM;
    }

    asp_disp_params_t params;
    ASP_RETURN_ON_ERR(asp_disp_get_params(&params));

    pax_buf_type_t type;
    switch (params.pixfmt) {
        default:
            return ASP_ERR_UNSUPPORTED;
        case ASP_PIXFMT_1_GREY:
            type = PAX_BUF_1_GREY;
            break;
        case ASP_PIXFMT_2_GREY:
            type = PAX_BUF_2_GREY;
            break;
        case ASP_PIXFMT_4_GREY:
            type = PAX_BUF_4_GREY;
            break;
        case ASP_PIXFMT_8_GREY:
            type = PAX_BUF_8_GREY;
            break;
        case ASP_PIXFMT_8_332RGB:
            type = PAX_BUF_8_332RGB;
            break;
        case ASP_PIXFMT_16_565RGB:
            type = PAX_BUF_16_565RGB;
            break;
        case ASP_PIXFMT_24_888RGB:
            type = PAX_BUF_24_888RGB;
            break;
    }

    pax_buf_init(buf, NULL, params.width, params.height, type);
    const bool host_little_endian = BYTE_ORDER == LITTLE_ENDIAN;
    pax_buf_reversed(buf, host_little_endian != params.little_endian);
    switch (params.rotation) {
        case ASP_DISP_ROTATION_0:
            break;
        case ASP_DISP_ROTATION_90:
            pax_buf_set_orientation(buf, PAX_O_ROT_CCW);
            break;
        case ASP_DISP_ROTATION_180:
            pax_buf_set_orientation(buf, PAX_O_ROT_HALF);
            break;
        case ASP_DISP_ROTATION_270:
            pax_buf_set_orientation(buf, PAX_O_ROT_CW);
            break;
    }

    return ASP_OK;
}

// Write the data in `fb` to the entirety of the main display.
// See the `asp_disp_params_t` produced by `asp_disp_get_params` for the buffer format.
asp_err_t asp_disp_write(void const* fb) {
    size_t                     w, h;
    bsp_display_color_format_t color_dummy;
    bsp_display_endianness_t   endian_dummy;
    ASP_RETURN_ON_ERR(asp_esp_err_conv(bsp_display_get_parameters(&w, &h, &color_dummy, &endian_dummy)));
    return asp_esp_err_conv(bsp_display_blit(0, 0, w, h, fb));
}

// Write the data in the PAX buffer `img` to the entirety of the main display.
// Will error if the format of `img` does not match that of the main display.
asp_err_t asp_disp_write_pax(pax_buf_t* img) {
    size_t                     w, h;
    bsp_display_color_format_t color_dummy;
    bsp_display_endianness_t   endian_dummy;
    ASP_RETURN_ON_ERR(asp_esp_err_conv(bsp_display_get_parameters(&w, &h, &color_dummy, &endian_dummy)));
    // TODO: Replace direct access of width and height with the PAX API since 2.0.
    if (w != img->width || h != img->height) {
        return ASP_ERR_PARAM;
    }
    pax_join();  // the host may render asynchronously
    return asp_esp_err_conv(bsp_display_blit(0, 0, w, h, pax_buf_get_pixels(img)));
}

// Write the data in `fb` to part of the main display.
// See the `asp_disp_params_t` produced by `asp_disp_get_params` for the buffer format.
// Will error if the specified region falls (partially) outside the size of the main display.
// **Note: It is up to the user to apply rotation, if needed.**
asp_err_t asp_disp_write_part(void const* fb, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    size_t                     disp_w, disp_h;
    bsp_display_color_format_t color_dummy;
    bsp_display_endianness_t   endian_dummy;
    ASP_RETURN_ON_ERR(asp_esp_err_conv(bsp_display_get_parameters(&disp_w, &disp_h, &color_dummy, &endian_dummy)));
    if (x + w > disp_w || x + w < x || y + h > disp_h || y + h < y) {
        return ASP_ERR_PARAM;
    }
    return asp_esp_err_conv(bsp_display_blit(x, y, w, h, fb));
}

// Write the data in the PAX buffer `img` to part of the main display.
// Will error if the specified region falls (partially) outside the size of the main display.
// **Note: It is up to the user to apply rotation, if needed.**
asp_err_t asp_disp_write_part_pax(pax_buf_t* img, uint16_t x, uint16_t y) {
    // TODO: Replace direct access of width and height with the PAX API since 2.0.
    pax_join();  // the host may render asynchronously
    return asp_disp_write_part(pax_buf_get_pixels(img), x, y, img->width, img->height);
}
