#include "../include/te2350.h"

// --- Internal Constants ---
// Fixed modulation scales to prevent chaos
#define MOD_SCALE_SHIFT 10

static void update_tone_filter(te2350_t *ctx);

bool te2350_init(te2350_t *ctx, void *memory_block, size_t total_bytes, float sample_rate) {
  if (sample_rate <= 0.0f) {
    sample_rate = 48000.0f; // Safe fallback
  }
  ctx->sample_rate = sample_rate;

  // Calculate Sample Rate Derived Parameters
  float sr_ratio = sample_rate / 48000.0f;

  // Smoothing coefficient: want ~5ms response for fast things, maybe slower for time.
  // For smoothing variables per sample, coeff = ~ 1 - exp(-1 / (tau * fs))
  // tau = 0.010s -> coeff ~ 0.00208 at 48k
  // original time_smooth_coeff was ~0.002.
  // Let's use a slightly faster one for non-time params (~10ms)
  // and keep time relatively slow.

  float time_smooth_f = 0.002f * (48000.0f / sample_rate); // ~10-20ms
  ctx->time_smooth_coeff = float_to_q31_safe(time_smooth_f);

  // Buffer length limits (keep max_d within bounds minus modulation headroom)
  ctx->max_delay_samples = TE_MAIN_DELAY_SIZE - 400; // Original
  ctx->min_delay_samples = (int32_t)(100 * sr_ratio);
  if (ctx->min_delay_samples < 1) ctx->min_delay_samples = 1;

  // Pre-calculate wobble modulation parameters (avoids float math in loop)
  ctx->wobble_mod_base = (int32_t)(40 * sr_ratio);
  ctx->wobble_mod_scale = (int32_t)(200 * sr_ratio);

  // Partition memory
  q31_t *mem = (q31_t *)memory_block;
  size_t available_words = total_bytes / sizeof(q31_t);
  size_t offset = 0;

// Helper to grab simplified chunk
#define ALLOC_BUF(size)                                                        \
  if (offset + (size) > available_words)                                       \
    return false;                                                              \
  q31_t *buf_##size = &mem[offset];                                            \
  offset += (size);

  ALLOC_BUF(TE_MAIN_DELAY_SIZE);
  dsp_delay_init(&ctx->main_delay, buf_TE_MAIN_DELAY_SIZE, TE_MAIN_DELAY_SIZE);

  ALLOC_BUF(TE_AP1_SIZE);
  dsp_allpass_init(&ctx->ap1, buf_TE_AP1_SIZE, TE_AP1_SIZE, FLOAT_TO_Q31(0.5f));  // Reduced from 0.6

  ALLOC_BUF(TE_AP2_SIZE);
  dsp_allpass_init(&ctx->ap2, buf_TE_AP2_SIZE, TE_AP2_SIZE, FLOAT_TO_Q31(0.4f));  // Reduced from 0.5

  ALLOC_BUF(TE_AP3_SIZE);
  dsp_allpass_init(&ctx->ap3, buf_TE_AP3_SIZE, TE_AP3_SIZE, FLOAT_TO_Q31(0.3f));  // Reduced from 0.4

  ALLOC_BUF(TE_AP4_SIZE);
  dsp_allpass_init(&ctx->ap4, buf_TE_AP4_SIZE, TE_AP4_SIZE, FLOAT_TO_Q31(0.3f));  // New stage
  
  ALLOC_BUF(TE_PITCH_SIZE);
  dsp_pitch_init(&ctx->pitch_shifter, buf_TE_PITCH_SIZE, TE_PITCH_SIZE);

  // Init modules
  dsp_dc_blocker_init(&ctx->dc_block);

  // SVF: Lowpass, ~2kHz initially
  // F coeff: 2*sin(pi*fc/fs). For 2kHz/48k ~ 0.26.
  dsp_svf_init(&ctx->damping_filter, FLOAT_TO_Q31(0.25f), FLOAT_TO_Q31(0.5f));

  // Modulation
  dsp_rand_walk_init(&ctx->modulator, 12345, FLOAT_TO_Q31(0.0001f));
  // Envelope for ducking: Faster attack, slower release for pumping effect
  dsp_env_init(&ctx->envelope, FLOAT_TO_Q31(0.05f), FLOAT_TO_Q31(0.0005f));
  
  // Init feedback LP filter (~1kHz @ 48kHz) - Slightly darker
  dsp_onepole_init(&ctx->fb_lp, FLOAT_TO_Q31(0.12f));

  // Default parameters
  ctx->p_feedback = FLOAT_TO_Q31(0.9f);
  ctx->p_time = FLOAT_TO_Q31(0.9f);
  ctx->p_rate = FLOAT_TO_Q31(0.5f); // Medium rate
  ctx->p_depth = FLOAT_TO_Q31(0.4f);
  // Default tone should match original dark voicing (0.12f filter coeff)
  // Mapping logic: coeff = tone + (tone >> 3) + 0.005
  // For ~0.12, tone roughly = 0.1f (0.1 + 0.0125 + 0.005 = ~0.1175)
  ctx->p_tone = FLOAT_TO_Q31(0.1f); // Darker tone
  ctx->p_mix = FLOAT_TO_Q31(0.5f);  // 50/50 mix default
  ctx->p_shimmer = 0;                      // No pitch shift by default
  ctx->p_diffusion = FLOAT_TO_Q31(1.0f);   // Full diffusion by default
  ctx->p_chaos = FLOAT_TO_Q31(0.2f);       // Moderate chaos
  ctx->p_ducking = 0;                      // No ducking by default
  ctx->p_wobble = FLOAT_TO_Q31(0.5f);      // Slight wobble by default

  // Smoothed initialization
  ctx->p_time_smoothed = ctx->p_time;
  ctx->p_feedback_smoothed = ctx->p_feedback;
  ctx->p_mix_smoothed = ctx->p_mix;
  ctx->p_tone_smoothed = ctx->p_tone;
  ctx->p_diffusion_smoothed = ctx->p_diffusion;

  // Internal state
  ctx->feedback_state = 0;
  ctx->chaos_seed = 98765;
  
  // Freeze mode
  ctx->freeze_mode = false;
  ctx->freeze_crossfade = 0;
  
  // Multi-tap delays (Prime numbers, spread over larger buffer)
  // Range expanded to ~315ms. Scale by sample rate ratio.
  ctx->tap_delays[0] = (size_t)(2399 * sr_ratio);   // ~50ms
  ctx->tap_delays[1] = (size_t)(5801 * sr_ratio);   // ~120ms
  ctx->tap_delays[2] = (size_t)(9811 * sr_ratio);   // ~205ms
  ctx->tap_delays[3] = (size_t)(15101 * sr_ratio);  // ~315ms (Safe < 16384)
  if (ctx->tap_delays[3] >= TE_MAIN_DELAY_SIZE) ctx->tap_delays[3] = TE_MAIN_DELAY_SIZE - 1; // Sanity check
  
  // Gains: Reduced to prevent clipping (Headroom)
  ctx->tap_gains[0] = FLOAT_TO_Q31(0.20f);
  ctx->tap_gains[1] = FLOAT_TO_Q31(0.30f);
  ctx->tap_gains[2] = FLOAT_TO_Q31(0.35f);
  ctx->tap_gains[3] = FLOAT_TO_Q31(0.30f);
  
  return true;
}

