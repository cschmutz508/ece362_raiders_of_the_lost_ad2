// #include <math.h>
// #include <stddef.h>
// #include <string.h>

// #include "fra.h"
// #include "hardware/adc.h"
// #include "hardware/clocks.h"
// #include "hardware/gpio.h"
// #include "hardware/pwm.h"
// #include "pico/stdlib.h"

// #ifndef FRA_PI
// #define FRA_PI 3.14159265358979323846f
// #endif

// #define FRA_PWM_PIN_A 28u
// #define FRA_PWM_PIN_B 30u
// #define FRA_PROBE_1_GPIO 43u
// #define FRA_PROBE_2_GPIO 44u

// #define FRA_PWM_WRAP 1023u
// #define FRA_PWM_CENTER (FRA_PWM_WRAP / 2u)
// #define FRA_DEFAULT_PWM_CARRIER_HZ 200000u
// #define FRA_DEFAULT_BIAS_MV 1650u
// #define FRA_DEFAULT_AMPLITUDE_MV 600u
// #define FRA_ADC_MAX_COUNTS 4095.0f
// #define FRA_ADC_REF_VOLTAGE 3.3f
// #define FRA_MIN_SAMPLES_PER_CYCLE 24u
// #define FRA_MAX_SAMPLES_PER_CYCLE 192u
// #define FRA_MIN_SETTLE_CYCLES 4u
// #define FRA_MIN_MEASURE_CYCLES 8u
// #define FRA_MAX_SWEEP_POINTS 256u
// #define FRA_MIN_OUTPUT_FREQ_HZ 1.0f
// #define FRA_EPSILON 1.0e-9f

// typedef struct {
//     float re;
//     float im;
//     float dc;
//     float amplitude;
//     float phase_rad;
// } fra_lockin_result_t;

// static uint g_pwm_slice_a;
// static uint g_pwm_slice_b;
// static bool g_fra_is_initialized;

// __attribute__((weak)) void fra_on_sweep_begin(const fra_sweep_cfg_t *cfg) {
//     (void)cfg;
// }

// __attribute__((weak)) void fra_on_point_ready(const fra_point_t *point, uint16_t index, uint16_t total) {
//     (void)point;
//     (void)index;
//     (void)total;
// }

// __attribute__((weak)) void fra_on_sweep_complete(const fra_point_t *points, uint16_t count) {
//     (void)points;
//     (void)count;
// }

// static float fra_clampf(float value, float lo, float hi) {
//     if (value < lo) {
//         return lo;
//     }
//     if (value > hi) {
//         return hi;
//     }
//     return value;
// }

// static uint16_t fra_voltage_to_pwm_level(float voltage_v) {
//     float clamped = fra_clampf(voltage_v, 0.0f, FRA_ADC_REF_VOLTAGE);
//     float scaled = (clamped / FRA_ADC_REF_VOLTAGE) * (float)FRA_PWM_WRAP;
//     if (scaled < 0.0f) {
//         scaled = 0.0f;
//     }
//     if (scaled > (float)FRA_PWM_WRAP) {
//         scaled = (float)FRA_PWM_WRAP;
//     }
//     return (uint16_t)(scaled + 0.5f);
// }

// static float fra_adc_counts_to_voltage(uint16_t counts) {
//     return ((float)counts / FRA_ADC_MAX_COUNTS) * FRA_ADC_REF_VOLTAGE;
// }

// static uint fra_samples_per_cycle_for(float frequency_hz, uint16_t requested) {
//     uint samples = requested;
//     float max_reasonable = 80000.0f / frequency_hz;

//     if (samples < FRA_MIN_SAMPLES_PER_CYCLE) {
//         samples = FRA_MIN_SAMPLES_PER_CYCLE;
//     }
//     if (samples > FRA_MAX_SAMPLES_PER_CYCLE) {
//         samples = FRA_MAX_SAMPLES_PER_CYCLE;
//     }

//     if (max_reasonable >= (float)FRA_MIN_SAMPLES_PER_CYCLE && (float)samples > max_reasonable) {
//         samples = (uint)max_reasonable;
//     }
//     if (samples < FRA_MIN_SAMPLES_PER_CYCLE) {
//         samples = FRA_MIN_SAMPLES_PER_CYCLE;
//     }

//     return samples;
// }

// static void fra_pwm_init_pin(uint gpio, uint *slice_out, uint32_t carrier_hz) {
//     gpio_set_function(gpio, GPIO_FUNC_PWM);

//     uint slice = pwm_gpio_to_slice_num(gpio);
//     uint channel = pwm_gpio_to_channel(gpio);
//     float divider = (float)clock_get_hz(clk_sys) / ((float)carrier_hz * (float)(FRA_PWM_WRAP + 1u));

//     if (divider < 1.0f) {
//         divider = 1.0f;
//     }
//     if (divider > 255.0f) {
//         divider = 255.0f;
//     }

//     pwm_set_wrap(slice, FRA_PWM_WRAP);
//     pwm_set_clkdiv(slice, divider);
//     pwm_set_chan_level(slice, channel, FRA_PWM_CENTER);
//     pwm_set_enabled(slice, true);

