#include <math.h>
#include <stddef.h>

#include "hardware/adc.h"
#include "hardware/dma.h"
#include "pico/stdlib.h"

#include "oscilloscope.h"

#define SCOPE_PROBE1_GPIO 43u
#define SCOPE_PROBE2_GPIO 44u
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
