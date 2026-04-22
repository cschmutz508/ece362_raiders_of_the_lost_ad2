#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "lcd.h"
#include "fra.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////

// sd card --> spi0
const int SPI_SD_SCK = 2;
const int SPI_SD_TX = 3;
const int SPI_SD_RX = 4;
const int SPI_SD_CSn = 5;

const int GENERIC0 = 7;
const int GENERIC1 = 8;
const int GENERIC2 = 9;
const int GENERIC3 = 10;

// tft display --> spi1
#define PIN_nRESET 11
#define PIN_DC     12
#define PIN_CS     13
#define PIN_SCK    14
#define PIN_SDI    15

const int DEBUG_LED1 = 17;
const int DEBUG_LED2 = 18;

const int GENERIC5 = 19;

// wavegens
const int PWM_WAVEGEN1 = 28;
const int PWM_WAVEGEN2 = 30;

// keypad
const int ROW4 = 33;
const int ROW3 = 34;
const int ROW2 = 35;
const int ROW1 = 36;
const int COL3 = 37;
const int COL2 = 38;
const int COL1 = 39;
const int COL0 = 40;

// probes
const int OSC_PROBE1 = 43;
const int OSC_PROBE2 = 44;

//////////////////////////////////////////////////////////////////////////

void init_spi_lcd() {
    gpio_set_function(PIN_CS, GPIO_FUNC_SIO);
    gpio_set_function(PIN_DC, GPIO_FUNC_SIO);
    gpio_set_function(PIN_nRESET, GPIO_FUNC_SIO);

    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_nRESET, GPIO_OUT);

    gpio_put(PIN_CS, 1); // CS high
    gpio_put(PIN_DC, 0); // DC low
    gpio_put(PIN_nRESET, 1); // nRESET high

    // initialize SPI1 with 48 MHz clock
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SDI, GPIO_FUNC_SPI);
    spi_init(spi1, 12 * 1000000);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

//////////////////////////////////////////////////////////////////////////

int main() {
    stdio_init_all();

    init_spi_lcd();

    LCD_Setup();
    LCD_Clear(0x0000); // Clear the screen to black

    LCD_DrawString(20, 20, WHITE, BLACK, "Hello World", 12, 1);

    for(;;);
}


