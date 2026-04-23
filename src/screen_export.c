/*===========================================================================
 * screen_export.c
 *
 * Shadow framebuffer and SD card image export.
 * See screen_export.h for full documentation.
 *===========================================================================*/

#include "screen_export.h"
#include "ff.h"
#include "lcd.h"
#include <string.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * Shadow framebuffer — FB_MAX × FB_MAX covers both portrait and landscape.
 * Active region is always [0..lcddev.height-1][0..lcddev.width-1].
 *---------------------------------------------------------------------------*/
uint16_t framebuffer[FB_MAX][FB_MAX];

/*---------------------------------------------------------------------------
 * fb_clear()
 *---------------------------------------------------------------------------*/
void fb_clear(uint16_t color)
{
    for (int y = 0; y < FB_MAX; y++)
        for (int x = 0; x < FB_MAX; x++)
            framebuffer[y][x] = color;
}

/*===========================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

static void write_u16_le(uint8_t *buf, int offset, uint16_t val)
{
    buf[offset]     = (uint8_t)(val & 0xFF);
    buf[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
}

static void write_u32_le(uint8_t *buf, int offset, uint32_t val)
{
    buf[offset]     = (uint8_t)(val & 0xFF);
    buf[offset + 1] = (uint8_t)((val >> 8)  & 0xFF);
    buf[offset + 2] = (uint8_t)((val >> 16) & 0xFF);
    buf[offset + 3] = (uint8_t)((val >> 24) & 0xFF);
}

/*===========================================================================
 * BMP EXPORT
 *
 * Uses lcddev.width and lcddev.height at the moment of export so it
 * correctly captures landscape (320×240) or portrait (240×320) content.
 *
 * BMP row stride must be a multiple of 4 bytes.
 *   Landscape: 320 * 3 = 960  — multiple of 4, no padding needed.
 *   Portrait:  240 * 3 = 720  — multiple of 4, no padding needed.
 * Either way we are fine without padding.
 *===========================================================================*/
#define BMP_HEADER_SIZE  54

FRESULT export_bmp(const char *filename)
{
    /* Read actual display dimensions at export time — not compile-time. */
    const int W = (int)lcddev.width;
    const int H = (int)lcddev.height;
    const int row_stride  = W * 3;
    const uint32_t pixel_data_size = (uint32_t)(H * row_stride);
    const uint32_t file_size = BMP_HEADER_SIZE + pixel_data_size;

    FIL     fil;
    FRESULT fr;
    UINT    bw;

    /* ---- Build 54-byte BMP header ---- */
    uint8_t header[BMP_HEADER_SIZE];
    memset(header, 0, sizeof(header));

    /* BITMAPFILEHEADER */
    header[0] = 'B';
    header[1] = 'M';
    write_u32_le(header,  2, file_size);
    write_u32_le(header, 10, BMP_HEADER_SIZE);

    /* BITMAPINFOHEADER */
    write_u32_le(header, 14, 40);
    write_u32_le(header, 18, (uint32_t)W);
    write_u32_le(header, 22, (uint32_t)H); /* positive = bottom-up rows */
    write_u16_le(header, 26, 1);           /* biPlanes */
    write_u16_le(header, 28, 24);          /* biBitCount */
    /* biCompression = 0 (BI_RGB), rest 0 */

    /* ---- Open file ---- */
    fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) return fr;

    /* ---- Write header ---- */
    fr = f_write(&fil, header, BMP_HEADER_SIZE, &bw);
    if (fr != FR_OK || bw != BMP_HEADER_SIZE) goto cleanup;

    /*
     * ---- Write pixel rows, bottom-up (BMP convention) ----
     * Row buffer on the stack: worst case 320*3 = 960 bytes.
     */
    uint8_t row_buf[FB_MAX * 3];

    for (int y = H - 1; y >= 0; y--)
    {
        for (int x = 0; x < W; x++)
        {
            uint16_t pixel = framebuffer[y][x];

            /* RGB565 → RGB888 with bit-replication for full range */
            uint8_t r5 = (pixel >> 11) & 0x1F;
            uint8_t g6 = (pixel >>  5) & 0x3F;
            uint8_t b5 = (pixel      ) & 0x1F;

            uint8_t r8 = (r5 << 3) | (r5 >> 2);
            uint8_t g8 = (g6 << 2) | (g6 >> 4);
            uint8_t b8 = (b5 << 3) | (b5 >> 2);

            int idx = x * 3;
            row_buf[idx    ] = b8;   /* BMP stores BGR */
            row_buf[idx + 1] = g8;
            row_buf[idx + 2] = r8;
        }

        fr = f_write(&fil, row_buf, (UINT)row_stride, &bw);
        if (fr != FR_OK || bw != (UINT)row_stride) goto cleanup;
    }

cleanup:
    f_close(&fil);
    return fr;
}

/*===========================================================================
 * CSV EXPORT
 *
 * Header row: "x,y,r,g,b"
 * One line per pixel, top-left to bottom-right.
 * Uses lcddev.width / lcddev.height so orientation is handled correctly.
 *===========================================================================*/

/* Worst case per row of pixels: 320 pixels * 20 chars = 6400 chars */
#define CSV_ROW_BUF  6500

FRESULT export_csv(const char *filename)
{
    const int W = (int)lcddev.width;
    const int H = (int)lcddev.height;

    FIL     fil;
    FRESULT fr;
    UINT    bw;

    fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) return fr;

    const char *hdr = "x,y,r,g,b\n";
    fr = f_write(&fil, hdr, strlen(hdr), &bw);
    if (fr != FR_OK) goto cleanup;

    char row_buf[CSV_ROW_BUF];

    for (int y = 0; y < H; y++)
    {
        int pos = 0;

        for (int x = 0; x < W; x++)
        {
            uint16_t pixel = framebuffer[y][x];

            uint8_t r5 = (pixel >> 11) & 0x1F;
            uint8_t g6 = (pixel >>  5) & 0x3F;
            uint8_t b5 = (pixel      ) & 0x1F;

            uint8_t r8 = (r5 << 3) | (r5 >> 2);
            uint8_t g8 = (g6 << 2) | (g6 >> 4);
            uint8_t b8 = (b5 << 3) | (b5 >> 2);

            pos += snprintf(row_buf + pos, CSV_ROW_BUF - pos,
                            "%d,%d,%d,%d,%d\n", x, y, r8, g8, b8);
        }

        fr = f_write(&fil, row_buf, (UINT)pos, &bw);
        if (fr != FR_OK || bw != (UINT)pos) goto cleanup;
    }

cleanup:
    f_close(&fil);
    return fr;
}
