#ifndef SCREEN_EXPORT_H
#define SCREEN_EXPORT_H

/*===========================================================================
 * screen_export.h
 *
 * Shadow framebuffer + SD card export module.
 *
 * The ILI9341 LCD is write-only over SPI, so we maintain a 240x320 16-bit
 * (RGB565) shadow framebuffer in RAM that mirrors every pixel written to the
 * display.  When an export is triggered, this buffer is the data source.
 *
 * Two export formats are supported:
 *   - BMP  : 24-bit uncompressed Windows BMP, readable on any OS.
 *   - CSV  : comma-separated "x,y,r,g,b" rows, one per pixel.
 *
 * RAM cost of the framebuffer:
 *   240 * 320 * 2 bytes = 153,600 bytes (~150 KB)
 * The RP2350 has 520 KB of SRAM, so this is feasible.
 *
 * Usage
 * -----
 *  1. Apply the 4 edits described in LCD_PATCH_INSTRUCTIONS.txt to lcd.c.
 *     After that, the framebuffer is kept in sync automatically — no
 *     changes needed anywhere else in your drawing code.
 *
 *  2. Mount the SD card once at startup via f_mount() or the mount() shell
 *     command from sdcard.c.
 *
 *  3. From your FSM export state, call either:
 *       export_bmp("SCREEN.BMP")   -- 24-bit BMP, opens on any OS
 *       export_csv("SCREEN.CSV")   -- x,y,r,g,b per pixel
 *     Both return FR_OK (0) on success.
 *
 *  FAT 8.3 filename rules apply (FF_USE_LFN == 0 in ffconf.h):
 *  max 8 chars before the dot, max 3 extension chars, no lowercase needed.
 *===========================================================================*/

#include <stdint.h>
#include "ff.h"   /* FatFs */
#include "lcd.h"  /* LCD_W, LCD_H, u16 */

/*---------------------------------------------------------------------------
 * Shadow framebuffer
 *   framebuffer[y][x]  -- row-major, RGB565 big-endian (same byte order that
 *   LCD_WriteData16 sends: high byte first).
 *---------------------------------------------------------------------------*/
#define FB_W  LCD_W   /* 240 */
#define FB_H  LCD_H   /* 320 */

/* The actual buffer is defined in screen_export.c */
extern uint16_t framebuffer[FB_H][FB_W];

/*---------------------------------------------------------------------------
 * fb_set_pixel()
 *   Call this every time you write a pixel to the display.
 *   color is the same RGB565 value you pass to LCD_DrawPoint().
 *---------------------------------------------------------------------------*/
static inline void fb_set_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x < FB_W && y < FB_H)
        framebuffer[y][x] = color;
}

/*---------------------------------------------------------------------------
 * fb_clear()
 *   Mirror of LCD_Clear() — fills the entire framebuffer with one color.
 *---------------------------------------------------------------------------*/
void fb_clear(uint16_t color);

/*---------------------------------------------------------------------------
 * export_bmp(filename)
 *   Write a 24-bit uncompressed BMP to the SD card.
 *   Pixels are converted from RGB565 → RGB888 during export.
 *   BMP rows are stored bottom-up per spec, so we reverse row order here.
 *
 *   Returns FR_OK (0) on success, a FatFs FRESULT error code on failure.
 *---------------------------------------------------------------------------*/
FRESULT export_bmp(const char *filename);

/*---------------------------------------------------------------------------
 * export_csv(filename)
 *   Write one line per pixel: "x,y,r,g,b\n"
 *   Values r/g/b are 8-bit (expanded from RGB565).
 *
 *   Returns FR_OK (0) on success, a FatFs FRESULT error code on failure.
 *---------------------------------------------------------------------------*/
FRESULT export_csv(const char *filename);

#endif /* SCREEN_EXPORT_H */
