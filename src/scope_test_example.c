#include <stdio.h>

#include "oscilloscope.h"
#include "pico/stdlib.h"

static void print_scope_stats(const char *label, const scope_stats_t *stats) {
    printf(
        "%s: min=%.3f V  max=%.3f V  avg=%.3f V  vpp=%.3f V  rms=%.3f V\r\n",
        label,
        stats->voltage_min,
        stats->voltage_max,
        stats->voltage_avg,
        stats->voltage_pp,
        stats->voltage_rms
    );
}

void run_scope_uart_test(void) {
    uint16_t probe1[256];
    uint16_t probe2[256];
    scope_stats_t stats1;
    scope_stats_t stats2;

    scope_init();

    while (true) {
        bool ok = scope_capture_dual_raw_dma(probe1, probe2, 256, 10000);

        if (!ok) {
            printf("Scope capture failed\r\n");
            sleep_ms(500);
            continue;
        }

        scope_calculate_stats(probe1, 256, &stats1);
        scope_calculate_stats(probe2, 256, &stats2);

        printf("\r\n--- Scope Capture ---\r\n");
        print_scope_stats("Probe 1", &stats1);
        print_scope_stats("Probe 2", &stats2);

        printf(
            "First samples: P1=%u,%u,%u,%u   P2=%u,%u,%u,%u\r\n",
            probe1[0], probe1[1], probe1[2], probe1[3],
            probe2[0], probe2[1], probe2[2], probe2[3]
        );

        fflush(stdout);
        sleep_ms(500);
    }
}