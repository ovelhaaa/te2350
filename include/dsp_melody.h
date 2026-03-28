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

#endif // DSP_MELODY_H
