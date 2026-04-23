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
 * Shadow framebuffer
 *   framebuffer[y][x], RGB565, same byte order as the LCD.
 *   150 KB of SRAM — fits comfortably on the RP2350 (520 KB total).
 *---------------------------------------------------------------------------*/
uint16_t framebuffer[FB_H][FB_W];

/*---------------------------------------------------------------------------
 * fb_clear()  —  fill the entire framebuffer with one color.
 *   Call this alongside LCD_Clear() to keep the two in sync.
 *---------------------------------------------------------------------------*/
void fb_clear(uint16_t color)
{
    for (int y = 0; y < FB_H; y++)
        for (int x = 0; x < FB_W; x++)
            framebuffer[y][x] = color;
}

/*===========================================================================
 * BMP EXPORT
 *
 * Format: Windows BMP, 24-bit uncompressed (no palette).
 * Pixel order: BGR (BMP convention), rows stored bottom-up.
 * RGB565 → RGB888 expansion:
 *   R8 = (R5 << 3) | (R5 >> 2)   — fills the 3 LSBs with the top bits
 *   G8 = (G6 << 2) | (G6 >> 4)
 *   B8 = (B5 << 3) | (B5 >> 2)
 *===========================================================================*/

/* Write a little-endian 16-bit value into a byte buffer at offset. */
static void write_u16_le(uint8_t *buf, int offset, uint16_t val)
{
    buf[offset]     = (uint8_t)(val & 0xFF);
    buf[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
}

/* Write a little-endian 32-bit value into a byte buffer at offset. */
static void write_u32_le(uint8_t *buf, int offset, uint32_t val)
{
    buf[offset]     = (uint8_t)(val & 0xFF);
    buf[offset + 1] = (uint8_t)((val >> 8)  & 0xFF);
    buf[offset + 2] = (uint8_t)((val >> 16) & 0xFF);
    buf[offset + 3] = (uint8_t)((val >> 24) & 0xFF);
}

/*
 * BMP file layout:
 *   BITMAPFILEHEADER  14 bytes
 *   BITMAPINFOHEADER  40 bytes
 *   pixel data        width * height * 3 bytes  (no row padding: 240*3=720, divisible by 4)
 *
 * Total header size: 54 bytes.
 * Row stride: FB_W * 3 = 720 bytes — already a multiple of 4, so no padding needed.
 */
#define BMP_HEADER_SIZE   54
#define BMP_ROW_STRIDE    (FB_W * 3)           /* 720 bytes, already 4-byte aligned */
#define BMP_PIXEL_DATA    (FB_H * BMP_ROW_STRIDE)
#define BMP_FILE_SIZE     (BMP_HEADER_SIZE + BMP_PIXEL_DATA)

FRESULT export_bmp(const char *filename)
{
    FIL     fil;
    FRESULT fr;
    UINT    bw;

    /* ---- Build the 54-byte header ---- */
    uint8_t header[BMP_HEADER_SIZE];
    memset(header, 0, sizeof(header));

    /* BITMAPFILEHEADER */
    header[0] = 'B';
    header[1] = 'M';
    write_u32_le(header,  2, BMP_FILE_SIZE);    /* bfSize */
    /* bfReserved1, bfReserved2 already 0 */
    write_u32_le(header, 10, BMP_HEADER_SIZE);  /* bfOffBits */

    /* BITMAPINFOHEADER */
    write_u32_le(header, 14, 40);               /* biSize = 40 */
    write_u32_le(header, 18, (uint32_t)FB_W);   /* biWidth */
    /* biHeight negative = top-down; positive = bottom-up (BMP default).
       We write rows bottom-up in the loop, so use positive height. */
    write_u32_le(header, 22, (uint32_t)FB_H);   /* biHeight (positive → bottom-up) */
    write_u16_le(header, 26, 1);                /* biPlanes */
    write_u16_le(header, 28, 24);               /* biBitCount = 24 */
    /* biCompression = 0 (BI_RGB), rest already 0 */

    /* ---- Open file ---- */
    fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) return fr;

    /* ---- Write header ---- */
    fr = f_write(&fil, header, BMP_HEADER_SIZE, &bw);
    if (fr != FR_OK || bw != BMP_HEADER_SIZE) goto cleanup;

    /*
     * ---- Write pixel data, bottom-up ----
     * BMP stores rows from bottom (y = FB_H-1) to top (y = 0).
     * Each pixel: B, G, R  (BMP is BGR).
     *
     * We build one row at a time into a small stack buffer (720 bytes)
     * to minimise the number of f_write calls without using heap.
     */
    uint8_t row_buf[BMP_ROW_STRIDE];

    for (int y = FB_H - 1; y >= 0; y--)
    {
        for (int x = 0; x < FB_W; x++)
        {
            uint16_t pixel = framebuffer[y][x];

            /* Extract RGB565 channels */
            uint8_t r5 = (pixel >> 11) & 0x1F;
            uint8_t g6 = (pixel >>  5) & 0x3F;
            uint8_t b5 = (pixel      ) & 0x1F;

            /* Expand to 8-bit by replicating the MSBs into the LSBs */
            uint8_t r8 = (r5 << 3) | (r5 >> 2);
            uint8_t g8 = (g6 << 2) | (g6 >> 4);
            uint8_t b8 = (b5 << 3) | (b5 >> 2);

            int idx = x * 3;
            row_buf[idx    ] = b8;   /* BMP is BGR */
            row_buf[idx + 1] = g8;
            row_buf[idx + 2] = r8;
        }

        fr = f_write(&fil, row_buf, BMP_ROW_STRIDE, &bw);
        if (fr != FR_OK || bw != (UINT)BMP_ROW_STRIDE) goto cleanup;
    }

cleanup:
    f_close(&fil);
    return fr;
}

/*===========================================================================
 * CSV EXPORT
 *
 * One line per pixel: "x,y,r,g,b\n"
 * r/g/b are 8-bit values expanded from RGB565 (same expansion as BMP).
 *
 * File size estimate: ~240*320*15 chars ≈ 1.15 MB — ensure your SD card
 * has sufficient space and is FAT32 formatted.
 *
 * To keep stack usage low, rows are assembled into a fixed-size line buffer
 * and flushed to SD one row at a time via a larger write buffer.
 *===========================================================================*/

/* Write buffer: one full row of CSV text.
 * Worst case per pixel: "239,319,255,255,255\n" = 20 chars
 * Worst case per row:   240 * 20 = 4800 chars — fits comfortably on stack. */
#define CSV_ROW_BUF  5000

FRESULT export_csv(const char *filename)
{
    FIL     fil;
    FRESULT fr;
    UINT    bw;

    fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) return fr;

    /* Write header row */
    const char *hdr = "x,y,r,g,b\n";
    fr = f_write(&fil, hdr, strlen(hdr), &bw);
    if (fr != FR_OK) goto cleanup;

    char row_buf[CSV_ROW_BUF];

    for (int y = 0; y < FB_H; y++)
    {
        int pos = 0;

        for (int x = 0; x < FB_W; x++)
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

        fr = f_write(&fil, row_buf, pos, &bw);
        if (fr != FR_OK || bw != (UINT)pos) goto cleanup;
    }

cleanup:
    f_close(&fil);
    return fr;
}
