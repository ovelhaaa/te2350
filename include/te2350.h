#ifndef TE_2350_H
#define TE_2350_H

#include "dsp_delay.h"
#include "dsp_filters.h"
#include "dsp_math.h"
#include "dsp_modulation.h"
#include "dsp_pitch.h"
#include "dsp_melody.h"

// --- Tuning Constants ---
// Just defaults or limits.
// 48kHz -> 100ms = 4800 samples.
// Main delay size is configurable at compile time:
//   16384 @ 48kHz ≈ 0.34s
//   32768 @ 48kHz ≈ 0.68s (default)
//   49152 @ 48kHz ≈ 1.02s (experimental: test SRAM/stack/DMA safety first)
// WARNING (RP2350): larger delay buffers increase SRAM pressure and can reduce
// headroom for stack, DMA buffers, and other runtime structures. If pushed too
// far this can increase underrun risk.
// Allpasses: 2-3 short ones.
// Total ~76KB + Pitch. Safe in 128KB.

#ifndef TE_MAIN_DELAY_SIZE
#define TE_MAIN_DELAY_SIZE 32768
#endif
#define TE_AP1_SIZE 1103  // Prime number to avoid ringing
#define TE_AP2_SIZE 1327  // Prime number
#define TE_AP3_SIZE 673   // Prime number
#define TE_AP4_SIZE 977   // Prime number (New stage)
#define TE_SIDE_AP_SIZE 191 // Short prime for side-only decorrelation
#define TE_PITCH_SIZE 2048  // Pitch shifter buffer
#define TE_OCTAVE_PITCH_SIZE 2048 // Pitch shifter for octave feedback

typedef struct {
  // --- Components ---
  dsp_dc_blocker_t dc_block;

  // Diffusion Chain
  dsp_allpass_t ap1;
  dsp_allpass_t ap2;
  dsp_allpass_t ap3;
  dsp_allpass_t ap4; // New stage // Decorrelator?
  dsp_allpass_t side_ap; // Dedicated side-only decorrelator

  // Main Echo Loop
  dsp_delay_t main_delay;
  dsp_svf_t damping_filter;

  // Modulation
  dsp_random_walk_t space_mod; // Spatial diffusion drift
  dsp_random_walk_t time_mod;  // Main delay drift (subtle)
  dsp_env_follower_t envelope; // Dynamics
  
  // Feedback filtering
  dsp_onepole_t fb_lp;  // 1-pole LP for feedback (~1.2kHz)
  dsp_onepole_t fb_hp;  // 1-pole HP helper for low-end cleanup
  dsp_dc_blocker_t fb_dc; // DC control in loop

  // Presence rail (post-loop only)
  dsp_onepole_t presence_hp;    // HP helper via src - LP(src)
  dsp_onepole_t presence_lp;    // final LP for harshness control
  q31_t presence_gain_smooth;   // optional smoothing for rail gain

  // Shimmer voicing (parallel branch only)
  dsp_onepole_t shimmer_hp;     // HP helper via src - LP(src)
  dsp_onepole_t shimmer_lp;     // soft LP to keep halo airy without harsh top
  
  // Pitch shifter
  dsp_pitch_shifter_t pitch_shifter;
  dsp_pitch_shifter_t octave_shifter;

  // --- State ---
  q31_t feedback_state;
  q31_t bloom_state;
  
  // Freeze mode state
  bool freeze_mode;
  q31_t freeze_crossfade;  // 0 = normal, Q31_MAX = frozen

  // Melody Generator
  bool p_melody_enabled;
  bool p_melody_only;
  dsp_melody_t melody;

  // --- Parameters (Q31) ---
  // Targets
  bool octave_feedback_enabled;
  q31_t octave_feedback_amount; // Amount 0..1
  q31_t p_feedback;
  q31_t p_time;          // Main delay time target (0..1 -> maps to ms)
  q31_t p_rate;   // Modulation rate
  q31_t p_depth;  // Modulation depth
  q31_t p_tone;   // Damping cutoff
  q31_t p_mix;    // Dry/Wet mix (0 = 100% dry, Q31_MAX = 100% wet)
  q31_t p_shimmer;    // Pitch shift amount (0 = no shift, Q31_MAX = +1 octave)
  q31_t p_diffusion;  // Allpass diffusion amount (0 = off, Q31_MAX = full)
  q31_t p_chaos;      // Chaos/instability amount (0 = stable, Q31_MAX = chaotic)
  q31_t p_ducking;    // Envelope ducking amount (0 = off, Q31_MAX = full duck)
  q31_t p_wobble;     // Feedback wobble amount (0 = stable, Q31_MAX = wobbly)
  q31_t p_presence;   // Presence rail gain (0 = soft, Q31_MAX = articulate)
  q31_t p_space_gravity; // Internal macro behavior (derived from existing params)

  // Smoothed runtime values
  q31_t p_time_smoothed;
  q31_t p_feedback_smoothed;
  q31_t p_mix_smoothed;
  q31_t p_tone_smoothed;
  q31_t p_diffusion_smoothed;
  q31_t p_presence_smoothed;
  q31_t p_space_gravity_smoothed;
  
  // Internal State
  uint32_t chaos_seed;
  float sample_rate; // Operating sample rate

  // Derived Sample-Rate Coefficients
  int32_t max_delay_samples; // Scaled max main delay samples
  int32_t min_delay_samples; // Scaled min main delay samples
  q31_t time_smooth_coeff; // Sample-rate adjusted smoothing speed
  int32_t wobble_mod_base; // Base modulation samples for wobble
  int32_t wobble_mod_scale; // Scale modulation samples for wobble

  // Time mapping LUT for perceptual control response
  #define TE_TIME_LUT_SIZE 128
  uint16_t time_lut[TE_TIME_LUT_SIZE];

  // Multi-tap cloud parameters
  #define TE_NUM_EARLY_TAPS 6
  #define TE_NUM_LATE_TAPS 4
  size_t early_tap_delays[TE_NUM_EARLY_TAPS];
  q31_t early_tap_gains[TE_NUM_EARLY_TAPS];
  size_t late_tap_delays[TE_NUM_LATE_TAPS];
  q31_t late_tap_gains[TE_NUM_LATE_TAPS];

} te2350_t;

