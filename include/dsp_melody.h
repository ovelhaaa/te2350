#ifndef DSP_MELODY_H
#define DSP_MELODY_H

#include <stdint.h>
#include <stdbool.h>
#include "dsp_math.h"

// Generative Melody: Em Pentatonic
typedef struct {
    uint32_t mel_phase;
    uint32_t mel_inc;
    uint32_t mel_timer;
    uint32_t mel_next_time;
    int32_t  mel_env;
    int32_t  mel_env_decay;
    q31_t volume;
    q31_t density;
    q31_t decay;
    bool enabled;
    uint32_t rnd_seed;
} dsp_melody_t;

/**
 * @brief Initialize the melody generator.
 *
 * @param ctx Melody generator context.
 */
void dsp_melody_init(dsp_melody_t *ctx);

/**
 * @brief Process and return the next sample from the melody generator.
 *
 * @param ctx Melody generator context.
 * @return Generated sample (Q31).
 */
q31_t dsp_melody_process(dsp_melody_t *ctx);

void dsp_melody_set_enabled(dsp_melody_t *ctx, bool enabled);
void dsp_melody_set_volume(dsp_melody_t *ctx, q31_t volume);
void dsp_melody_set_density(dsp_melody_t *ctx, q31_t density);
void dsp_melody_set_decay(dsp_melody_t *ctx, q31_t decay);

#endif // DSP_MELODY_H
