#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "lcd.h"
//#include "fra.h"
#include "oscilloscope.h"
#include "wavegen.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "screen_export.h"
#include "sdcard.h"
#include "ff.h"
#include "display.h"

//////////////////////////////////////////////////////////////////////////

// const char keymap[16] = "DCBA#9630852*741";
const char keymap[16] = "147*2580369#ABCD";
volatile char key_pressed = '\0';
volatile int col = 0;
volatile int active_col = 0;

ScreenState screen_state;

typedef struct {
    float volts_per_div_ch1;
    float volts_per_div_ch2;
    float time_per_div;

    float ch1_offset;
    float ch2_offset;

    uint8_t run_mode;
    uint8_t menu_mode;

    uint8_t redraw_ui;
    uint8_t redraw_waveform;
} ScopeState;

ScopeState scope = {
    .volts_per_div_ch1 = 2.0f,
    .volts_per_div_ch2 = 0.150f,
    .time_per_div = 500e-6f,
    .ch1_offset = 0.0f,
    .ch2_offset = 0.0f,
    .run_mode = 1,
    .menu_mode = 1,
    .redraw_ui = 1,
    .redraw_waveform = 1
};

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
#define ROW4 33
#define ROW3 34
#define ROW2 35
#define ROW1 36
#define COL4 37
#define COL3 38
#define COL2 39
#define COL1 40

// probes
const int OSC_PROBE1 = 43;
const int OSC_PROBE2 = 44;

void run_scope_uart_test(void);


//////////////////////////////////////////////////////////////////////////

void init_spi_lcd() {
    gpio_set_function(PIN_CS, GPIO_FUNC_SIO);
    gpio_set_function(PIN_DC, GPIO_FUNC_SIO);
    gpio_set_function(PIN_nRESET, GPIO_FUNC_SIO);

    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_nRESET, GPIO_OUT);

    gpio_put(PIN_CS, 1);
    gpio_put(PIN_DC, 0);
    gpio_put(PIN_nRESET, 1);

    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SDI, GPIO_FUNC_SPI);
    spi_init(spi1, 12 * 1000000);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void init_keypad() {
    for (int i = 0; i < 4; i++) {
        gpio_init(ROW1 - i);
        gpio_set_dir(ROW1 - i, GPIO_IN);
    }
    for (int i = 0; i < 4; i++) {
        gpio_init(COL1 - i);
        gpio_set_dir(COL1 - i, GPIO_OUT);
        gpio_put(COL1 - i, 0);
    }
}

void handle_key(char key)
{
    Menu *menu = menu_table[screen_state];

    if (screen_state == IDLE) {
        screen_state = SCOPE;
        return;
    }

    for (int i = 0; i < menu->item_count; i++)
    {
        if (menu->items[i].key == key) {
            screen_state = menu->items[i].next_state;
            return;
        }
    }

    // global controls
    if (key == '#')
        screen_state = SCOPE;

    if (key == 'C') {
        scope.run_mode ^= 1u;
        if (scope.run_mode) {
            LCD_DrawFillRectangle(280, 3, 310, 18, BLACK);
            LCD_DrawRectangle(280, 3, 310, 18, RED);
            LCD_DrawString(282, 5, RED, BLACK, "Stop", 12, 1);
        } else {
            LCD_DrawFillRectangle(280, 3, 310, 18, BLACK);
            LCD_DrawRectangle(280, 3, 310, 18, GREEN);
            LCD_DrawString(282, 5, GREEN, BLACK, "Run", 12, 1);
        }
    }
}

void draw_menu(void)
{
    Menu *menu = menu_table[screen_state];

    // clear entire bottom bar
    LCD_DrawFillRectangle(0, 213, 320, 240, BLACK);

    int drawn = 0;

    for (int i = 0; i < menu->item_count; i++)
    {
        if (!menu->items[i].listed)
            continue;

        int x = (drawn < 2) ? 20 : 160;
        int y = (drawn % 2 == 0) ? 213 : 225;

        char buf[32];
        snprintf(buf, sizeof(buf), "(%c) %s", menu->items[i].key, menu->items[i].label);

        // clear just this slot before drawing
        LCD_DrawFillRectangle(x, y, x + 138, y + 12, BLACK);
        LCD_DrawString(x, y, WHITE, BLACK, buf, 12, 1);

        drawn++;
        if (drawn == 4) break;
    }
}

//////////////////////////////////////////////////////////////////////////

int main() {
    stdio_init_all();

    init_keypad();
    init_spi_lcd();

    LCD_Setup();
    LCD_Clear(0x0000);

    generateTestWaveform();
    dispFunc(waveform, 2048.0f, 800.0f, ORANGE);

    dispInit();
    draw_menu();

    char prev_key = '\0';

    while (1) {
        char found = '\0';
        for (int i = 0; i < 4; i++) {
            gpio_put(COL1 - i, true);
            sleep_us(100);  // short settle
            for (int j = 0; j < 4; j++) {
                if (gpio_get(ROW1 - j)) {
                    found = keymap[i*4 + j];
                }
            }
            gpio_put(COL1 - i, false);
        }

        // rising edge: key newly pressed (wasn't pressed before, is now)
        if (found != '\0' && prev_key == '\0') {
            printf("key: %c\n", found);
            handle_key(found);
            draw_menu();
        }

        prev_key = found;
    }
}