#pragma once

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "lcd.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define X0 10
#define Y0 20
#define WIDTH 299
#define HEIGHT 190   // 210 - 20

#define MAX_MENU_ITEMS 10

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum {
    IDLE,
    SCOPE,
    SCOPE_MEASURE,
    SCOPE_MEASURE_CHA,
    SCOPE_MEASURE_CHB,
    SCOPE_EXPORT,
    SCOPE_EXPORT_CONFIRM,
    SCOPE_CHA,
    SCOPE_CHB,
    WAVEGEN
} ScreenState;

typedef struct {
    char key;
    const char *label;
    ScreenState next_state;
    bool listed;
} MenuItem;

typedef struct {
    const char *title;
    MenuItem items[MAX_MENU_ITEMS];
    int item_count;
} Menu;

extern Menu menu_idle;
extern Menu menu_scope;
extern Menu menu_scope_measure;
extern Menu menu_scope_measure_cha;
extern Menu menu_scope_measure_chb;
extern Menu menu_scope_export_confirm;
extern Menu menu_scope_export;
extern Menu menu_scope_cha;
extern Menu menu_scope_chb;
extern Menu menu_wavegen;
extern Menu *menu_table[];

extern uint16_t waveform[WIDTH];

void dispInit(void);
void generateTestWaveform(void);
void dispFunc(uint16_t *data, float voltage_offset, float voltage_div, uint16_t color);