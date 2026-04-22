#include <math.h>                  // gives access to sin()
#include <stdint.h>                // gives fixed-width integer types like uint16_t
#include <stdbool.h>               // gives bool, true, false

#include "pico/stdlib.h"           // standard Pico SDK functions
#include "hardware/pwm.h"          // PWM hardware control
#include "hardware/irq.h"          // interrupt support

#include "wavegen.h"               // header file for this module

#define M_PI 3.14
// ------------------------------------------------------------
// Pin definitions for your custom PCB
// ------------------------------------------------------------

// GPIO28 is wave generator output channel 1
#define WAVEGEN_PIN_1 28

// GPIO30 is wave generator output channel 2
#define WAVEGEN_PIN_2 30


// ------------------------------------------------------------
// PWM/sample settings
// ------------------------------------------------------------

// PWM counter wraps at 255, so duty cycle values go from 0 to 255
#define PWM_WRAP_VALUE 255

// Number of points in the sine lookup table
#define TABLE_SIZE 256

// How often we update the PWM duty cycle in the interrupt
// This acts like the waveform sample rate
#define SAMPLE_RATE 40000


// ------------------------------------------------------------
// Global variables
// ------------------------------------------------------------

// Stores one full sine wave as 256 samples
static uint16_t sine_table[TABLE_SIZE];

// PWM slice number for channel 1
static uint slice1;

// PWM slice number for channel 2
static uint slice2;

// PWM channel (A or B) for GPIO28
static uint chan1;

// PWM channel (A or B) for GPIO30
static uint chan2;

// Phase accumulator for waveform channel 1
static uint32_t phase_accum_1 = 0;

// Phase accumulator for waveform channel 2
static uint32_t phase_accum_2 = 0;

// Phase step for waveform channel 1
// Bigger value = higher frequency
static uint32_t phase_step_1 = 0;

// Phase step for waveform channel 2
static uint32_t phase_step_2 = 0;

// Tracks whether channel 1 is in manual PWM mode
static bool manual_mode_1 = true;

// Tracks whether channel 2 is in manual PWM mode
static bool manual_mode_2 = true;


// ------------------------------------------------------------
// Helper function: build sine lookup table
// ------------------------------------------------------------

static void wavegen_build_sine_table(void) {
    // Loop through every entry in the table
    for (int i = 0; i < TABLE_SIZE; i++) {
        // Compute sine value from 0 to 2*pi over the full table
        float angle = 2.0f * (float)M_PI * ((float)i / (float)TABLE_SIZE);

        // sin(angle) gives a value from -1 to +1
        float s = sinf(angle);

        // Shift and scale it into the PWM range 0 to 255
        // Center is about 127.5
        float shifted = ((s + 1.0f) * 0.5f) * PWM_WRAP_VALUE;

        // Store as an integer
        sine_table[i] = (uint16_t)(shifted + 0.5f);
    }
}


// ------------------------------------------------------------
// Helper function: set up PWM on one pin
// ------------------------------------------------------------

static void wavegen_setup_pwm_pin(uint gpio, uint *slice_out, uint *chan_out) {
    // Set this GPIO pin to use the PWM hardware instead of normal GPIO
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    // Find which PWM slice controls this GPIO pin
    *slice_out = pwm_gpio_to_slice_num(gpio);

    // Find whether this pin is channel A or channel B inside that slice
    *chan_out = pwm_gpio_to_channel(gpio);

    // Set PWM clock divider
    // Assumes 150 MHz system clock, divides it down for a good PWM rate
    pwm_set_clkdiv(*slice_out, 1.0f);

    // Set the PWM period
    // Counter counts from 0 to PWM_WRAP_VALUE
    pwm_set_wrap(*slice_out, PWM_WRAP_VALUE);

    // Start at 50% duty cycle
    pwm_set_chan_level(*slice_out, *chan_out, PWM_WRAP_VALUE / 2);

    // Turn on this PWM slice
    pwm_set_enabled(*slice_out, true);
}


// ------------------------------------------------------------
// Interrupt handler
// This runs repeatedly and updates PWM duty cycle
// when a channel is in waveform mode.
// ------------------------------------------------------------

static void wavegen_pwm_irq_handler(void) {
    // Clear the interrupt flag for slice1
    pwm_clear_irq(slice1);

    // -------- Channel 1 --------
    // Only update channel 1 automatically if it is NOT in manual mode
    if (!manual_mode_1) {
        // Add phase step to phase accumulator
        phase_accum_1 += phase_step_1;

        // Top 8 bits choose the sine table index (0 to 255)
        uint16_t index1 = (phase_accum_1 >> 24) & 0xFF;

        // Set PWM duty cycle from the sine table
        pwm_set_chan_level(slice1, chan1, sine_table[index1]);
    }

    // -------- Channel 2 --------
    // Only update channel 2 automatically if it is NOT in manual mode
    if (!manual_mode_2) {
        // Add phase step to phase accumulator
        phase_accum_2 += phase_step_2;

        // Top 8 bits choose the sine table index (0 to 255)
        uint16_t index2 = (phase_accum_2 >> 24) & 0xFF;

        // Set PWM duty cycle from the sine table
        pwm_set_chan_level(slice2, chan2, sine_table[index2]);
    }
}


