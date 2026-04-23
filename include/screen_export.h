#ifndef SCREEN_EXPORT_H
#define SCREEN_EXPORT_H

/*===========================================================================
 * screen_export.h
 *
 * Shadow framebuffer + SD card export module.
 *
 * The ILI9341 LCD is write-only over SPI, so we maintain a shadow
 * framebuffer in RAM that mirrors every pixel written to the display.
 *
 * IMPORTANT — orientation:
 *   The display can run in portrait (240×320) or landscape (320×240)
 *   depending on USE_HORIZONTAL in lcd.h / lcd.c.  Your project uses
 *   coordinates up to x=310, confirming landscape mode (lcddev.width=320,
 *   lcddev.height=240).  The framebuffer is sized for the worst case
 *   (320×320) and the actual active region is read from lcddev at runtime.
 *
 * RAM cost: 320 * 320 * 2 = 204,800 bytes (~200 KB).
 * RP2350 has 520 KB — still fine.
 *
 * Usage
 * -----
 *  1. Apply the 4 edits in LCD_PATCH_INSTRUCTIONS.txt to lcd.c.
 *     After that, the framebuffer stays in sync automatically.
 *
 *  2. Mount the SD card once at startup via f_mount() / mount().
 *
 *  3. From your FSM export state, call either:
 *       export_bmp("SCREEN.BMP")   -- 24-bit BMP, opens on any OS
 *       export_csv("SCREEN.CSV")   -- x,y,r,g,b per pixel
 *     Both return FR_OK (0) on success.
 *
 *  FAT 8.3 filename rules apply (FF_USE_LFN == 0 in ffconf.h):
 *  max 8 chars before the dot, max 3 extension chars.
 *===========================================================================*/

#include <stdint.h>
#include "ff.h"
#include "lcd.h"   /* for lcddev, u16 */

/*---------------------------------------------------------------------------
 * Framebuffer — sized for the maximum possible dimension in either
 * orientation (320 in both axes covers portrait and landscape).
 * Active pixels are [0 .. lcddev.height-1][0 .. lcddev.width-1].
 *---------------------------------------------------------------------------*/
#define FB_MAX  320   /* max of LCD_W and LCD_H */

extern uint16_t framebuffer[FB_MAX][FB_MAX];

/*---------------------------------------------------------------------------
 * fb_set_pixel(x, y, color)
 *   Mirror of a single pixel write to the display.
 *   x/y are in display coordinates (i.e. the same values passed to
 *   LCD_DrawPoint or LCD_SetWindow).
 *---------------------------------------------------------------------------*/
static inline void fb_set_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x < FB_MAX && y < FB_MAX)
        framebuffer[y][x] = color;
}

/*---------------------------------------------------------------------------
 * fb_clear(color)  —  fill the whole framebuffer with one color.
 *   Call this alongside LCD_Clear().
 *---------------------------------------------------------------------------*/
void fb_clear(uint16_t color);

/*---------------------------------------------------------------------------
 * export_bmp(filename)
 *   Write a 24-bit uncompressed BMP to the SD card.
 *   Width and height are taken from lcddev.width / lcddev.height so the
 *   export always matches the actual display orientation.
 *   Returns FR_OK on success.
 *---------------------------------------------------------------------------*/
FRESULT export_bmp(const char *filename);

/*---------------------------------------------------------------------------
 * export_csv(filename)
 *   Write "x,y,r,g,b\n" for every pixel in the active display region.
 *   Returns FR_OK on success.
 *---------------------------------------------------------------------------*/
FRESULT export_csv(const char *filename);

#endif /* SCREEN_EXPORT_H */