//     if (slice_out != NULL) {
//         *slice_out = slice;
//     }
// }

// static inline void fra_pwm_write(uint gpio, uint16_t level) {
//     pwm_set_gpio_level(gpio, level);
// }

// static uint16_t fra_adc_read_gpio(uint gpio) {
//     adc_select_input(adc_gpio_to_input(gpio));
//     return adc_read();
// }

// static void fra_lockin_reset(fra_lockin_result_t *state) {
//     memset(state, 0, sizeof(*state));
// }

// static void fra_lockin_finalize(fra_lockin_result_t *state, uint32_t sample_count) {
//     float n = (float)sample_count;

//     state->dc /= n;
//     state->re *= (2.0f / n);
//     state->im *= (2.0f / n);
//     state->amplitude = sqrtf((state->re * state->re) + (state->im * state->im));
//     state->phase_rad = atan2f(state->im, state->re);
// }

// static void fra_capture_point(
//     float frequency_hz,
//     uint samples_per_cycle,
//     uint settle_cycles,
//     uint measure_cycles,
//     uint32_t bias_mv,
//     uint32_t amplitude_mv,
//     bool drive_both_outputs,
//     fra_point_t *out_point
// ) {
//     uint64_t t_next_us = time_us_64();
//     uint32_t total_samples = samples_per_cycle * measure_cycles;
//     uint32_t settle_samples = samples_per_cycle * settle_cycles;
//     float sample_period_us = 1000000.0f / (frequency_hz * (float)samples_per_cycle);
//     uint32_t sample_period_ticks = (uint32_t)(sample_period_us + 0.5f);
//     float bias_v = (float)bias_mv / 1000.0f;
//     float amp_v = (float)amplitude_mv / 1000.0f * 0.5f;
//     float phase_step = (2.0f * FRA_PI) / (float)samples_per_cycle;
//     float phase = 0.0f;
//     fra_lockin_result_t probe1;
//     fra_lockin_result_t probe2;

//     fra_lockin_reset(&probe1);
//     fra_lockin_reset(&probe2);

//     for (uint32_t sample = 0; sample < settle_samples + total_samples; ++sample) {
//         float ref_sin = sinf(phase);
//         float ref_cos = cosf(phase);
//         float out_v = bias_v + (amp_v * ref_sin);
//         uint16_t level = fra_voltage_to_pwm_level(out_v);
//         uint16_t raw1;
//         uint16_t raw2;

//         fra_pwm_write(FRA_PWM_PIN_A, level);
//         if (drive_both_outputs) {
//             fra_pwm_write(FRA_PWM_PIN_B, level);
//         }

//         t_next_us += sample_period_ticks;
//         busy_wait_until(from_us_since_boot(t_next_us));

//         raw1 = fra_adc_read_gpio(FRA_PROBE_1_GPIO);
//         raw2 = fra_adc_read_gpio(FRA_PROBE_2_GPIO);

//         if (sample >= settle_samples) {
//             float v1 = fra_adc_counts_to_voltage(raw1);
//             float v2 = fra_adc_counts_to_voltage(raw2);

//             probe1.dc += v1;
//             probe1.re += v1 * ref_sin;
//             probe1.im += v1 * ref_cos;

//             probe2.dc += v2;
//             probe2.re += v2 * ref_sin;
//             probe2.im += v2 * ref_cos;
//         }

//         phase += phase_step;
//         if (phase >= (2.0f * FRA_PI)) {
//             phase -= (2.0f * FRA_PI);
//         }
//     }

//     fra_lockin_finalize(&probe1, total_samples);
//     fra_lockin_finalize(&probe2, total_samples);

//     out_point->frequency_hz = frequency_hz;
//     out_point->input_vpp = 2.0f * probe1.amplitude;
//     out_point->output_vpp = 2.0f * probe2.amplitude;
//     out_point->input_dc_v = probe1.dc;
//     out_point->output_dc_v = probe2.dc;

//     if (probe1.amplitude > FRA_EPSILON && probe2.amplitude > FRA_EPSILON) {
//         out_point->gain_db = 20.0f * log10f(probe2.amplitude / probe1.amplitude);
//         out_point->phase_deg = (probe2.phase_rad - probe1.phase_rad) * (180.0f / FRA_PI);
//     } else {
//         out_point->gain_db = -120.0f;
//         out_point->phase_deg = 0.0f;
//     }

//     while (out_point->phase_deg > 180.0f) {
//         out_point->phase_deg -= 360.0f;
//     }
//     while (out_point->phase_deg < -180.0f) {
//         out_point->phase_deg += 360.0f;
//     }
// }

// static float fra_frequency_for_index(const fra_sweep_cfg_t *cfg, uint16_t index) {
//     if (cfg->points <= 1u || cfg->start_hz >= cfg->stop_hz) {
//         return cfg->start_hz;
//     }

//     if (cfg->logarithmic) {
//         float start_ln = logf(cfg->start_hz);
//         float stop_ln = logf(cfg->stop_hz);
//         float step = (stop_ln - start_ln) / (float)(cfg->points - 1u);
//         return expf(start_ln + (step * (float)index));
//     }

