/*===========================================================================
 * sdcard_init.c
 *
 * SPI0 initialisation for the SD card on your custom PCB.
 *
 * Pinout (from your hardware):
 *   GPIO2  -- SPI0 SCK
 *   GPIO3  -- SPI0 TX  (MOSI)
 *   GPIO4  -- SPI0 RX  (MISO)
 *   GPIO5  -- SPI0 CSn  (controlled manually as GPIO, not SPI hardware CS)
 *
 * These are the five weak functions declared in diskio.c.
 * Defining them here overrides the weak stubs.
 *
 * This file should be compiled as part of your main project alongside
 * diskio.c, sdcard.c, ff.c, and screen_export.c.
 *===========================================================================*/

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <stdio.h>

/* ---- Pin definitions (must match your PCB) ---- */
#define SD_SCK   2
#define SD_MOSI  3
#define SD_MISO  4
#define SD_CS    5

/* ---- SPI peripheral used for the SD card ---- */
/* diskio.c declares:  spi_inst_t *sd = spi0;
   GPIO2/3/4/5 are all SPI0 pins on the RP2350 — consistent. */
#define SD_SPI   spi0

/*---------------------------------------------------------------------------
 * init_spi_sdcard()
 *   Configure the SPI0 peripheral and GPIO pins for SD card communication.
 *   Initial clock is 400 kHz — required by the SD card SPI init sequence.
 *   CS is driven manually (SIO), not by the SPI hardware.
 *---------------------------------------------------------------------------*/
void init_spi_sdcard(void)
{
    /* SCK, MOSI, MISO → SPI function */
    gpio_set_function(SD_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SD_MISO, GPIO_FUNC_SPI);

    /* CS → plain GPIO, manually controlled */
    gpio_set_function(SD_CS, GPIO_FUNC_SIO);
    gpio_set_dir(SD_CS, GPIO_OUT);
    gpio_put(SD_CS, 1);   /* deassert (active-low) */

    /*
     * Internal pull-up on MISO.
     * The MSP2202 board does not have an external pull-up on MISO.
     * The SD card spec recommends 10 kΩ external, but the RP2350's
     * internal ~50 kΩ pull-up is sufficient to get started.
     * If you see unreliable reads, add an external 10 kΩ resistor
     * from GPIO4 to 3.3 V.
     */
    gpio_pull_up(SD_MISO);

    /* 400 kHz, 8-bit, CPOL=0, CPHA=0 — SD card SPI init requirement */
    spi_init(SD_SPI, 400 * 1000);
    spi_set_format(SD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

/*---------------------------------------------------------------------------
 * disable_sdcard()
 *   Deassert CS, send a dummy 0xFF byte to give the card clock cycles to
 *   finish up, then release (force high) the MOSI line.
 *   Some SD card specs require MOSI to idle high between transactions.
 *---------------------------------------------------------------------------*/
void disable_sdcard(void)
{
    gpio_put(SD_CS, 1);   /* CS high = deselect */

    /* Give the card 8 clock cycles with MOSI high to complete any
       internal operation it might be finishing. */
    uint8_t dummy = 0xFF;
    spi_write_blocking(SD_SPI, &dummy, 1);

    /*
     * "Release" MOSI: temporarily make it a plain GPIO driven high.
     * This prevents noise on the MOSI line from being misinterpreted
     * as a command start bit while the card is idle.
     */
    gpio_set_function(SD_MOSI, GPIO_FUNC_SIO);
    gpio_set_dir(SD_MOSI, GPIO_OUT);
    gpio_put(SD_MOSI, 1);
}

/*---------------------------------------------------------------------------
 * enable_sdcard()
 *   Reclaim MOSI as an SPI pin, then assert CS low to begin a transaction.
 *---------------------------------------------------------------------------*/
void enable_sdcard(void)
{
    /* Restore MOSI to SPI function before asserting CS */
    gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);

    gpio_put(SD_CS, 0);   /* CS low = select */
}

/*---------------------------------------------------------------------------
 * sdcard_io_high_speed()
 *   Switch SPI0 to 12 MHz after the SD card has been initialised.
 *   The SD card can typically handle up to 25 MHz in SPI mode; 12 MHz is
 *   a safe default.  You can try 24 MHz if your wiring is short and clean.
 *---------------------------------------------------------------------------*/
void sdcard_io_high_speed(void)
{
    spi_set_baudrate(SD_SPI, 12 * 1000 * 1000);
}

/*---------------------------------------------------------------------------
 * init_sdcard_io()
 *   Top-level init called by diskio.c → disk_initialize().
 *   Calls init_spi_sdcard() then disable_sdcard() to leave everything in
 *   a clean idle state before the SD card init sequence begins.
 *---------------------------------------------------------------------------*/
void init_sdcard_io(void)
{
    init_spi_sdcard();
    disable_sdcard();
}