void te2350_process(te2350_t *ctx, q31_t in_mono, q31_t *out_l, q31_t *out_r) {
  // 1. Input Conditioning
  q31_t dry = dsp_dc_blockProcess(&ctx->dc_block, in_mono);

  // 2. Parameter Smoothing (Zipper Noise Elimination)

  // Generic helper macro for one-pole smoothing
  #define SMOOTH_PARAM(target, current, coeff) \
    do { \
      q31_t diff = q31_sub_sat((target), (current)); \
      q31_t delta = q31_mul(diff, (coeff)); \
      if (diff > 0 && delta == 0) delta = 1; \
      if (diff < 0 && delta == 0) delta = -1; \
      (current) = q31_add_sat((current), delta); \
    } while(0)

  // One-pole lowpass filter on delay time for smooth "tape varispeed" effect
  SMOOTH_PARAM(ctx->p_time, ctx->p_time_smoothed, ctx->time_smooth_coeff);

  // Other parameters can use a slightly faster coefficient (e.g. 5x faster)
  q31_t fast_smooth = ctx->time_smooth_coeff * 5;
  if (fast_smooth > FLOAT_TO_Q31(0.1f)) fast_smooth = FLOAT_TO_Q31(0.1f);

  SMOOTH_PARAM(ctx->p_feedback, ctx->p_feedback_smoothed, fast_smooth);
  SMOOTH_PARAM(ctx->p_mix, ctx->p_mix_smoothed, fast_smooth);
  SMOOTH_PARAM(ctx->p_tone, ctx->p_tone_smoothed, fast_smooth);
  SMOOTH_PARAM(ctx->p_diffusion, ctx->p_diffusion_smoothed, fast_smooth);

  // 3. Modulation Update with Chaos
  // Make base step proportional to p_rate (so it controls speed).
  // p_chaos controls random variation of the step size, rather than over-writing it.
  q31_t base_step = (ctx->p_rate >> 6);
  if (base_step < FLOAT_TO_Q31(0.0001f)) base_step = FLOAT_TO_Q31(0.0001f);

  // Add some chaotic jitter to the step size
  // Random multiplier between 1.0 and (1.0 + chaos)
  ctx->chaos_seed = ctx->chaos_seed * 1664525 + 1013904223;
  q31_t random_chaos_factor = (q31_t)(ctx->chaos_seed >> 1); // 0..MAX
  q31_t chaos_mod = q31_mul(random_chaos_factor, ctx->p_chaos); // 0..p_chaos
  
  q31_t current_step = q31_add_sat(base_step, q31_mul(base_step, chaos_mod));
  dsp_rand_walk_set_step(&ctx->modulator, current_step);

  q31_t rnd = dsp_rand_walk_process(&ctx->modulator);

  // Apply user depth
  q31_t mod_scale_factor = 0x03000000; // ~125ms max swing
  q31_t mod_val = q31_mul(rnd, mod_scale_factor);
  mod_val = q31_mul(mod_val, ctx->p_depth);
  
  // 4. Freeze Mode Processing
  // Crossfade between normal and frozen states
  if (ctx->freeze_mode) {
    // Ramp up freeze_crossfade
    ctx->freeze_crossfade = q31_add_sat(ctx->freeze_crossfade, FLOAT_TO_Q31(0.001f));
    if (ctx->freeze_crossfade > Q31_MAX) ctx->freeze_crossfade = Q31_MAX;
  } else {
    // Ramp down
    ctx->freeze_crossfade = q31_sub_sat(ctx->freeze_crossfade, FLOAT_TO_Q31(0.001f));
    if (ctx->freeze_crossfade < 0) ctx->freeze_crossfade = 0;
  }
  
  // Crossfade input: reduce input INTO LOOP during freeze
  q31_t input_gain = q31_sub_sat(Q31_MAX, ctx->freeze_crossfade);
  q31_t loop_feed_dry = q31_mul(dry, input_gain);  // Only mute what goes into the loop!
  
  // NOTE: 'dry' variable remains untouched for output mixing!
  
  // Boost feedback during freeze (approach 0.98)
  q31_t effective_feedback = ctx->p_feedback_smoothed;
  if (ctx->freeze_mode && ctx->freeze_crossfade > 0) {
    q31_t freeze_fb = FLOAT_TO_Q31(0.98f);
    q31_t fb_boost = q31_sub_sat(freeze_fb, effective_feedback);
    fb_boost = q31_mul(fb_boost, ctx->freeze_crossfade);
    effective_feedback = q31_add_sat(effective_feedback, fb_boost);
  }

  // 5. Multi-Tap Injection (Pre-diffusion) - BOSS STYLE
  // Taps read from main delay and injected BEFORE allpasses
  q31_t multi_tap = 0;
  for (int i = 0; i < TE_NUM_TAPS; i++) {
    q31_t tap = dsp_delay_read(&ctx->main_delay, ctx->tap_delays[i]);
    // Gains reduzidos drasticamente (-18dB range) para evitar explosão
    // Shift >> 3 divide por 8, mais os gains originais que já eram < 1.0
    q31_t tap_scaled = q31_mul(tap, ctx->tap_gains[i] >> 3); 
    multi_tap = q31_add_sat(multi_tap, tap_scaled);
  }

  // 6. Feedback Loop Injection
  q31_t fb_signal = ctx->feedback_state;
  q31_t loop_in = q31_add_sat(loop_feed_dry, fb_signal); // Use loop_feed_dry here
  loop_in = q31_add_sat(loop_in, multi_tap); // Inject taps into diffusion chain

  // 6. Diffusion (Modulated Allpasses) - BOSS STYLE
  // Diffusion parameter dynamically scales allpass gains and modulation depths
  // Lower diffusion = distinct articulated echoes
  // High diffusion = dense ambient cloud
  
  // Base fixed gains (scaled by p_diffusion_smoothed)
  q31_t diff_gain_ap1 = q31_mul(FLOAT_TO_Q31(0.55f), ctx->p_diffusion_smoothed);
  q31_t diff_gain_ap2 = q31_mul(FLOAT_TO_Q31(0.50f), ctx->p_diffusion_smoothed);
  q31_t diff_gain_ap3 = q31_mul(FLOAT_TO_Q31(0.40f), ctx->p_diffusion_smoothed);
  q31_t diff_gain_ap4 = q31_mul(FLOAT_TO_Q31(0.40f), ctx->p_diffusion_smoothed);

  ctx->ap1.gain = diff_gain_ap1;
  ctx->ap2.gain = diff_gain_ap2;
  ctx->ap3.gain = diff_gain_ap3;
  ctx->ap4.gain = diff_gain_ap4;

  // Modulate diffusion allpass sizes slightly less if diffusion is low
  q31_t diff_mod_val = q31_mul(mod_val, ctx->p_diffusion_smoothed);

  // --- AP1 Modulation ---
  int32_t temp_d1 = ((int32_t)(TE_AP1_SIZE / 2) << 16) + diff_mod_val;
  if (temp_d1 < 0x10000)
    temp_d1 = 0x10000;
  if (temp_d1 > ((TE_AP1_SIZE - 2) << 16))
    temp_d1 = ((TE_AP1_SIZE - 2) << 16);
  q16_16_t ap1_d = (q16_16_t)temp_d1;

  // --- AP2 Modulation (Inverse for decorrelation) ---
  int32_t temp_d2 = ((int32_t)(TE_AP2_SIZE / 2) << 16) - diff_mod_val;
  if (temp_d2 < 0x10000)
    temp_d2 = 0x10000;
  if (temp_d2 > ((TE_AP2_SIZE - 2) << 16))
    temp_d2 = ((TE_AP2_SIZE - 2) << 16);
  q16_16_t ap2_d = (q16_16_t)temp_d2;

  q31_t stage1 = dsp_allpass_process(&ctx->ap1, loop_in, ap1_d);
  q31_t stage2 = dsp_allpass_process(&ctx->ap2, stage1, ap2_d);
  
  // --- AP4 Modulation (Slower/Decorrelated) ---
  // Simple fixed modulation offset for now or reused mod
  int32_t temp_d4 = ((int32_t)(TE_AP4_SIZE / 2) << 16) + (diff_mod_val >> 1);
  if (temp_d4 < 0x10000) temp_d4 = 0x10000;
  if (temp_d4 > ((TE_AP4_SIZE - 2) << 16)) temp_d4 = ((TE_AP4_SIZE - 2) << 16);
  q31_t stage3 = dsp_allpass_process(&ctx->ap4, stage2, (q16_16_t)temp_d4);
  
  // 7. Main Delay with Modulation (for shimmer effect)
  // Write to main delay
  dsp_delay_write(&ctx->main_delay, stage3);

  // Read from main delay using SMOOTHED time parameter + modulation
  // Convert smoothed Q31 time (0..1) to sample count
  int32_t d_samp_int = (int32_t)(((int64_t)ctx->p_time_smoothed * ctx->max_delay_samples) >> 31);
  if (d_samp_int < ctx->min_delay_samples)
    d_samp_int = ctx->min_delay_samples;
  
  // Add modulation to main delay
  // Base mod is subtle (±40 samples at 48k), but wobble parameter increases this significantly
  // This gives wobble a clearly audible tape wow/flutter effect
  int32_t wobble_mod_samples = ctx->wobble_mod_base + (int32_t)(((int64_t)ctx->p_wobble * ctx->wobble_mod_scale) >> 31);
  int32_t main_delay_mod = (int32_t)(((int64_t)mod_val * wobble_mod_samples) >> 31);
  int32_t d_samp_modulated = d_samp_int + main_delay_mod;
  if (d_samp_modulated < ctx->min_delay_samples)
    d_samp_modulated = ctx->min_delay_samples;
  if (d_samp_modulated > ctx->max_delay_samples)
    d_samp_modulated = ctx->max_delay_samples;

  // Use Hermite interpolation for highest quality (reduced artifacts)
  q16_16_t delay_samples_frac = (q16_16_t)d_samp_modulated << 16;
  q31_t delay_out = dsp_delay_read_hermite(&ctx->main_delay, delay_samples_frac);

  // 8. Feedback Processing
  // TONE / FILTER (One-Pole Conservative Tilt EQ)
  
  q31_t filter_out;
  q31_t feedback_gain_final;
  q31_t fb_processed;

  // FREEZE MODE - "Lossless Infinity" Logic
  if (ctx->freeze_mode && ctx->freeze_crossfade > 0x7F000000) { 
      // WHEN FROZEN (>99%):
      // 1. Bypass Filter (Prevent spectral decay)
      filter_out = delay_out;
      
      // 2. Bypass Saturation/Pitch (Prevent degradation)
      fb_processed = filter_out;
      
      // 3. Force Unity Gain (Infinite Hold)
      // Q31_MAX is slightly less than 1.0, but close enough for long holds.
      // Ideally we want 1.0, effectively just writing output back to input.
      feedback_gain_final = Q31_MAX;
      
  } else {
      // NORMAL MODE (or Transition):
      
      // Update tone filter coefficients based on smoothed tone
      update_tone_filter(ctx);

      // Apply Tone Filter
      q31_t lp = dsp_onepole_lp(&ctx->fb_lp, delay_out);
      if (ctx->p_tone_smoothed > FLOAT_TO_Q31(0.55f)) {
          filter_out = q31_sub_sat(delay_out, lp); // HP
      } else {
          filter_out = lp; // LP
      }
      
      // Apply Saturation (Gentle)
      q31_t fb_sat = dsp_soft_saturate_gentle(filter_out);
      
      // Apply Pitch Smear (Continuous control via p_shimmer)
      q31_t fb_with_smear = fb_sat;
      if (ctx->p_shimmer > 0) {
        // Smear amount scaled by shimmer
        q31_t smear_amount = q31_mul(rnd, q31_mul(ctx->p_shimmer, FLOAT_TO_Q31(0.005f)));
        q31_t pitched_fb = dsp_pitch_process(&ctx->pitch_shifter, fb_sat, smear_amount);

        // Crossfade between unshifted and shifted feedback using p_shimmer
        // Map p_shimmer (0..1) to mix (0..0.4 max pitched contribution to prevent chaos)
        q31_t pitch_mix = q31_mul(ctx->p_shimmer, FLOAT_TO_Q31(0.4f));
        q31_t dry_mix = q31_sub_sat(Q31_MAX, pitch_mix);

        fb_with_smear = q31_add_sat(q31_mul(fb_sat, dry_mix),
                                    q31_mul(pitched_fb, pitch_mix));
      }
      fb_processed = fb_with_smear;
      
      // Apply Wobble to Feedback Amount
      // Give wobble a stronger effect on feedback instability
      q31_t fb_wobble = q31_mul(rnd, ctx->p_wobble);
      fb_wobble = q31_mul(fb_wobble, FLOAT_TO_Q31(0.25f));
      q31_t mod_fb = q31_add_sat(effective_feedback, fb_wobble);
      
      // Clamp - INCREASED MAX FEEDBACK to allow near-oscillation
      // 0.99f or slightly higher gives beautiful infinite wash, but keep under 1.0 to stay somewhat sane
      if (mod_fb > FLOAT_TO_Q31(0.995f)) mod_fb = FLOAT_TO_Q31(0.995f);
      if (mod_fb < 0) mod_fb = 0;
      
      feedback_gain_final = mod_fb;
  }
  
  ctx->feedback_state = q31_mul(fb_processed, feedback_gain_final);
  
  // 9. Multi-Tap Delay moved to Input (Pre-diffusion)
  
  // Blend main delay output (70%) - Taps removed from here (moved to input)
  q31_t wet = delay_out;
  
  // NOTE: Pitch shifter agora está APENAS no feedback loop (pitch smear sutil)
  // Isso dá movimento sem "chorus audível" no caminho direto
  
  // 11. Envelope Ducking
  // 11. Envelope Ducking
  // Reduce wet signal when input is strong
  // Always run the envelope so it tracks correctly
  q31_t env_level = dsp_env_process(&ctx->envelope, q31_abs(dry));

  if (ctx->p_ducking > 0) {
    // We want stronger ducking effect.
    // Amplify the envelope so it hits maximum ducking easier (approx 12dB boost / x4)
    // Shift left by 2 to multiply by 4. Use saturate to avoid overflow wrap-around.
    q31_t amp_env;
    if (env_level > (Q31_MAX >> 2)) {
      amp_env = Q31_MAX;
    } else {
      amp_env = env_level << 2;
    }
    
    // Invert envelope: more input = less wet
    q31_t duck_amount = q31_sub_sat(Q31_MAX, amp_env);
    
    // Scale ducking effect by parameter.
    // If ducking is 1.0, duck_amount goes to 0 on loud inputs.
    // The final gain is 1.0 - (duck_amount_reduction)
    q31_t reduction = q31_sub_sat(Q31_MAX, duck_amount);
    reduction = q31_mul(reduction, ctx->p_ducking);

    q31_t final_duck_gain = q31_sub_sat(Q31_MAX, reduction);

    wet = q31_mul(wet, final_duck_gain);
  }

  // 12. Output Mixing & Decorrelation
  // Right channel: decorrelated through AP3
  q31_t wet_r = dsp_allpass_process(&ctx->ap3, wet, (TE_AP3_SIZE / 2) << 16);

  // Variable Dry/Wet Mix (controlled by p_mix_smoothed parameter)
  q31_t dry_gain = q31_sub_sat(Q31_MAX, ctx->p_mix_smoothed);
  q31_t wet_gain = ctx->p_mix_smoothed;

  *out_l = q31_add_sat(q31_mul(dry, dry_gain), q31_mul(wet, wet_gain));
  *out_r = q31_add_sat(q31_mul(dry, dry_gain), q31_mul(wet_r, wet_gain));
}