/**
 * @brief Initialize the TE-2350 effect types.
 *
 * @param ctx Effect context.
 * @param memory_block Pointer to a single large memory block (at least 64KB
 * recommended).
 * @param size Size of the memory block in bytes.
 * @param sample_rate The operating sample rate (e.g., 48000.0f).
 * @return true if successful, false if memory too small.
 */
bool te2350_init(te2350_t *ctx, void *memory_block, size_t size, float sample_rate);

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
void te2350_set_melody_enabled(te2350_t *ctx, bool enabled);
void te2350_set_melody_only(te2350_t *ctx, bool only);
void te2350_set_melody_volume(te2350_t *ctx, q31_t volume);
void te2350_set_melody_density(te2350_t *ctx, q31_t density);
void te2350_set_melody_decay(te2350_t *ctx, q31_t decay);
void te2350_set_octave_feedback_enabled(te2350_t *ctx, bool enabled);
void te2350_set_octave_feedback_amount(te2350_t *ctx, q31_t amount);
void te2350_set_shimmer(te2350_t *ctx, q31_t shimmer);   // 0..1 (pitch shift amount)
void te2350_set_diffusion(te2350_t *ctx, q31_t diffusion); // 0..1 (allpass diffusion)
void te2350_set_chaos(te2350_t *ctx, q31_t chaos);       // 0..1 (modulation chaos)
void te2350_set_ducking(te2350_t *ctx, q31_t ducking);   // 0..1 (envelope ducking)
void te2350_set_wobble(te2350_t *ctx, q31_t wobble);     // 0..1 (feedback wobble)
void te2350_set_presence(te2350_t *ctx, q31_t presence); // 0..1 (presence rail blend)

#endif // TE_2350_H
