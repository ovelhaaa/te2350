#include "../include/dsp_modulation.h"

// --- Random Walk ---

// LCG Constants
#define LCG_A 1664525
#define LCG_C 1013904223

static q31_t rand_q31(uint32_t *seed) {
  *seed = (*seed * LCG_A + LCG_C);
  // Convert uint32 to Q31 (0..MAX).
  // Ideally we want -1..1 range.
  // Cast to int32 works appropriately for full range noise.
  return (int32_t)(*seed);
}

void dsp_rand_walk_init(dsp_random_walk_t *walk, uint32_t seed,
                        q31_t initial_step) {
  walk->seed = seed;
  walk->current_value = 0;
  walk->target_value = 0;
  walk->step_size = initial_step;
}

void dsp_rand_walk_set_step(dsp_random_walk_t *walk, q31_t step_size) {
  walk->step_size = step_size;
}

q31_t dsp_rand_walk_process(dsp_random_walk_t *walk) {
  // Logic: move current towards target. If reached, pick new target.
  // Random walk with LIMITED range for smoother, more musical modulation

  // Check distance
  q31_t dist = q31_sub_sat(walk->target_value, walk->current_value);

  // If close enough, pick new target
  // Threshold: a bit larger than step size to avoid oscillation.
  // We'll just define close as abs(dist) < step.

  if (q31_abs(dist) <= walk->step_size) {
    walk->current_value = walk->target_value;
    
    // NEW: Pick target within ±0.3 of current value for smoothness
    q31_t rand_val = rand_q31(&walk->seed);
    q31_t offset = q31_mul(rand_val, FLOAT_TO_Q31(0.3f));
    walk->target_value = q31_add_sat(walk->current_value, offset);
    
    // Clamp to range [-0.8, 0.8] to avoid extremes
    q31_t max_val = FLOAT_TO_Q31(0.8f);
    q31_t min_val = FLOAT_TO_Q31(-0.8f);
    if (walk->target_value > max_val) walk->target_value = max_val;
    if (walk->target_value < min_val) walk->target_value = min_val;
    
    return walk->current_value;
  }

  // Move towards target
  if (dist > 0) {
    walk->current_value = q31_add_sat(walk->current_value, walk->step_size);
  } else {
    walk->current_value = q31_sub_sat(walk->current_value, walk->step_size);
  }

  return walk->current_value;
}

// --- Envelope Follower ---

void dsp_env_init(dsp_env_follower_t *env, q31_t attack, q31_t release) {
  env->envelope = 0;
  env->attack = attack;
  env->release = release;
}

q31_t dsp_env_process(dsp_env_follower_t *env, q31_t in) {
  q31_t abs_in = q31_abs(in);

  // Check if input > envelope (Attack phase) or input < envelope (Release
  // phase)
  if (abs_in > env->envelope) {
    // env = env + attack * (in - env)
    // or env = in * attack + env * (1-attack) ?
    // Standard "coeff" usually means "per frame decay".
    // y[n] = x[n] + coeff * (y[n-1] - x[n]) -> y approaches x.
    // Let's use:
    // delta = in - env
    // env += delta * coeff

    q31_t delta = q31_sub_sat(abs_in, env->envelope);
    q31_t change = q31_mul(delta, env->attack);
    env->envelope = q31_add_sat(env->envelope, change);
  } else {
    // Release
    q31_t delta = q31_sub_sat(abs_in, env->envelope); // Negative
    q31_t change = q31_mul(delta, env->release);
    env->envelope = q31_add_sat(env->envelope, change);
  }

  return env->envelope;
}

// --- Perlin Noise (1D) ---

// Smoothstep interpolation function (3rd order)
static inline q31_t smoothstep(q31_t t) {
  // t^2 * (3 - 2*t) in Q31
  // t is in range [0, Q31_MAX]
  int64_t t_sq = ((int64_t)t * t) >> 31;
  int64_t three = (3LL << 31);
  int64_t two_t = ((int64_t)t << 1);
  int64_t result = (t_sq * (three - two_t)) >> 31;
  
  if (result > Q31_MAX) result = Q31_MAX;
  if (result < Q31_MIN) result = Q31_MIN;
  return (q31_t)result;
}

void dsp_perlin_init(dsp_perlin_t *perlin, uint32_t seed) {
  perlin->seed = seed;
  
  // Initialize gradient table with random values [-1, 1]
  uint32_t rng = seed;
  for (int i = 0; i < PERLIN_TABLE_SIZE; i++) {
    rng = rng * 1664525 + 1013904223;  // LCG
    perlin->gradient_table[i] = (int8_t)(rng >> 24);  // -128..127
  }
  
  // Initialize permutation table (0..255 shuffled)
  for (int i = 0; i < PERLIN_TABLE_SIZE; i++) {
    perlin->perm_table[i] = i;
  }
  
  // Fisher-Yates shuffle
  for (int i = PERLIN_TABLE_SIZE - 1; i > 0; i--) {
    rng = rng * 1664525 + 1013904223;
    int j = rng % (i + 1);
    uint8_t temp = perlin->perm_table[i];
    perlin->perm_table[i] = perlin->perm_table[j];
    perlin->perm_table[j] = temp;
  }
}

q31_t dsp_perlin_1d(dsp_perlin_t *perlin, q31_t t) {
  // t is in Q31 (0..1 range), scale to table size
  // We want to map [0, Q31_MAX] to [0, PERLIN_TABLE_SIZE-1]
  int64_t scaled = ((int64_t)t * (PERLIN_TABLE_SIZE - 1)) >> 31;
  
  // Integer and fractional parts
  int32_t i0 = (int32_t)scaled;
  int32_t i1 = i0 + 1;
  
  // Wrap indices
  i0 = i0 & (PERLIN_TABLE_SIZE - 1);
  i1 = i1 & (PERLIN_TABLE_SIZE - 1);
  
  // Get gradients via permutation table
  uint8_t hash0 = perlin->perm_table[i0];
  uint8_t hash1 = perlin->perm_table[i1];
  
  int8_t grad0 = perlin->gradient_table[hash0];
  int8_t grad1 = perlin->gradient_table[hash1];
  
  // Calculate fractional position (0..1 in Q31)
  q31_t frac = (q31_t)((scaled - i0) << 31);
  
  // Dot products (gradient * distance)
  // grad is int8_t (-128..127), normalize to Q31
  int64_t g0_q31 = ((int64_t)grad0 << 24);  // Scale to Q31 range
  int64_t g1_q31 = ((int64_t)grad1 << 24);
  
  q31_t dot0 = (q31_t)((g0_q31 * (Q31_MAX - frac)) >> 31);
  q31_t dot1 = (q31_t)((g1_q31 * frac) >> 31);
  
  // Interpolate with smoothstep
  q31_t smooth_frac = smoothstep(frac);
  q31_t inv_frac = q31_sub_sat(Q31_MAX, smooth_frac);
  
  q31_t result = q31_add_sat(q31_mul(dot0, inv_frac), q31_mul(dot1, smooth_frac));
  
  return result;
}

