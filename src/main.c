/**
 * TE-2350 "Antigravity" Main
 * Robust version for Pico 2
 */

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>

#include "hardware/pio.h"
#include "ws2812.pio.h"

#include "../include/audio_driver.h"
#include "../include/te2350.h"
#include "../include/dsp_melody.h"

#define WS2812_PIN 16
#define IS_RGBW false

static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// Simple Hue to RGB (0..255 -> RGB)
// Input: 0-255
void hue_to_rgb(uint8_t hue, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (hue < 85) {
        *r = (85 - hue) * 3;
        *g = (hue) * 3;
        *b = 0;
    } else if (hue < 170) {
        hue -= 85;
        *r = 0;
        *g = (85 - hue) * 3;
        *b = (hue) * 3;
    } else {
        hue -= 170;
        *r = (hue) * 3;
        *g = 0;
        *b = (85 - hue) * 3;
    }
}

// System state
te2350_t pedal;
#define MEM_POOL_SIZE (128 * 1024) // Increased to 128KB for longer delay
static q31_t memory_pool[MEM_POOL_SIZE / 4];

// --- Generative Melody: Em Pentatonic ---
static dsp_melody_t melody;
static bool melody_enabled = false;  // Disabled by default

// --- Audio Callback ---
void process_audio_block(int32_t *rx_buffer, int32_t *tx_buffer, size_t sample_count) {
    for (size_t i = 0; i < sample_count; i++) {
        q31_t dry_signal = 0;
        
        // 1. Sequencer Logic (only if melody enabled)
        if (melody_enabled) {
            dry_signal = dsp_melody_process(&melody);
        }

        // 3. Mix External Input (if any) + Generated Melody
        q31_t in_sig = rx_buffer[i * 2];
        q31_t mixed = in_sig + dry_signal; // Basic summing
        
        q31_t out_l, out_r;
        te2350_process(&pedal, mixed, &out_l, &out_r);

        tx_buffer[i * 2]     = out_l;
        tx_buffer[i * 2 + 1] = out_r;
    }
}

