#include "../include/dsp_melody.h"

// Notes: E3, G3, A3, B3, D4, E4, G4, A4
static const float scale_freqs[] = {
    164.81f, 196.00f, 220.00f, 246.94f, 293.66f, 329.63f, 392.00f, 440.00f
};

static uint32_t fast_rand(dsp_melody_t *ctx) {
    ctx->rnd_seed = ctx->rnd_seed * 1664525 + 1013904223;
    return ctx->rnd_seed;
}

static uint32_t map_density_to_interval(q31_t density) {
    // 0.0 -> sparse (~1.5s), 1.0 -> dense (~0.125s)
    uint32_t sparse = 72000;
    uint32_t dense = 6000;
    uint32_t blend = (uint32_t)(((int64_t)density * (sparse - dense)) >> 31);
    return sparse - blend;
}

static int32_t map_decay_to_shift(q31_t decay) {
    // Lower shift => longer note.
    // decay=0 -> fast (~>>11), decay=1 -> slow (~>>15)
    return 11 + (int32_t)(((int64_t)decay * 4) >> 31);
}

void dsp_melody_init(dsp_melody_t *ctx) {
    ctx->mel_phase = 0;
    ctx->mel_inc = 0;
    ctx->mel_timer = 0;
    ctx->mel_next_time = 0;
    ctx->mel_env = 0;
    ctx->mel_env_decay = 14;
    ctx->volume = FLOAT_TO_Q31(0.18f);
    ctx->density = FLOAT_TO_Q31(0.45f);
    ctx->decay = FLOAT_TO_Q31(0.55f);
    ctx->enabled = false;
    ctx->rnd_seed = 12345;
}

q31_t dsp_melody_process(dsp_melody_t *ctx) {
    if (!ctx->enabled) {
        return 0;
    }

    q31_t out = 0;

    ctx->mel_timer++;
    if (ctx->mel_timer >= ctx->mel_next_time) {
        ctx->mel_timer = 0;
        uint32_t base_interval = map_density_to_interval(ctx->density);
        uint32_t rhythmic_div = (fast_rand(ctx) % 4);
        if (rhythmic_div == 0) base_interval >>= 1;       // denser accent
        else if (rhythmic_div == 3) base_interval += 6000; // occasional breath
        ctx->mel_next_time = base_interval + (fast_rand(ctx) % 700);

        // Pick random note from scale
        int note_idx = fast_rand(ctx) % (sizeof(scale_freqs)/sizeof(scale_freqs[0]));
        float freq = scale_freqs[note_idx];
        // Subtle octave lift for motion while preserving identity
        if ((fast_rand(ctx) & 0x0F) == 0x0F) {
            freq *= 2.0f;
        }
        ctx->mel_inc = (uint32_t)(freq * 89478.4853f); // (2^32 / 48000)

        // Trigger Envelope with tiny accents
        int32_t accent = (fast_rand(ctx) & 0x03) == 0 ? 0x07000000 : 0x05000000;
        ctx->mel_env = accent;
        ctx->mel_env_decay = map_decay_to_shift(ctx->decay);
    }

    // Sound Generation
    ctx->mel_phase += ctx->mel_inc;
    // Simple Envelope Decay
    if (ctx->mel_env > 256) ctx->mel_env -= (ctx->mel_env >> ctx->mel_env_decay);

    // Triangle-ish Wave (less harsh than saw)
    int32_t wav = (ctx->mel_phase >> 1) ^ (ctx->mel_phase & 0x80000000 ? 0xFFFFFFFF : 0);
    out = ((int64_t)wav * ctx->mel_env) >> 30;
    out = q31_mul(out, ctx->volume);

    return out;
}

void dsp_melody_set_enabled(dsp_melody_t *ctx, bool enabled) {
    ctx->enabled = enabled;
}

void dsp_melody_set_volume(dsp_melody_t *ctx, q31_t volume) {
    if (volume < 0) volume = 0;
    if (volume > Q31_MAX) volume = Q31_MAX;
    ctx->volume = volume;
}

void dsp_melody_set_density(dsp_melody_t *ctx, q31_t density) {
    if (density < 0) density = 0;
    if (density > Q31_MAX) density = Q31_MAX;
    ctx->density = density;
}

void dsp_melody_set_decay(dsp_melody_t *ctx, q31_t decay) {
    if (decay < 0) decay = 0;
    if (decay > Q31_MAX) decay = Q31_MAX;
    ctx->decay = decay;
}
