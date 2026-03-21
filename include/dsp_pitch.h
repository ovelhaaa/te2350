#ifndef DSP_PITCH_H
#define DSP_PITCH_H

#include "dsp_delay.h"
#include "dsp_math.h"

// --- Pitch Shifter ---
// Simple pitch shifter using delay modulation with crossfade
// Based on dual-head tape varispeed technique

typedef struct {
  dsp_delay_t delay;
  q16_16_t read_pos_a;  // First read head position
  q16_16_t read_pos_b;  // Second read head position
  q31_t crossfade;      // Crossfade position (0..Q31_MAX)
  q16_16_t pitch_inc;   // Pitch increment (Q16.16)
  size_t window_size;   // Crossfade window size
} dsp_pitch_shifter_t;

/**
 * @brief Initialize pitch shifter
 * 
 * @param ps Pitch shifter structure
 * @param buffer Delay buffer (externally allocated)
 * @param size Buffer size (must be power of 2)
 */
void dsp_pitch_init(dsp_pitch_shifter_t *ps, q31_t *buffer, size_t size);

/**
 * @brief Process one sample through pitch shifter
 * 
 * @param ps Pitch shifter structure
 * @param in Input sample
 * @param pitch_ratio Pitch ratio (1.0 = no shift, 1.05 = +5 semitones, etc.)
 * @return q31_t Pitch-shifted output
 */
q31_t dsp_pitch_process(dsp_pitch_shifter_t *ps, q31_t in, q31_t pitch_ratio_q31);

#endif // DSP_PITCH_H
