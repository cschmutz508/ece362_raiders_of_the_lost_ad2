#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

#include <stdbool.h>
#include <stdint.h>

#define SCOPE_MAX_CAPTURE_SAMPLES 2048u

typedef enum {
    SCOPE_CHANNEL_PROBE1 = 0,
    SCOPE_CHANNEL_PROBE2 = 1
} scope_channel_t;

typedef struct {
    uint16_t raw_min;
    uint16_t raw_max;
    float voltage_min;
    float voltage_max;
    float voltage_avg;
    float voltage_pp;
    float voltage_rms;
} scope_stats_t;

void scope_init(void);
void scope_deinit(void);

uint16_t scope_read_raw(scope_channel_t channel);
float scope_read_voltage(scope_channel_t channel);
float scope_raw_to_voltage(uint16_t raw);

bool scope_capture_raw_dma(
    scope_channel_t channel,
    uint16_t *buffer,
    uint16_t sample_count,
    uint32_t sample_rate_hz
);

bool scope_capture_dual_raw_dma(
    uint16_t *probe1_buffer,
    uint16_t *probe2_buffer,
    uint16_t sample_count_per_probe,
    uint32_t sample_rate_hz
);

bool scope_calculate_stats(
    const uint16_t *buffer,
    uint16_t sample_count,
    scope_stats_t *out_stats
);

#endif