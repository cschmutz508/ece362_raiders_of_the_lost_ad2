#include <math.h>
#include <stddef.h>

#include "hardware/adc.h"
#include "hardware/dma.h"
#include "pico/stdlib.h"

#include "oscilloscope.h"

#define SCOPE_PROBE1_GPIO 43u
#define SCOPE_PROBE2_GPIO 44u
#define SCOPE_PROBE1_ADC_INPUT 3u
#define SCOPE_PROBE2_ADC_INPUT 4u
#define SCOPE_ADC_REF_VOLTAGE 3.3f
#define SCOPE_ADC_MAX_COUNTS 4095.0f
#define SCOPE_ADC_CLK_HZ 48000000.0f
#define SCOPE_MIN_SAMPLE_RATE 100u
#define SCOPE_MAX_SAMPLE_RATE 500000u

static bool g_scope_initialized;
static int g_scope_dma_chan = -1;
static uint16_t g_dual_capture_temp[SCOPE_MAX_CAPTURE_SAMPLES * 2u];

static uint scope_channel_to_gpio(scope_channel_t channel) {
    switch (channel) {
        case SCOPE_CHANNEL_PROBE1:
            return SCOPE_PROBE1_GPIO;
        case SCOPE_CHANNEL_PROBE2:
            return SCOPE_PROBE2_GPIO;
        default:
            return SCOPE_PROBE1_GPIO;
    }
}

static uint scope_channel_to_adc_input(scope_channel_t channel) {
    switch (channel) {
        case SCOPE_CHANNEL_PROBE1:
            return SCOPE_PROBE1_ADC_INPUT;
        case SCOPE_CHANNEL_PROBE2:
            return SCOPE_PROBE2_ADC_INPUT;
        default:
            return SCOPE_PROBE1_ADC_INPUT;
    }
}

static void scope_configure_single_channel(scope_channel_t channel) {
    adc_select_input(scope_channel_to_adc_input(channel));
    adc_set_round_robin(0u);
}

static void scope_prepare_adc_fifo(uint32_t sample_rate_hz) {
    float clkdiv = (SCOPE_ADC_CLK_HZ / (float)sample_rate_hz) - 1.0f;

    if (clkdiv < 0.0f) {
        clkdiv = 0.0f;
    }

    adc_run(false);
    adc_fifo_drain();
    adc_set_clkdiv(clkdiv);
    adc_fifo_setup(true, true, 1, false, false);
}

