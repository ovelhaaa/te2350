#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../../include/te2350.h"
#include "../../include/dsp_math.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

// Global effect instance
static te2350_t pedal;

// Memory pool for delay lines (128KB, same as main.c)
#define MEM_POOL_SIZE (128 * 1024)
static q31_t memory_pool[MEM_POOL_SIZE / 4];

// Interleaved audio buffers for processing
// Web Audio typically uses block size of 128
#define BLOCK_SIZE 128

// Internal Q31 buffers
static q31_t in_q31[BLOCK_SIZE]; // Mono input
static q31_t out_l_q31[BLOCK_SIZE];
static q31_t out_r_q31[BLOCK_SIZE];

EMSCRIPTEN_KEEPALIVE
bool wasm_te2350_init(float sample_rate) {
    bool success = te2350_init(&pedal, memory_pool, MEM_POOL_SIZE, sample_rate);

    // Set some defaults (similar to main.c)
    if (success) {
        te2350_set_time(&pedal, FLOAT_TO_Q31(0.8f));
        te2350_set_feedback(&pedal, FLOAT_TO_Q31(0.90f));
    }
    return success;
}

EMSCRIPTEN_KEEPALIVE
void wasm_te2350_process_block(float* in_f32, float* out_l_f32, float* out_r_f32, int num_samples) {
    if (num_samples > BLOCK_SIZE) {
        num_samples = BLOCK_SIZE; // Cap to buffer size
    }

    // Convert float to Q31 (assuming input is float mono)
    for (int i = 0; i < num_samples; i++) {
        in_q31[i] = float_to_q31_safe(in_f32[i]);
    }

    // Process
    for (int i = 0; i < num_samples; i++) {
        te2350_process(&pedal, in_q31[i], &out_l_q31[i], &out_r_q31[i]);
    }

    // Convert Q31 to float
    for (int i = 0; i < num_samples; i++) {
        out_l_f32[i] = Q31_TO_FLOAT(out_l_q31[i]);
        out_r_f32[i] = Q31_TO_FLOAT(out_r_q31[i]);
    }
}

// Parameter Setters exposed to Wasm

EMSCRIPTEN_KEEPALIVE
void wasm_te2350_set_time(float time) {
    te2350_set_time(&pedal, float_to_q31_safe(time));
}

EMSCRIPTEN_KEEPALIVE
void wasm_te2350_set_feedback(float feedback) {
    te2350_set_feedback(&pedal, float_to_q31_safe(feedback));
}

EMSCRIPTEN_KEEPALIVE
void wasm_te2350_set_mix(float mix) {
    te2350_set_mix(&pedal, float_to_q31_safe(mix));
}

EMSCRIPTEN_KEEPALIVE
void wasm_te2350_set_shimmer(float shimmer) {
    te2350_set_shimmer(&pedal, float_to_q31_safe(shimmer));
}

EMSCRIPTEN_KEEPALIVE
void wasm_te2350_set_diffusion(float diffusion) {
    te2350_set_diffusion(&pedal, float_to_q31_safe(diffusion));
}

EMSCRIPTEN_KEEPALIVE
void wasm_te2350_set_chaos(float chaos) {
    te2350_set_chaos(&pedal, float_to_q31_safe(chaos));
}

EMSCRIPTEN_KEEPALIVE
void wasm_te2350_set_tone(float tone) {
    te2350_set_tone(&pedal, float_to_q31_safe(tone));
}

EMSCRIPTEN_KEEPALIVE
void wasm_te2350_set_ducking(float ducking) {
    te2350_set_ducking(&pedal, float_to_q31_safe(ducking));
}

EMSCRIPTEN_KEEPALIVE
void wasm_te2350_set_wobble(float wobble) {
    te2350_set_wobble(&pedal, float_to_q31_safe(wobble));
}

EMSCRIPTEN_KEEPALIVE
void wasm_te2350_set_freeze(bool freeze) {
    te2350_set_freeze(&pedal, freeze);
}

EMSCRIPTEN_KEEPALIVE
void wasm_te2350_set_mod(float rate, float depth) {
    te2350_set_mod(&pedal, float_to_q31_safe(rate), float_to_q31_safe(depth));
}
