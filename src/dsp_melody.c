#include "../include/dsp_melody.h"

// Notes: E3, G3, A3, B3, D4, E4, G4, A4
static const float scale_freqs[] = {
    164.81f, 196.00f, 220.00f, 246.94f, 293.66f, 329.63f, 392.00f, 440.00f
};

static uint32_t fast_rand(dsp_melody_t *ctx) {
    ctx->rnd_seed = ctx->rnd_seed * 1664525 + 1013904223;
    return ctx->rnd_seed;
}

void dsp_melody_init(dsp_melody_t *ctx) {
    ctx->mel_phase = 0;
    ctx->mel_inc = 0;
    ctx->mel_timer = 0;
    ctx->mel_next_time = 0;
    ctx->mel_env = 0;
    ctx->rnd_seed = 12345;
}

q31_t dsp_melody_process(dsp_melody_t *ctx) {
    q31_t out = 0;

    ctx->mel_timer++;
    if (ctx->mel_timer >= ctx->mel_next_time) {
        ctx->mel_timer = 0;
        // Next note duration logic (Rhythmic Variation)
        // Base BPM ~120 (48000 samples/sec -> 24000 samples per beat)

        uint32_t r = fast_rand(ctx) % 100;
        if (r < 20) {
            // 1/16th notes (bursts)
            ctx->mel_next_time = 6000;
        } else if (r < 50) {
            // 1/8th notes
            ctx->mel_next_time = 12000;
        } else if (r < 80) {
            // 1/4 notes (beat)
            ctx->mel_next_time = 24000;
        } else {
            // Long notes / pauses
            ctx->mel_next_time = 48000 + (fast_rand(ctx) % 24000);
        }

        // Humanize (jitter)
        ctx->mel_next_time += (fast_rand(ctx) % 500);

        // Pick random note from scale
        int note_idx = fast_rand(ctx) % (sizeof(scale_freqs)/sizeof(scale_freqs[0]));
        float freq = scale_freqs[note_idx];
        ctx->mel_inc = (uint32_t)(freq * 89478.4853f); // (2^32 / 48000)

        // Trigger Envelope (low velocity)
        ctx->mel_env = 0x05000000; // ~-26dBFS peak
    }

    // Sound Generation
    ctx->mel_phase += ctx->mel_inc;
    // Simple Envelope Decay
    if (ctx->mel_env > 256) ctx->mel_env -= (ctx->mel_env >> 14);

    // Triangle-ish Wave (less harsh than saw)
    int32_t wav = (ctx->mel_phase >> 1) ^ (ctx->mel_phase & 0x80000000 ? 0xFFFFFFFF : 0);
    out = ((int64_t)wav * ctx->mel_env) >> 30;

    return out;
}