static bool scope_start_dma_capture(uint16_t *buffer, uint32_t transfer_count) {
    dma_channel_config cfg;

    if (g_scope_dma_chan < 0 || buffer == NULL || transfer_count == 0u) {
        return false;
    }

    dma_channel_abort((uint)g_scope_dma_chan);

    cfg = dma_channel_get_default_config((uint)g_scope_dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    dma_channel_configure(
        (uint)g_scope_dma_chan,
        &cfg,
        buffer,
        &adc_hw->fifo,
        transfer_count,
        false
    );

    dma_start_channel_mask(1u << (uint)g_scope_dma_chan);
    adc_run(true);
    dma_channel_wait_for_finish_blocking((uint)g_scope_dma_chan);
    adc_run(false);
    adc_fifo_drain();

    return true;
}

void scope_init(void) {
    if (g_scope_initialized) {
        return;
    }

    adc_init();
    adc_gpio_init(SCOPE_PROBE1_GPIO);
    adc_gpio_init(SCOPE_PROBE2_GPIO);

    if (g_scope_dma_chan < 0) {
        g_scope_dma_chan = dma_claim_unused_channel(true);
    }

    g_scope_initialized = true;
}

void scope_deinit(void) {
    if (!g_scope_initialized) {
        return;
    }

    adc_run(false);
    adc_fifo_drain();

    if (g_scope_dma_chan >= 0) {
        dma_channel_abort((uint)g_scope_dma_chan);
        dma_channel_unclaim((uint)g_scope_dma_chan);
        g_scope_dma_chan = -1;
    }

    g_scope_initialized = false;
}

float scope_raw_to_voltage(uint16_t raw) {
    return ((float)raw / SCOPE_ADC_MAX_COUNTS) * SCOPE_ADC_REF_VOLTAGE;
}

uint16_t scope_read_raw(scope_channel_t channel) {
    if (!g_scope_initialized) {
        return 0u;
    }

    scope_configure_single_channel(channel);
    return adc_read();
}

float scope_read_voltage(scope_channel_t channel) {
    return scope_raw_to_voltage(scope_read_raw(channel));
}

bool scope_capture_raw_dma(
    scope_channel_t channel,
    uint16_t *buffer,
    uint16_t sample_count,
    uint32_t sample_rate_hz
) {
    if (!g_scope_initialized || buffer == NULL || sample_count == 0u) {
        return false;
    }

    if (sample_count > SCOPE_MAX_CAPTURE_SAMPLES) {
        return false;
    }

    if (sample_rate_hz < SCOPE_MIN_SAMPLE_RATE || sample_rate_hz > SCOPE_MAX_SAMPLE_RATE) {
        return false;
    }

    scope_configure_single_channel(channel);
    scope_prepare_adc_fifo(sample_rate_hz);

    return scope_start_dma_capture(buffer, sample_count);
}

bool scope_capture_dual_raw_dma(
    uint16_t *probe1_buffer,
    uint16_t *probe2_buffer,
    uint16_t sample_count_per_probe,
    uint32_t sample_rate_hz
) {
    uint probe1_input;
    uint probe2_input;
    uint32_t total_transfers;

    if (!g_scope_initialized || probe1_buffer == NULL || probe2_buffer == NULL || sample_count_per_probe == 0u) {
        return false;
    }

    if (sample_count_per_probe > SCOPE_MAX_CAPTURE_SAMPLES) {
        return false;
    }

    if (sample_rate_hz < SCOPE_MIN_SAMPLE_RATE || sample_rate_hz > (SCOPE_MAX_SAMPLE_RATE / 2u)) {
        return false;
    }

    probe1_input = SCOPE_PROBE1_ADC_INPUT;
    probe2_input = SCOPE_PROBE2_ADC_INPUT;
    total_transfers = (uint32_t)sample_count_per_probe * 2u;

    adc_select_input(probe1_input);
    adc_set_round_robin((1u << probe1_input) | (1u << probe2_input));
    scope_prepare_adc_fifo(sample_rate_hz * 2u);

    if (!scope_start_dma_capture(g_dual_capture_temp, total_transfers)) {
        adc_set_round_robin(0u);
        return false;
    }

    adc_set_round_robin(0u);

    for (uint16_t i = 0; i < sample_count_per_probe; ++i) {
        probe1_buffer[i] = g_dual_capture_temp[i * 2u];
        probe2_buffer[i] = g_dual_capture_temp[(i * 2u) + 1u];
    }

    return true;
}

bool scope_calculate_stats(
    const uint16_t *buffer,
    uint16_t sample_count,
    scope_stats_t *out_stats
) {
    uint16_t raw_min;
    uint16_t raw_max;
    double voltage_sum = 0.0;
    double voltage_square_sum = 0.0;

    if (buffer == NULL || out_stats == NULL || sample_count == 0u) {
        return false;
    }

    raw_min = buffer[0];
    raw_max = buffer[0];

    for (uint16_t i = 0; i < sample_count; ++i) {
        float voltage = scope_raw_to_voltage(buffer[i]);

        if (buffer[i] < raw_min) {
            raw_min = buffer[i];
        }
        if (buffer[i] > raw_max) {
            raw_max = buffer[i];
        }

        voltage_sum += voltage;
        voltage_square_sum += (double)voltage * (double)voltage;
    }

    out_stats->raw_min = raw_min;
    out_stats->raw_max = raw_max;
    out_stats->voltage_min = scope_raw_to_voltage(raw_min);
    out_stats->voltage_max = scope_raw_to_voltage(raw_max);
    out_stats->voltage_avg = (float)(voltage_sum / (double)sample_count);
    out_stats->voltage_pp = out_stats->voltage_max - out_stats->voltage_min;
    out_stats->voltage_rms = (float)sqrt(voltage_square_sum / (double)sample_count);

    return true;
}
