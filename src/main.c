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
// Notes: E3, G3, A3, B3, D4, E4, G4, A4
static const float scale_freqs[] = { 
    164.81f, 196.00f, 220.00f, 246.94f, 293.66f, 329.63f, 392.00f, 440.00f 
};

static uint32_t mel_phase = 0;
static uint32_t mel_inc = 0;
static uint32_t mel_timer = 0;
static uint32_t mel_next_time = 0;
static int32_t  mel_env = 0;
static uint32_t rnd_seed = 12345;
static bool melody_enabled = false;  // Disabled by default

static uint32_t fast_rand(void) {
    rnd_seed = rnd_seed * 1664525 + 1013904223;
    return rnd_seed;
}

// --- Audio Callback ---
void process_audio_block(int32_t *rx_buffer, int32_t *tx_buffer, size_t sample_count) {
    for (size_t i = 0; i < sample_count; i++) {
        q31_t dry_signal = 0;
        
        // 1. Sequencer Logic (only if melody enabled)
        if (melody_enabled) {
            mel_timer++;
            if (mel_timer >= mel_next_time) {
                mel_timer = 0;
                // Next note duration logic (Rhythmic Variation)
                // Base BPM ~120 (48000 samples/sec -> 24000 samples per beat)
                
                uint32_t r = fast_rand() % 100;
                if (r < 20) {
                    // 1/16th notes (bursts)
                    mel_next_time = 6000;
                } else if (r < 50) {
                    // 1/8th notes
                    mel_next_time = 12000;
                } else if (r < 80) {
                    // 1/4 notes (beat)
                    mel_next_time = 24000;
                } else {
                    // Long notes / pauses
                    mel_next_time = 48000 + (fast_rand() % 24000);
                }
                
                // Humanize (jitter)
                mel_next_time += (fast_rand() % 500);
                
                // Pick random note from scale
                int note_idx = fast_rand() % (sizeof(scale_freqs)/sizeof(scale_freqs[0]));
                float freq = scale_freqs[note_idx];
                mel_inc = (uint32_t)(freq * 89478.4853f); // (2^32 / 48000)
                
                // Trigger Envelope (low velocity)
                mel_env = 0x05000000; // ~-26dBFS peak
            }

            // 2. Sound Generation
            mel_phase += mel_inc;
            // Simple Envelope Decay
            if (mel_env > 256) mel_env -= (mel_env >> 14);
            
            // Triangle-ish Wave (less harsh than saw)
            int32_t wav = (mel_phase >> 1) ^ (mel_phase & 0x80000000 ? 0xFFFFFFFF : 0);
            dry_signal = ((int64_t)wav * mel_env) >> 30;
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
    if (!te2350_init(&pedal, memory_pool, MEM_POOL_SIZE)) {
        printf("Effect Init FAILED\n");
    }
    te2350_set_time(&pedal, FLOAT_TO_Q31(0.8f));  // Long delay for cloud
    te2350_set_feedback(&pedal, FLOAT_TO_Q31(0.90f)); // High feedback for sustain
    
    // synth_init_test(440.0f); // Removed

    // Init Audio
    if (!audio_init(process_audio_block)) {
        printf("Audio Init FAILED\n");
        while(1) { gpio_put(25,1); sleep_ms(50); gpio_put(25,0); sleep_ms(50); }
    }

    printf("Sys Running. Cmds: q/w(Time), a/s(Fbk), z/x(Mix), e/r(Shim), t/y(Ton), d/f(Diff), c(Freeze), m(Melody)\n");

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
            
            if (changed || c == 'p') {
                // Approximate float conversion for display
                printf("T:%.2f F:%.2f M:%.2f S:%.2f D:%.2f Ton:%.2f\n",
                    (double)pedal.p_time / 2147483648.0,
                    (double)pedal.p_feedback / 2147483648.0,
                    (double)pedal.p_mix / 2147483648.0,
                    (double)pedal.p_shimmer / 2147483648.0,
                    (double)pedal.p_diffusion / 2147483648.0,
                    (double)pedal.p_tone / 2147483648.0
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