// --- Getters ---
q31_t te2350_get_envelope(te2350_t *ctx) {
  return ctx->envelope.envelope;
}

q31_t te2350_get_modulator(te2350_t *ctx) {
  return ctx->modulator.current_value;
}

// --- Parameter Setters ---
void te2350_set_feedback(te2350_t *ctx, q31_t feedback) {
  ctx->p_feedback = feedback;
}

void te2350_set_time(te2350_t *ctx, q31_t time) { 
  ctx->p_time = time; 
  // Note: p_time_smoothed will slew towards this value automatically
}

void te2350_set_mod(te2350_t *ctx, q31_t rate, q31_t depth) {
  ctx->p_rate = rate;
  ctx->p_depth = depth;
  // Step size is now dynamically calculated in process loop based on rate and chaos
}

void te2350_set_tone(te2350_t *ctx, q31_t tone) {
  ctx->p_tone = tone;
}

// Helper to update tone filter coefficient from smoothed tone parameter
static void update_tone_filter(te2350_t *ctx) {
  q31_t tone = ctx->p_tone_smoothed;
  
  // One-Pole Filter Coefficients
  // Increased range for darker lows and thinner highs
  // Range 0..0.45: Lowpass (Very Dark -> Open)
  // Range 0.55..1: Highpass (Open -> Very Thin)
  
  if (tone < FLOAT_TO_Q31(0.45f)) {
      // Lowpass: Coeff 0.005 (Very Dark) to 0.5 (Open)
      q31_t c = q31_add_sat(tone, tone >> 3);
      c = q31_add_sat(c, FLOAT_TO_Q31(0.005f));
      if (c > FLOAT_TO_Q31(0.5f)) c = FLOAT_TO_Q31(0.5f);
      ctx->fb_lp.coeff = c;
  } else if (tone > FLOAT_TO_Q31(0.55f)) {
      // Highpass: Coeff 0.02 (Open) to 0.7 (Very Thin)
      q31_t t = q31_sub_sat(tone, FLOAT_TO_Q31(0.55f));
      q31_t c = q31_add_sat(t, t >> 1); // Steeper curve (x1.5)
      c = q31_add_sat(c, FLOAT_TO_Q31(0.02f));
      if (c > FLOAT_TO_Q31(0.7f)) c = FLOAT_TO_Q31(0.7f);
      ctx->fb_lp.coeff = c;
  } else {
      // Neutral: LP Open (0.5)
      ctx->fb_lp.coeff = FLOAT_TO_Q31(0.5f);
  }
}

void te2350_set_mix(te2350_t *ctx, q31_t mix) {
  ctx->p_mix = mix;
}

void te2350_set_freeze(te2350_t *ctx, bool freeze) {
  ctx->freeze_mode = freeze;
}

void te2350_set_shimmer(te2350_t *ctx, q31_t shimmer) {
  ctx->p_shimmer = shimmer;
}

void te2350_set_diffusion(te2350_t *ctx, q31_t diffusion) {
  ctx->p_diffusion = diffusion;
}

void te2350_set_chaos(te2350_t *ctx, q31_t chaos) {
  ctx->p_chaos = chaos;
}

void te2350_set_ducking(te2350_t *ctx, q31_t ducking) {
  ctx->p_ducking = ducking;
}

void te2350_set_wobble(te2350_t *ctx, q31_t wobble) {
  ctx->p_wobble = wobble;
}
