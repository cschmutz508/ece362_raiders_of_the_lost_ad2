#include "display.h"

uint16_t waveform[WIDTH];

void dispInit() {
    LCD_DrawRectangle(10, 20, 310, 210, WHITE);
    LCD_DrawString(20, 213, WHITE, BLACK, "(1) FRA", 12, 1);
    LCD_DrawString(20, 225, WHITE, BLACK, "(2) Measurements", 12, 1);
    LCD_DrawString(160, 213, WHITE, BLACK, "(3) Cursors", 12, 1);
    LCD_DrawString(160, 225, WHITE, BLACK, "(4) Export", 12, 1);
    LCD_DrawString(282, 5, GREEN, BLACK, "Run", 12, 1);
    LCD_DrawRectangle(280, 3, 310, 18, GREEN);
    LCD_DrawString(20, 5, ORANGE, BLACK, "1", 12, 1);
    LCD_DrawString(35, 5, WHITE, BLACK, "2.00 V/", 12, 1);
    LCD_DrawString(90, 5, GRAYBLUE, BLACK, "2", 12, 1);
    LCD_DrawString(105, 5, WHITE, BLACK, " 150mV/", 12, 1);
    LCD_DrawString(180, 5, WHITE, BLACK, "0.0s", 12, 1);
    LCD_DrawString(220, 5, WHITE, BLACK, "500.0uS/", 12, 1);
}

Menu menu_idle = {
    .title = "Idle",
    .items = {},
    .item_count = 0
};

Menu menu_scope = {
    .title = "Scope",
    .items = {
        {'1', "Wavegen",      WAVEGEN,       true},
        {'2', "Measurements", SCOPE_MEASURE, true},
        {'3', "Export",       SCOPE_EXPORT_CONFIRM,  true},
        {'A', "Zafeer",       SCOPE_CHA,     false},
        {'B', "Carman",       SCOPE_CHB,     false}
    },
    .item_count = 5
};

Menu menu_scope_measure = {
    .title = "Measure",
    .items = {
        {'1', "Channel a", SCOPE_MEASURE_CHA, true},
        {'2', "Channel b", SCOPE_MEASURE_CHB, true},
        {'A', "Zafeer",    SCOPE_CHA,         false},
        {'B', "Carman",    SCOPE_CHB,         false}
    },
    .item_count = 4
};

Menu menu_scope_measure_cha = {
    .title = "Measure CHA",
    .items = {
        {'1', "Frequency", SCOPE_MEASURE_CHA, true},
        {'2', "Vp-p",      SCOPE_MEASURE_CHA, true},
        {'3', "Average",   SCOPE_MEASURE_CHA, true},
        {'A', "Zafeer",    SCOPE_CHA,         false},
        {'B', "Carman",    SCOPE_CHB,         false}
    },
    .item_count = 5
};

Menu menu_scope_measure_chb = {
    .title = "Measure CHB",
    .items = {
        {'1', "Frequency", SCOPE_MEASURE_CHB, true},
        {'2', "Vp-p",      SCOPE_MEASURE_CHB, true},
        {'3', "Average",   SCOPE_MEASURE_CHB, true},
        {'A', "Zafeer",    SCOPE_CHA,         false},
        {'B', "Carman",    SCOPE_CHB,         false}
    },
    .item_count = 5
};

Menu menu_scope_export = {
    .title = "Export",
    .items = {
        {'I', "Wait", SCOPE, true},
        {'A', "Zafeer",              SCOPE_CHA, false},
        {'B', "Carman",              SCOPE_CHB, false}
    },
    .item_count = 3
};

Menu menu_scope_export_confirm = {
    .title = "Export Confirm",
    .items = {
        {'1', "CSV", SCOPE_EXPORT, true},
        {'2', "Image", SCOPE_EXPORT, true},
        {'A', "Rapha is a good goy", SCOPE_CHA, false},
        {'B', "Carman",              SCOPE_CHB, false}
    },
    .item_count = 4
};

Menu menu_scope_cha = {
    .title = "Channel A",
    .items = {
        {'1', "Divisions", SCOPE_CHA, true},
        {'2', "Offset",    SCOPE_CHA, true},
        {'3', "Enable",    SCOPE_CHA, true},
        {'B', "Carman",    SCOPE_CHB, false}
    },
    .item_count = 4
};

Menu menu_scope_chb = {
    .title = "Channel B",
    .items = {
        {'1', "Divisions", SCOPE_CHB, true},
        {'2', "Offset",    SCOPE_CHB, true},
        {'3', "Enable",    SCOPE_CHB, true},
        {'A', "Zafeer",    SCOPE_CHA, false}
    },
    .item_count = 4   // was 3 — item_count should match actual items
};

Menu menu_wavegen = {
    .title = "Wavegen",
    .items = {
        {'1', "Oscilloscope", SCOPE, true}
    },
    .item_count = 1
};

Menu *menu_table[] = {
    [IDLE]              = &menu_idle,
    [SCOPE]             = &menu_scope,
    [SCOPE_MEASURE]     = &menu_scope_measure,
    [SCOPE_MEASURE_CHA] = &menu_scope_measure_cha,
    [SCOPE_MEASURE_CHB] = &menu_scope_measure_chb,
    [SCOPE_EXPORT_CONFIRM] = &menu_scope_export_confirm,
    [SCOPE_EXPORT]      = &menu_scope_export,
    [SCOPE_CHA]         = &menu_scope_cha,
    [SCOPE_CHB]         = &menu_scope_chb,
    [WAVEGEN]           = &menu_wavegen
};

void generateTestWaveform() {
    for (int i = 0; i < WIDTH; i++) {
        waveform[i] = 2048 + (uint16_t)(800 * sinf(2.0f * M_PI * i / WIDTH));
    }
}

void dispFunc(uint16_t *data, float voltage_offset, float voltage_div, uint16_t color) {
    static int16_t prev_x = 0;
    static int16_t prev_y = 0;
    static uint8_t first = 1;

    for (int i = 0; i < WIDTH; i++) {
        int16_t x = X0 + i;

        float v = ((float)data[i] - voltage_offset) / voltage_div;
        int16_t y = 120 - (int16_t)(v * 50);

        if (y < Y0)       y = Y0;
        if (y > Y0+HEIGHT) y = Y0+HEIGHT;

        if (!first) {
            LCD_DrawLine(prev_x, prev_y, x, y, color);
        } else {
            first = 0;
        }

        prev_x = x;
        prev_y = y;
    }

    first = 1;
}