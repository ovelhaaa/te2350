#ifndef DSP_PITCH_H
#define DSP_PITCH_H

#include "dsp_delay.h"
#include "dsp_math.h"

// --- Pitch Shifter ---
// Simple pitch shifter using delay modulation with crossfade.
// Based on dual-head tape varispeed technique. Each shifter instance owns its
// own delay buffer/read heads, so separate musical roles (for example shimmer
// vs. feedback octave) should use separate dsp_pitch_shifter_t objects.

typedef struct {
  dsp_delay_t delay;
  q16_16_t read_pos_a;  // First read head delay position (Q16.16 samples)
  q16_16_t read_pos_b;  // Second read head delay position (Q16.16 samples)
  q31_t crossfade;      // Last crossfade phase (0..Q31_MAX)
  q16_16_t pitch_inc;       // Pitch increment (Q16.16 ratio)
  size_t window_size;       // Read-head wrap/crossfade window size in samples
  uint32_t window_phase_inc; // Precomputed Q31 phase increment per window sample
} dsp_pitch_shifter_t;

/**
 * @brief Initialize pitch shifter with the full buffer as the crossfade window.
 *
 * @param ps Pitch shifter structure
 * @param buffer Delay buffer (externally allocated)
 * @param size Buffer size (must be power of 2)
 */
void dsp_pitch_init(dsp_pitch_shifter_t *ps, q31_t *buffer, size_t size);

/**
 * @brief Set the read-head wrap/crossfade window.
 *
 * Shorter windows reduce pitch-shifter latency and can preserve transients, at
 * the cost of more frequent crossfades. The value is clamped to the shifter's
 * delay-buffer size and to the largest Q16.16-safe window (65535 samples).
 *
 * @param ps Pitch shifter structure
 * @param window_size Window size in samples
 */
void dsp_pitch_set_window_size(dsp_pitch_shifter_t *ps, size_t window_size);

/**
 * @brief Convert a musical octave amount to dsp_pitch_process() control scale.
 *
 * @param octave_amount Q31 control where 0 = unison (1.0x) and Q31_MAX = +1 octave (2.0x)
 * @return q31_t Internal pitch control where 0..Q31_MAX maps to 0.5x..2.0x
 */
q31_t dsp_pitch_ratio_from_octave_amount(q31_t octave_amount);

/**
 * @brief Process one sample through pitch shifter.
 *
 * @param ps Pitch shifter structure
 * @param in Input sample
 * @param pitch_ratio_q31 Internal pitch control: 0 maps to 0.5x, Q31_MAX/3 maps to 1.0x (no shift), and Q31_MAX maps to 2.0x (+1 octave)
 * @return q31_t Pitch-shifted output
 */
q31_t dsp_pitch_process(dsp_pitch_shifter_t *ps, q31_t in, q31_t pitch_ratio_q31);

#endif // DSP_PITCH_H
