#ifndef DSP_DELAY_H
#define DSP_DELAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include "dsp_math.h"

// --- Fixed Point Types & Constants ---
// Types and constants are defined in dsp_math.h

// --- Data Structures ---

/**
 * @brief Circular delay line structure.
 *
 * Uses an external buffer to allow static allocation by the caller.
 * The size MUST be a power of two for efficient masking.
 */
typedef struct {
  q31_t *buffer;    // Pointer to externally allocated buffer
  size_t size;      // Total size in samples (Must be Power of Two)
  size_t mask;      // Bitmask for fast wrapping (size - 1)
  size_t write_idx; // Current write index
} dsp_delay_t;

// --- Function Prototypes ---

/**
 * @brief Initialize the delay line.
 *
 * @param delay Pointer to the delay structure.
 * @param buffer Pointer to the data array (allocated by caller).
 * @param size Size of the buffer in samples. MUST be a power of two.
 */
void dsp_delay_init(dsp_delay_t *delay, q31_t *buffer, size_t size);

/**
 * @brief Write a sample to the delay line.
 *
 * Advances the write index automatically.
 *
 * @param delay Pointer to the delay structure.
 * @param sample The Q31 sample to write.
 */
void dsp_delay_write(dsp_delay_t *delay, q31_t sample);

/**
 * @brief Read a sample with fractional delay.
 *
 * Uses linear interpolation between samples.
 *
 * @param delay Pointer to the delay structure.
 * @param delay_samples_q16 The delay time in samples, fixed point Q16.16.
 *                          Example: 1.5 samples = 1 << 16 | 0x8000
 * @return q31_t Interpolated sample.
 */
q31_t dsp_delay_read_frac(const dsp_delay_t *delay, q16_16_t delay_samples_q16);

/**
 * @brief Basic integer read (no interpolation).
 *
 * @param delay Pointer to the delay structure.
 * @param delay_samples integer delay in samples.
 * @return q31_t The sample at the requested delay.
 */
q31_t dsp_delay_read(const dsp_delay_t *delay, size_t delay_samples);

/**
 * @brief Read a sample with Hermite interpolation (higher quality).
 *
 * Uses 4-point Hermite interpolation for smoother modulation.
 *
 * @param delay Pointer to the delay structure.
 * @param delay_samples_q16 The delay time in samples, fixed point Q16.16.
 * @return q31_t Interpolated sample.
 */
q31_t dsp_delay_read_hermite(const dsp_delay_t *delay, q16_16_t delay_samples_q16);

/**
 * @brief Hybrid Linear+Hermite interpolation (for pitch shifter).
 *
 * Blends 70% Hermite + 30% Linear to reduce warbling and digital shimmer.
 * Best for fast modulation (pitch shifting).
 *
 * @param delay Pointer to the delay structure.
 * @param delay_samples_q16 The delay time in samples, fixed point Q16.16.
 * @return q31_t Interpolated sample.
 */
q31_t dsp_delay_read_hybrid(const dsp_delay_t *delay, q16_16_t delay_samples_q16);

#endif // DSP_DELAY_H