//     return cfg->start_hz + (((cfg->stop_hz - cfg->start_hz) * (float)index) / (float)(cfg->points - 1u));
// }

// void fra_init(void) {
//     if (g_fra_is_initialized) {
//         return;
//     }

//     fra_pwm_init_pin(FRA_PWM_PIN_A, &g_pwm_slice_a, FRA_DEFAULT_PWM_CARRIER_HZ);
//     fra_pwm_init_pin(FRA_PWM_PIN_B, &g_pwm_slice_b, FRA_DEFAULT_PWM_CARRIER_HZ);
//     fra_pwm_write(FRA_PWM_PIN_A, FRA_PWM_CENTER);
//     fra_pwm_write(FRA_PWM_PIN_B, FRA_PWM_CENTER);

//     adc_init();
//     adc_gpio_init(FRA_PROBE_1_GPIO);
//     adc_gpio_init(FRA_PROBE_2_GPIO);

//     g_fra_is_initialized = true;
// }

// void fra_deinit(void) {
//     if (!g_fra_is_initialized) {
//         return;
//     }

//     fra_pwm_write(FRA_PWM_PIN_A, FRA_PWM_CENTER);
//     fra_pwm_write(FRA_PWM_PIN_B, FRA_PWM_CENTER);
//     pwm_set_enabled(g_pwm_slice_a, false);
//     pwm_set_enabled(g_pwm_slice_b, false);

//     g_fra_is_initialized = false;
// }

// bool fra_run_sweep(
//     const fra_sweep_cfg_t *cfg,
//     fra_point_t *points,
//     uint16_t point_capacity,
//     uint16_t *out_count,
//     fra_abort_cb_t abort_cb
// ) {
//     uint16_t count;

//     if (out_count != NULL) {
//         *out_count = 0u;
//     }

//     if (!g_fra_is_initialized || cfg == NULL || points == NULL || point_capacity == 0u) {
//         return false;
//     }

//     if (cfg->start_hz < FRA_MIN_OUTPUT_FREQ_HZ || cfg->stop_hz < cfg->start_hz || cfg->points == 0u) {
//         return false;
//     }

//     count = cfg->points;
//     if (count > point_capacity) {
//         count = point_capacity;
//     }
//     if (count > FRA_MAX_SWEEP_POINTS) {
//         count = FRA_MAX_SWEEP_POINTS;
//     }

//     fra_on_sweep_begin(cfg);

//     for (uint16_t i = 0; i < count; ++i) {
//         uint settle_cycles = cfg->settle_cycles < FRA_MIN_SETTLE_CYCLES ? FRA_MIN_SETTLE_CYCLES : cfg->settle_cycles;
//         uint measure_cycles = cfg->measure_cycles < FRA_MIN_MEASURE_CYCLES ? FRA_MIN_MEASURE_CYCLES : cfg->measure_cycles;
//         float frequency_hz = fra_frequency_for_index(cfg, i);
//         uint samples_per_cycle = fra_samples_per_cycle_for(frequency_hz, cfg->samples_per_cycle);

//         if (abort_cb != NULL && abort_cb()) {
//             if (out_count != NULL) {
//                 *out_count = i;
//             }
//             return false;
//         }

//         fra_capture_point(
//             frequency_hz,
//             samples_per_cycle,
//             settle_cycles,
//             measure_cycles,
//             cfg->output_bias_mv == 0u ? FRA_DEFAULT_BIAS_MV : cfg->output_bias_mv,
//             cfg->output_amplitude_mv == 0u ? FRA_DEFAULT_AMPLITUDE_MV : cfg->output_amplitude_mv,
//             cfg->drive_both_outputs,
//             &points[i]
//         );

//         fra_on_point_ready(&points[i], i, count);
//     }

//     if (out_count != NULL) {
//         *out_count = count;
//     }

//     fra_on_sweep_complete(points, count);
//     return true;
// }

// void fra_get_default_sweep_cfg(fra_sweep_cfg_t *cfg) {
//     if (cfg == NULL) {
//         return;
//     }

//     cfg->start_hz = 10.0f;
//     cfg->stop_hz = 20000.0f;
//     cfg->points = 40u;
//     cfg->samples_per_cycle = 64u;
//     cfg->settle_cycles = 6u;
//     cfg->measure_cycles = 12u;
//     cfg->output_bias_mv = FRA_DEFAULT_BIAS_MV;
//     cfg->output_amplitude_mv = FRA_DEFAULT_AMPLITUDE_MV;
//     cfg->logarithmic = true;
//     cfg->drive_both_outputs = false;
// }

// bool fra_measure_single_frequency(float frequency_hz, fra_point_t *out_point) {
//     fra_sweep_cfg_t cfg;
//     uint16_t point_count = 0;

//     if (out_point == NULL) {
//         return false;
//     }

//     fra_get_default_sweep_cfg(&cfg);
//     cfg.start_hz = frequency_hz;
//     cfg.stop_hz = frequency_hz;
//     cfg.points = 1u;

//     return fra_run_sweep(&cfg, out_point, 1u, &point_count, NULL) && point_count == 1u;
// }