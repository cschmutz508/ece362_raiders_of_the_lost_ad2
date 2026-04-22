
#include <stddef.h>

#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"

#include "oscilloscope.h"

#define SCOPE_PROBE1_GPIO      43u
#define SCOPE_PROBE2_GPIO      44u
#define SCOPE_ADC_REF_VOLTAGE  3.3f
#define SCOPE_ADC_MAX_COUNTS   4095.0f
#define SCOPE_ADC_REF_VOLTAGE  3.3f
#define SCOPE_MIN_SAMPLE_RATE  1u
#define SCOPE_MAX_SAMPLE_RATE  200000u
#define SCOPE_ADC_CLK_HZ       48000000.0f
#define SCOPE_MIN_SAMPLE_RATE  100u
#define SCOPE_MAX_SAMPLE_RATE  500000u

static bool g_scope_initialized;
static int g_scope_dma_chan = -1;
static uint16_t g_dual_capture_temp[SCOPE_MAX_CAPTURE_SAMPLES * 2u];

static uint scope_channel_to_gpio(scope_channel_t channel) {
    switch (channel) {
    }
}

static uint16_t scope_read_gpio_raw(uint gpio) {
    adc_select_input(adc_gpio_to_input(gpio));
    return adc_read();
static uint scope_channel_to_adc_input(scope_channel_t channel) {
    return adc_gpio_to_input(scope_channel_to_gpio(channel));
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

    adc_fifo_setup(
        true,
        true,
        1,
        false,
        false
    );
}

static bool scope_start_dma_capture(uint16_t *buffer, uint32_t transfer_count) {
    dma_channel_config cfg;

    if (g_scope_dma_chan < 0 || buffer == NULL || transfer_count == 0u) {
        return false;
    }

    dma_channel_abort((uint)g_scope_dma_chan);
    dma_channel_acknowledge_irq0((uint)g_scope_dma_chan);

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

        return 0u;
    }

    return scope_read_gpio_raw(scope_channel_to_gpio(channel));
    scope_configure_single_channel(channel);
    return adc_read();
}

float scope_read_voltage(scope_channel_t channel) {
    return scope_raw_to_voltage(scope_read_raw(channel));
}

bool scope_capture_raw(
bool scope_capture_raw_dma(
    scope_channel_t channel,
    uint16_t *buffer,
    uint16_t sample_count,
    uint32_t sample_rate_hz
) {
    uint gpio;
    uint32_t sample_period_us;
    uint64_t next_sample_us;

    if (!g_scope_initialized || buffer == NULL || sample_count == 0u) {
        return false;
    }
        return false;
    }

    gpio = scope_channel_to_gpio(channel);
    sample_period_us = 1000000u / sample_rate_hz;
    if (sample_period_us == 0u) {
        sample_period_us = 1u;
    }
    scope_configure_single_channel(channel);
    scope_prepare_adc_fifo(sample_rate_hz);

    next_sample_us = time_us_64();

    for (uint16_t i = 0; i < sample_count; ++i) {
        next_sample_us += sample_period_us;
        busy_wait_until(from_us_since_boot(next_sample_us));
        buffer[i] = scope_read_gpio_raw(gpio);
    }

    return true;
    return scope_start_dma_capture(buffer, sample_count);
}

bool scope_capture_dual_raw(
bool scope_capture_dual_raw_dma(
    uint16_t *probe1_buffer,
    uint16_t *probe2_buffer,
    uint16_t sample_count,
    uint16_t sample_count_per_probe,
    uint32_t sample_rate_hz
) {
    uint32_t sample_period_us;
    uint64_t next_sample_us;
    uint input1;
    uint input2;
    uint32_t total_transfers;

    if (!g_scope_initialized || probe1_buffer == NULL || probe2_buffer == NULL || sample_count == 0u) {
    if (!g_scope_initialized || probe1_buffer == NULL || probe2_buffer == NULL || sample_count_per_probe == 0u) {
        return false;
    }

    if (sample_count > SCOPE_MAX_CAPTURE_SAMPLES) {
    if (sample_count_per_probe > SCOPE_MAX_CAPTURE_SAMPLES) {
        return false;
    }

    if (sample_rate_hz < SCOPE_MIN_SAMPLE_RATE || sample_rate_hz > SCOPE_MAX_SAMPLE_RATE) {
    if (sample_rate_hz < SCOPE_MIN_SAMPLE_RATE || sample_rate_hz > (SCOPE_MAX_SAMPLE_RATE / 2u)) {
        return false;
    }

    sample_period_us = 1000000u / sample_rate_hz;
    if (sample_period_us == 0u) {
        sample_period_us = 1u;
    }
    input1 = adc_gpio_to_input(SCOPE_PROBE1_GPIO);
    input2 = adc_gpio_to_input(SCOPE_PROBE2_GPIO);
    total_transfers = (uint32_t)sample_count_per_probe * 2u;

    next_sample_us = time_us_64();
    adc_select_input(input1);
    adc_set_round_robin((1u << input1) | (1u << input2));
    scope_prepare_adc_fifo(sample_rate_hz * 2u);

    for (uint16_t i = 0; i < sample_count; ++i) {
        next_sample_us += sample_period_us;
        busy_wait_until(from_us_since_boot(next_sample_us));
    if (!scope_start_dma_capture(g_dual_capture_temp, total_transfers)) {
        adc_set_round_robin(0u);
        return false;
    }

        probe1_buffer[i] = scope_read_gpio_raw(SCOPE_PROBE1_GPIO);
        probe2_buffer[i] = scope_read_gpio_raw(SCOPE_PROBE2_GPIO);
    adc_set_round_robin(0u);

    for (uint16_t i = 0; i < sample_count_per_probe; ++i) {
        probe1_buffer[i] = g_dual_capture_temp[i * 2u];
        probe2_buffer[i] = g_dual_capture_temp[(i * 2u) + 1u];
    }

    return true;