// ------------------------------------------------------------
// Public function: initialize the wave generator module
// ------------------------------------------------------------

void wavegen_init(void) {
    // Build the sine lookup table once
    wavegen_build_sine_table();

    // Set up PWM on GPIO28
    wavegen_setup_pwm_pin(WAVEGEN_PIN_1, &slice1, &chan1);

    // Set up PWM on GPIO30
    wavegen_setup_pwm_pin(WAVEGEN_PIN_2, &slice2, &chan2);

    // Set the PWM interrupt to come from slice1
    // We use slice1 as the "timing source" for waveform updates
    pwm_clear_irq(slice1);

    // Enable interrupt generation for slice1
    pwm_set_irq_enabled(slice1, true);

    // Tell the CPU which function should run when PWM IRQ happens
    irq_set_exclusive_handler(PWM_IRQ_WRAP, wavegen_pwm_irq_handler);

    // Turn on the PWM interrupt in the NVIC
    irq_set_enabled(PWM_IRQ_WRAP, true);

    // Start both outputs centered at 50%
    pwm_set_chan_level(slice1, chan1, PWM_WRAP_VALUE / 2);
    pwm_set_chan_level(slice2, chan2, PWM_WRAP_VALUE / 2);
}


// ------------------------------------------------------------
// Public function: put one channel into manual PWM mode
// In this mode, software directly controls duty cycle.
// ------------------------------------------------------------

void wavegen_set_manual_mode(uint channel) {
    // If channel 0 is selected, mark it as manual
    if (channel == 0) {
        manual_mode_1 = true;
    }

    // If channel 1 is selected, mark it as manual
    else if (channel == 1) {
        manual_mode_2 = true;
    }
}


// ------------------------------------------------------------
// Public function: set a manual duty cycle directly
// duty should be between 0 and 255
// ------------------------------------------------------------

void wavegen_set_duty(uint channel, uint16_t duty) {
    // Prevent duty from going above the allowed PWM range
    if (duty > PWM_WRAP_VALUE) {
        duty = PWM_WRAP_VALUE;
    }

    // Channel 0 controls GPIO28
    if (channel == 0) {
        // Put channel 0 into manual mode
        manual_mode_1 = true;

        // Update PWM compare value directly
        pwm_set_chan_level(slice1, chan1, duty);
    }

    // Channel 1 controls GPIO30
    else if (channel == 1) {
        // Put channel 1 into manual mode
        manual_mode_2 = true;

        // Update PWM compare value directly
        pwm_set_chan_level(slice2, chan2, duty);
    }
}


// ------------------------------------------------------------
// Public function: set duty cycle using percentage
// percent should be between 0 and 100
// ------------------------------------------------------------

void wavegen_set_percent(uint channel, float percent) {
    // Clamp percent so it stays in range
    if (percent < 0.0f) {
        percent = 0.0f;
    }
    if (percent > 100.0f) {
        percent = 100.0f;
    }

    // Convert percent into a 0-255 duty value
    uint16_t duty = (uint16_t)((percent / 100.0f) * PWM_WRAP_VALUE);

    // Reuse the duty-setting function
    wavegen_set_duty(channel, duty);
}


// ------------------------------------------------------------
// Public function: enable sine-wave mode on one channel
// freq_hz = output sine frequency
// ------------------------------------------------------------

void wavegen_set_sine_mode(uint channel, float freq_hz) {
    // Prevent negative frequency
    if (freq_hz < 0.0f) {
        freq_hz = 0.0f;
    }

    // Convert desired output frequency into phase step
    // This is direct digital synthesis (DDS)
    uint32_t phase_step = (uint32_t)((freq_hz * 4294967296.0f) / SAMPLE_RATE);

    // Channel 0 uses channel 1 data
    if (channel == 0) {
        manual_mode_1 = false;
        phase_step_1 = phase_step;
    }

    // Channel 1 uses channel 2 data
    else if (channel == 1) {
        manual_mode_2 = false;
        phase_step_2 = phase_step;
    }
}


// ------------------------------------------------------------
// Public function: stop one channel
// This returns output to midscale so the filtered analog output
// sits near the center instead of rail-to-rail.
// ------------------------------------------------------------

void wavegen_stop(uint channel) {
    // Stop channel 0
    if (channel == 0) {
        manual_mode_1 = true;
        phase_step_1 = 0;
        phase_accum_1 = 0;
        pwm_set_chan_level(slice1, chan1, PWM_WRAP_VALUE / 2);
    }

    // Stop channel 1
    else if (channel == 1) {
        manual_mode_2 = true;
        phase_step_2 = 0;
        phase_accum_2 = 0;
        pwm_set_chan_level(slice2, chan2, PWM_WRAP_VALUE / 2);
    }
}