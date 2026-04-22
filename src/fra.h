#ifndef FRA_H
#define FRA_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float frequency_hz;
    float input_vpp;
    float output_vpp;
    float gain_db;
    float phase_deg;
    float input_dc_v;
    float output_dc_v;
} fra_point_t;

typedef struct {
    float start_hz;
    float stop_hz;
    uint16_t points;
    uint16_t samples_per_cycle;
    uint16_t settle_cycles;
    uint16_t measure_cycles;
    uint32_t output_bias_mv;
    uint32_t output_amplitude_mv;
    bool logarithmic;
    bool drive_both_outputs;
} fra_sweep_cfg_t;

typedef bool (*fra_abort_cb_t)(void);

void fra_init(void);
void fra_deinit(void);

bool fra_run_sweep(
    const fra_sweep_cfg_t *cfg,
    fra_point_t *points,
    uint16_t point_capacity,
    uint16_t *out_count,
    fra_abort_cb_t abort_cb
);

void fra_get_default_sweep_cfg(fra_sweep_cfg_t *cfg);
bool fra_measure_single_frequency(float frequency_hz, fra_point_t *out_point);

void fra_on_sweep_begin(const fra_sweep_cfg_t *cfg);
void fra_on_point_ready(const fra_point_t *point, uint16_t index, uint16_t total);
void fra_on_sweep_complete(const fra_point_t *points, uint16_t count);

#endif