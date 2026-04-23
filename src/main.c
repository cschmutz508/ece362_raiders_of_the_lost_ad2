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

    uint8_t ch1_en;
    uint8_t ch2_en;

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
    .run_mode = 0,
    .ch1_en = 0,
    .ch2_en = 0,
    .menu_mode = 1,
    .redraw_ui = 1,
    .redraw_waveform = 1
};

typedef struct {
    float freq_ch1;
    float freq_ch2;
    uint8_t ch1_on;
    uint8_t ch2_on;
} WavegenState;

WavegenState wg = {
    .freq_ch1 = 1000.0f,
    .freq_ch2 = 1000.0f,
    .ch1_on = 0,
    .ch2_on = 0
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

void draw_wavegen_screen() {
    LCD_DrawFillRectangle(0, 20, 320, 210, BLACK);
    char buf[64];
    LCD_DrawString(20, 30, WHITE, BLACK, "== Wavegen ==", 12, 1);
    snprintf(buf, sizeof(buf), "CH1: %s  %.1f Hz", wg.ch1_on ? "ON " : "OFF", wg.freq_ch1);
    LCD_DrawString(20, 50, wg.ch1_on ? GREEN : RED, BLACK, buf, 12, 1);
    snprintf(buf, sizeof(buf), "CH2: %s  %.1f Hz", wg.ch2_on ? "ON " : "OFF", wg.freq_ch2);
    LCD_DrawString(20, 70, wg.ch2_on ? GREEN : RED, BLACK, buf, 12, 1);
    LCD_DrawString(20, 100, GRAYBLUE, BLACK, "(2) Toggle CH1", 12, 1);
    LCD_DrawString(20, 115, GRAYBLUE, BLACK, "(3) Toggle CH2", 12, 1);
    LCD_DrawString(20, 130, GRAYBLUE, BLACK, "(4) CH1 freq up x10", 12, 1);
    LCD_DrawString(20, 145, GRAYBLUE, BLACK, "(5) CH1 freq dn x10", 12, 1);
    LCD_DrawString(20, 160, GRAYBLUE, BLACK, "(6) CH2 freq up x10", 12, 1);
    LCD_DrawString(20, 175, GRAYBLUE, BLACK, "(7) CH2 freq dn x10", 12, 1);
}

void handle_wavegen_key(char key) {
    switch (key) {
        case '2':
            wg.ch1_on ^= 1;
            if (wg.ch1_on) wavegen_set_sine_mode(0, wg.freq_ch1);
            else            wavegen_stop(0);
            break;
        case '3':
            wg.ch2_on ^= 1;
            if (wg.ch2_on) wavegen_set_sine_mode(1, wg.freq_ch2);
            else            wavegen_stop(1);
            break;
        case '4':
            wg.freq_ch1 = fminf(wg.freq_ch1 * 10.0f, 20000.0f);
            if (wg.ch1_on) wavegen_set_sine_mode(0, wg.freq_ch1);
            break;
        case '5':
            wg.freq_ch1 = fmaxf(wg.freq_ch1 / 10.0f, 1.0f);
            if (wg.ch1_on) wavegen_set_sine_mode(0, wg.freq_ch1);
            break;
        case '6':
            wg.freq_ch2 = fminf(wg.freq_ch2 * 10.0f, 20000.0f);
            if (wg.ch2_on) wavegen_set_sine_mode(1, wg.freq_ch2);
            break;
        case '7':
            wg.freq_ch2 = fmaxf(wg.freq_ch2 / 10.0f, 1.0f);
            if (wg.ch2_on) wavegen_set_sine_mode(1, wg.freq_ch2);
            break;
        default:
            screen_state = SCOPE;
            LCD_Clear(0x0000);
            dispInit();
            draw_menu();
            return;
    }
    draw_wavegen_screen();
}
void handle_key(char key)
{
    if (screen_state == WAVEGEN) {
        handle_wavegen_key(key);
        return;
    }
    Menu *menu = menu_table[screen_state];

    if (screen_state == IDLE) {
        screen_state = SCOPE;
        return;
    }

    if (key == '3' && screen_state == SCOPE_CHA) {
        scope.ch1_en ^= 1;
        if (scope.ch1_en == 0) {
            LCD_DrawFillRectangle(X0+1, 90, X0+WIDTH, 103, BLACK);
        }
    }
    if (key == '3' && screen_state == SCOPE_CHB) {
        scope.ch2_en ^= 1;
        if (scope.ch2_en == 0) {
            LCD_DrawFillRectangle(X0+1, 105, X0+WIDTH, 117, BLACK);
        }
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
    wavegen_init();
    scope_init();

    LCD_Setup();
    LCD_Clear(0x0000);

    generateTestWaveform();
    dispFunc(waveform, 2400.0f, 800.0f, ORANGE);
    dispFunc(waveform, 1600.0f, 1000.0f, GRAYBLUE);

    dispInit();
    draw_menu();

    char *mount_argv[] = {"mount"};
    mount(1, mount_argv);

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

        ScreenState old_state = screen_state;  // save before handle_key

        if (found != '\0' && prev_key == '\0') {
            printf("key: %c\n", found);
            handle_key(found);
            if (screen_state == WAVEGEN && old_state != WAVEGEN) {
                LCD_DrawFillRectangle(0, 0, 320, 240, BLACK);
                draw_wavegen_screen();
            } else if (screen_state != WAVEGEN) {
                draw_menu();
            }
        }
        prev_key = found;

        // rising edge: key newly pressed (wasn't pressed before, is now)
        if (found != '\0' && prev_key == '\0') {
            printf("key: %c\n", found);
            handle_key(found);
            draw_menu();
        }

        prev_key = found;

        if (screen_state == SCOPE_EXPORT) {
            FRESULT result_bmp = export_bmp("SCREEN.BMP");
            FRESULT result_csv = export_csv("SCREEN.CSV");
            screen_state = SCOPE;
        }

        if (screen_state != WAVEGEN && scope.run_mode) {
            char buf[48];
            if (scope.ch1_en) {
                float v1 = scope_read_voltage(SCOPE_CHANNEL_PROBE1);
                snprintf(buf, sizeof(buf), "CH1: %.3f V", v1);
                LCD_DrawFillRectangle(X0, 90, X0+WIDTH, 103, BLACK);
                LCD_DrawString(X0 + 15, 90, ORANGE, BLACK, buf, 12, 1);
            }
            if (scope.ch2_en) {
                float v2 = scope_read_voltage(SCOPE_CHANNEL_PROBE2);
                snprintf(buf, sizeof(buf), "CH2: %.3f V", v2);
                LCD_DrawFillRectangle(X0, 105, X0+WIDTH, 118, BLACK);
                LCD_DrawString(X0 + 15, 105, GRAYBLUE, BLACK, buf, 12, 1);
            }
        }
    }
}