int main() {
    // 200MHz System Clock
    set_sys_clock_khz(200000, true);
    stdio_init_all();

    // Init LED
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);

    // Init WS2812 (Neopixel)
    // Use PIO0 which is standard. GP16 is compatible with PIO0.
    PIO pio = pio0; 
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    // Startup Flash Sequence (Debug)
    for(int i=0; i<3; i++) {
        put_pixel(pio, sm, urgb_u32(20, 0, 0)); sleep_ms(200); // Red
        put_pixel(pio, sm, urgb_u32(0, 20, 0)); sleep_ms(200); // Green
        put_pixel(pio, sm, urgb_u32(0, 0, 20)); sleep_ms(200); // Blue
    }
    put_pixel(pio, sm, 0); // Off

    // Give some time to open serial
    for(int i=0; i<3; i++) {
        gpio_put(25, 1); sleep_ms(100);
        gpio_put(25, 0); sleep_ms(100);
    }

    printf("TE-2350 Starting...\n");

    // Init Effect
    // RP2040 I2S audio driver runs at 48kHz
    if (!te2350_init(&pedal, memory_pool, MEM_POOL_SIZE, 48000.0f)) {
        printf("Effect Init FAILED\n");
    }
    te2350_set_time(&pedal, FLOAT_TO_Q31(0.8f));  // Long delay for cloud
    te2350_set_feedback(&pedal, FLOAT_TO_Q31(0.90f)); // High feedback for sustain
    
    dsp_melody_init(&melody);

    // Init Audio
    if (!audio_init(process_audio_block)) {
        printf("Audio Init FAILED\n");
        while(1) { gpio_put(25,1); sleep_ms(50); gpio_put(25,0); sleep_ms(50); }
    }

    printf("Sys Running. Cmds: q/w(Time), a/s(Fbk), z/x(Mix), e/r(Shim), t/y(Ton), d/f(Diff), c(Freeze), m(Melody), o(Octave Toggle), i/u(Octave Amt)\n");

    // Main loop: Serial Processing
    while (true) {
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            bool changed = false;
            // Time
            if (c == 'q') { 
                q31_t v = pedal.p_time - FLOAT_TO_Q31(0.05f); 
                if(v < 0) v = 0; 
                te2350_set_time(&pedal, v); changed = true; 
            }
            if (c == 'w') { 
                q31_t v = q31_add_sat(pedal.p_time, FLOAT_TO_Q31(0.05f)); 
                te2350_set_time(&pedal, v); changed = true; 
            }
            
            // Feedback
            if (c == 'a') { 
                q31_t v = pedal.p_feedback - FLOAT_TO_Q31(0.05f); 
                if(v < 0) v = 0; 
                te2350_set_feedback(&pedal, v); changed = true; 
            }
            if (c == 's') { 
                q31_t v = q31_add_sat(pedal.p_feedback, FLOAT_TO_Q31(0.05f)); 
                te2350_set_feedback(&pedal, v); changed = true; 
            }
            
            // Mix
            if (c == 'z') { 
                q31_t v = pedal.p_mix - FLOAT_TO_Q31(0.05f); 
                if(v < 0) v = 0; 
                te2350_set_mix(&pedal, v); changed = true; 
            }
            if (c == 'x') { 
                q31_t v = q31_add_sat(pedal.p_mix, FLOAT_TO_Q31(0.05f)); 
                te2350_set_mix(&pedal, v); changed = true; 
            }

            // Shimmer / Pitch (Continuous 0.0 to 1.0)
            if (c == 'e') { 
                 q31_t v = pedal.p_shimmer - FLOAT_TO_Q31(0.05f);
                 if(v < 0) v = 0;
                 te2350_set_shimmer(&pedal, v); changed = true;
            }
            if (c == 'r') { 
                 q31_t v = q31_add_sat(pedal.p_shimmer, FLOAT_TO_Q31(0.05f));
                 te2350_set_shimmer(&pedal, v); changed = true;
            }

            // Tone - NEW
            if (c == 't') { 
                 q31_t v = pedal.p_tone - FLOAT_TO_Q31(0.05f);
                 if(v<0) v=0;
                 te2350_set_tone(&pedal, v); changed = true;
            }
            if (c == 'y') { 
                 q31_t v = q31_add_sat(pedal.p_tone, FLOAT_TO_Q31(0.05f));
                 te2350_set_tone(&pedal, v); changed = true;
            }

            // Diffusion
            if (c == 'd') { 
                q31_t v = pedal.p_diffusion - FLOAT_TO_Q31(0.1f);
                if(v<0) v=0;
                te2350_set_diffusion(&pedal, v); changed = true;
            }
            if (c == 'f') { 
                 q31_t v = q31_add_sat(pedal.p_diffusion, FLOAT_TO_Q31(0.1f));
                 te2350_set_diffusion(&pedal, v); changed = true;
            }
            
            // Freeze
            if (c == 'c') {
                te2350_set_freeze(&pedal, !pedal.freeze_mode);
                printf("Freeze: %d\n", pedal.freeze_mode);
            }
            
            // Melody Generator Toggle
            if (c == 'm') {
                melody_enabled = !melody_enabled;
                printf("Melody Generator: %s\n", melody_enabled ? "ON" : "OFF");
            }
            
            // Octave Feedback Toggle
            if (c == 'o') {
                te2350_set_octave_feedback(&pedal, !pedal.p_octave_feedback_enabled, pedal.p_octave_feedback);
                printf("Octave Feedback: %s\n", pedal.p_octave_feedback_enabled ? "ON" : "OFF");
            }

            // Octave Feedback Amount
            if (c == 'i') {
                q31_t v = pedal.p_octave_feedback - FLOAT_TO_Q31(0.05f);
                if(v < 0) v = 0;
                te2350_set_octave_feedback(&pedal, pedal.p_octave_feedback_enabled, v);
                changed = true;
            }
            if (c == 'u') {
                q31_t v = q31_add_sat(pedal.p_octave_feedback, FLOAT_TO_Q31(0.05f));
                te2350_set_octave_feedback(&pedal, pedal.p_octave_feedback_enabled, v);
                changed = true;
            }

            if (changed || c == 'p') {
                // Approximate float conversion for display
                printf("T:%.2f F:%.2f M:%.2f S:%.2f D:%.2f Ton:%.2f OctFb:%.2f\n",
                    (double)pedal.p_time / 2147483648.0,
                    (double)pedal.p_feedback / 2147483648.0,
                    (double)pedal.p_mix / 2147483648.0,
                    (double)pedal.p_shimmer / 2147483648.0,
                    (double)pedal.p_diffusion / 2147483648.0,
                    (double)pedal.p_tone / 2147483648.0,
                    (double)pedal.p_octave_feedback / 2147483648.0
                );
            }
        }
        
        // Blink heartbeat & Neopixel Update
        static uint32_t t = 0;
        if (t++ % 10000 == 0) { 
             // ...
        }
        
        // Neopixel "Bobaginha"
        // Update every ~20ms
         static uint64_t last_pixel_time = 0;
         uint64_t now = time_us_64();
         if (now - last_pixel_time > 20000) {
             last_pixel_time = now;
             
             // 1. Get Envelope (Volume) -> Brightness
             // Env is Q31.
             q31_t env = te2350_get_envelope(&pedal);
             uint8_t bright = (uint8_t)(env >> 23); 
             
             // Idle Heartbeat: Always show at least faint light to prove it works
             if (bright < 5) bright = 5; 
             if (bright > 50) bright = 50; 
             
             // 2. Get Modulator (Chaos) -> Hue
             q31_t mod = te2350_get_modulator(&pedal);
             int8_t hue_shift = (int8_t)(mod >> 26); 
             uint8_t hue = 140 + hue_shift; // Base cyan/blue
             
             // 3. Freeze Mode - Turn Red!
             if (pedal.freeze_mode) {
                 hue = 0; // Red
                 bright = 30 + (bright/2);
             }
             
             uint8_t r, g, b;
             hue_to_rgb(hue, &r, &g, &b);
             
             // Apply brightness
             // Ensure at least minimal visibility if bright=5
             r = (r * bright) >> 8;
             g = (g * bright) >> 8;
             b = (b * bright) >> 8;
             
             put_pixel(pio, sm, urgb_u32(r, g, b));
         }

        if (t % 100000 == 0) {
            gpio_xor_mask(1<<25);
            sleep_us(100); // Small sleep to yield
        }
    }
}
