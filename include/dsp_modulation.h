#ifndef DSP_MODULATION_H
#define DSP_MODULATION_H

#include "dsp_math.h"

// --- Random Walk Generator ---
// Generates a slowly drifting value.
typedef struct {
  q31_t current_value; // Q31
  q31_t target_value;  // Q31
  q31_t step_size;     // Q31 - how fast it moves per frame
  uint32_t seed;       // LCG seed
} dsp_random_walk_t;

void dsp_rand_walk_init(dsp_random_walk_t *walk, uint32_t seed,
                        q31_t initial_step);
void dsp_rand_walk_set_step(dsp_random_walk_t *walk, q31_t step_size);
// Returns a new random value in range [-1, 1] (or subset)
q31_t dsp_rand_walk_process(dsp_random_walk_t *walk);

// --- Envelope Follower ---
// Simple AR or 1-pole follower
typedef struct {
  q31_t envelope; // Current level
  q31_t attack;   // Attack coeff (0..1)
  q31_t release;  // Release coeff (0..1)
} dsp_env_follower_t;

void dsp_env_init(dsp_env_follower_t *env, q31_t attack, q31_t release);
q31_t dsp_env_process(dsp_env_follower_t *env, q31_t in);

// --- Perlin Noise (1D) ---
// Simplified Perlin noise for organic modulation
#define PERLIN_TABLE_SIZE 256

typedef struct {
  uint32_t seed;
  int8_t gradient_table[PERLIN_TABLE_SIZE];  // Pre-computed gradients
  uint8_t perm_table[PERLIN_TABLE_SIZE];     // Permutation table
} dsp_perlin_t;

void dsp_perlin_init(dsp_perlin_t *perlin, uint32_t seed);
q31_t dsp_perlin_1d(dsp_perlin_t *perlin, q31_t t);  // t in Q31 (0..1 range)

#endif // DSP_MODULATION_H
