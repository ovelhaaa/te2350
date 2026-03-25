#ifndef TE_2350_H
#define TE_2350_H

#include "dsp_delay.h"
#include "dsp_filters.h"
#include "dsp_math.h"
#include "dsp_modulation.h"
#include "dsp_pitch.h"


// --- Tuning Constants ---
// Just defaults or limits.
// 48kHz -> 100ms = 4800 samples.
// We need ~64KB total memory.
// Main Delay: 64KB (16384 words * 4 bytes). ~340ms at 48k.
// Allpasses: 2-3 short ones.
// Total ~76KB + Pitch. Safe in 128KB.

#define TE_MAIN_DELAY_SIZE 16384 // Doubled for "bleeps" and texture
#define TE_AP1_SIZE 1103  // Prime number to avoid ringing
#define TE_AP1_SIZE 1103  // Prime number to avoid ringing
#define TE_AP2_SIZE 1327  // Prime number
#define TE_AP3_SIZE 673   // Prime number
#define TE_AP4_SIZE 977   // Prime number (New stage)
#define TE_PITCH_SIZE 2048  // Pitch shifter buffer

typedef struct {
  // --- Components ---
  dsp_dc_blocker_t dc_block;

  // Diffusion Chain
  dsp_allpass_t ap1;
  dsp_allpass_t ap2;
  dsp_allpass_t ap3;
  dsp_allpass_t ap4; // New stage // Decorrelator?

  // Main Echo Loop
  dsp_delay_t main_delay;
  dsp_svf_t damping_filter;

  // Modulation
  dsp_random_walk_t modulator; // Slow drift
  dsp_env_follower_t envelope; // Dynamics
  
  // Feedback filtering
  dsp_onepole_t fb_lp;  // 1-pole LP for feedback (~1.2kHz)
  
  // Pitch shifter
  dsp_pitch_shifter_t pitch_shifter;

  // --- State ---
  q31_t feedback_state;
  
  // Freeze mode state
  bool freeze_mode;
  q31_t freeze_crossfade;  // 0 = normal, Q31_MAX = frozen

  // --- Parameters (Q31) ---
  q31_t p_feedback;
  q31_t p_time;          // Main delay time target (0..1 -> maps to ms)
  q31_t p_time_smoothed; // Smoothed delay time (eliminates zipper noise)
  q31_t p_rate;   // Modulation rate
  q31_t p_depth;  // Modulation depth
  q31_t p_tone;   // Damping cutoff
  q31_t p_spread; // Stereo spread
  q31_t p_mix;    // Dry/Wet mix (0 = 100% dry, Q31_MAX = 100% wet)
  
  // New parameters
  q31_t p_shimmer;    // Pitch shift amount (0 = no shift, Q31_MAX = +1 octave)
  q31_t p_diffusion;  // Allpass diffusion amount (0 = off, Q31_MAX = full)
  q31_t p_chaos;      // Chaos/instability amount (0 = stable, Q31_MAX = chaotic)
  q31_t p_ducking;    // Envelope ducking amount (0 = off, Q31_MAX = full duck)
  q31_t p_wobble;     // Feedback wobble amount (0 = stable, Q31_MAX = wobbly)
  
  // Multi-tap delay parameters
  #define TE_NUM_TAPS 4
  size_t tap_delays[TE_NUM_TAPS];
  q31_t tap_gains[TE_NUM_TAPS];

} te2350_t;

/**
 * @brief Initialize the TE-2350 effect types.
 *
 * @param ctx Effect context.
 * @param memory_block Pointer to a single large memory block (at least 64KB
 * recommended).
 * @param size Size of the memory block in bytes.
 * @return true if successful, false if memory too small.
 */
bool te2350_init(te2350_t *ctx, void *memory_block, size_t size);

/**
 * @brief Process one audio frame (Mono In -> Stereo Out).
 *
 * @param ctx Effect context.
 * @param in_mono Input sample.
 * @param out_l Pointer to left output.
 * @param out_r Pointer to right output.
 */
void te2350_process(te2350_t *ctx, q31_t in_mono, q31_t *out_l, q31_t *out_r);

// Getters
q31_t te2350_get_envelope(te2350_t *ctx);
q31_t te2350_get_modulator(te2350_t *ctx);

// Parameter Setters
void te2350_set_feedback(te2350_t *ctx, q31_t feedback); // 0..~0.95
void te2350_set_time(te2350_t *ctx, q31_t time);         // 0..1
void te2350_set_mod(te2350_t *ctx, q31_t rate, q31_t depth);
void te2350_set_tone(te2350_t *ctx, q31_t tone);
void te2350_set_mix(te2350_t *ctx, q31_t mix);           // 0..1 (dry..wet)
void te2350_set_freeze(te2350_t *ctx, bool freeze);      // Enable/disable freeze

// New parameter setters
void te2350_set_shimmer(te2350_t *ctx, q31_t shimmer);   // 0..1 (pitch shift amount)
void te2350_set_diffusion(te2350_t *ctx, q31_t diffusion); // 0..1 (allpass diffusion)
void te2350_set_chaos(te2350_t *ctx, q31_t chaos);       // 0..1 (modulation chaos)
void te2350_set_ducking(te2350_t *ctx, q31_t ducking);   // 0..1 (envelope ducking)
void te2350_set_wobble(te2350_t *ctx, q31_t wobble);     // 0..1 (feedback wobble)

#endif // TE_2350_H
