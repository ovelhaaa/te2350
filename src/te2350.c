#include "../include/te2350.h"

// --- Internal Constants ---
// Fixed modulation scales to prevent chaos
#define MOD_SCALE_SHIFT 10

// Parameter smoothing coefficient for time (one-pole lowpass)
// ~0.002 gives smooth "tape varispeed" effect (~5ms response at 48kHz block rate)
#define TIME_SMOOTH_COEFF ((q31_t)0x00418937) // ~0.002 in Q31

// Minimum delay in samples (prevents feedback explosion at very short delays)
#define MIN_DELAY_SAMPLES 100

bool te2350_init(te2350_t *ctx, void *memory_block, size_t total_bytes) {
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
  // Smoother envelope: Attack 0.01 -> 0.005 to reduce "thump"
  dsp_env_init(&ctx->envelope, FLOAT_TO_Q31(0.005f), FLOAT_TO_Q31(0.001f));
  
  // Init feedback LP filter (~1kHz @ 48kHz) - Slightly darker
  dsp_onepole_init(&ctx->fb_lp, FLOAT_TO_Q31(0.12f));

  // Defaults
  ctx->p_feedback = FLOAT_TO_Q31(0.9f);
  ctx->p_time = FLOAT_TO_Q31(0.9);
  ctx->p_time_smoothed = FLOAT_TO_Q31(0.5f); // Initialize to match target
  ctx->p_depth = FLOAT_TO_Q31(0.4f);
  ctx->p_mix = FLOAT_TO_Q31(0.5f);  // 50/50 mix default
  ctx->feedback_state = 0;
  
  // Freeze mode
  ctx->freeze_mode = false;
  ctx->freeze_crossfade = 0;
  
  // Multi-tap delays (Prime numbers for less rhythmic repeats)
  // ~37ms, ~83ms, ~137ms, ~199ms @ 48kHz
  // Multi-tap delays (Prime numbers, spread over larger buffer)
  // Range expanded to ~315ms
  ctx->tap_delays[0] = 2399;   // ~50ms
  ctx->tap_delays[1] = 5801;   // ~120ms
  ctx->tap_delays[2] = 9811;   // ~205ms
  ctx->tap_delays[3] = 15101;  // ~315ms (Safe < 16384)
  
  // Gains: Reduced to prevent clipping (Headroom)
  ctx->tap_gains[0] = FLOAT_TO_Q31(0.20f);
  ctx->tap_gains[1] = FLOAT_TO_Q31(0.30f);
  ctx->tap_gains[2] = FLOAT_TO_Q31(0.35f);
  ctx->tap_gains[3] = FLOAT_TO_Q31(0.30f);
  
  // New parameters (defaults)
  ctx->p_shimmer = 40;                      // No pitch shift by default
  ctx->p_diffusion = FLOAT_TO_Q31(1.0f);   // Full diffusion by default
  ctx->p_chaos = FLOAT_TO_Q31(0.2f);       // Moderate chaos
  ctx->p_ducking = 0;                      // No ducking by default
  ctx->p_wobble = FLOAT_TO_Q31(0.5f);      // Slight wobble by default

  return true;
}

void te2350_process(te2350_t *ctx, q31_t in_mono, q31_t *out_l, q31_t *out_r) {
  // 1. Input Conditioning
  q31_t dry = dsp_dc_blockProcess(&ctx->dc_block, in_mono);

  // 2. Parameter Smoothing (Zipper Noise Elimination)
  // One-pole lowpass filter on delay time for smooth "tape varispeed" effect
  {
    q31_t diff = q31_sub_sat(ctx->p_time, ctx->p_time_smoothed);
    q31_t delta = q31_mul(diff, TIME_SMOOTH_COEFF);
    // Ensure we always make progress towards target (avoid stuck due to rounding)
    if (diff > 0 && delta == 0) delta = 1;
    if (diff < 0 && delta == 0) delta = -1;
    ctx->p_time_smoothed = q31_add_sat(ctx->p_time_smoothed, delta);
  }

  // 3. Modulation Update with Chaos
  q31_t rnd = dsp_rand_walk_process(&ctx->modulator);
  
  // Apply chaos: increase random walk step size based on p_chaos
  q31_t chaos_step = q31_mul(FLOAT_TO_Q31(0.01f), ctx->p_chaos);
  chaos_step = q31_add_sat(FLOAT_TO_Q31(0.001f), chaos_step);  // Base + chaos
  dsp_rand_walk_set_step(&ctx->modulator, chaos_step);

  // INCREASED modulation scale for deeper effect (was 0x00F00000 = ~240 samples = 5ms)
  // Now: 0x03000000 = ~6000 samples = ~125ms @ 48kHz for true Tera Echo character
  q31_t mod_scale_factor = 0x03000000; // ~125ms max swing
  q31_t mod_val = q31_mul(rnd, mod_scale_factor);
  mod_val = q31_mul(mod_val, ctx->p_depth); // Apply user depth
  
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
  q31_t effective_feedback = ctx->p_feedback;
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
  // Allpass gains FIXOS (não modulados por envelope)
  // Damping interno já está implementado no dsp_allpass_process
  
  // Ganhos fixos Boss-style (Levemente reduzidos para evitar clipping)
  ctx->ap1.gain = FLOAT_TO_Q31(0.55f);
  ctx->ap2.gain = FLOAT_TO_Q31(0.5f);
  ctx->ap3.gain = FLOAT_TO_Q31(0.4f);
  ctx->ap4.gain = FLOAT_TO_Q31(0.4f); // New stage gain

  // --- AP1 Modulation ---
  int32_t temp_d1 = ((int32_t)(TE_AP1_SIZE / 2) << 16) + mod_val;
  if (temp_d1 < 0x10000)
    temp_d1 = 0x10000;
  if (temp_d1 > ((TE_AP1_SIZE - 2) << 16))
    temp_d1 = ((TE_AP1_SIZE - 2) << 16);
  q16_16_t ap1_d = (q16_16_t)temp_d1;

  // --- AP2 Modulation (Inverse for decorrelation) ---
  int32_t temp_d2 = ((int32_t)(TE_AP2_SIZE / 2) << 16) - mod_val;
  if (temp_d2 < 0x10000)
    temp_d2 = 0x10000;
  if (temp_d2 > ((TE_AP2_SIZE - 2) << 16))
    temp_d2 = ((TE_AP2_SIZE - 2) << 16);
  q16_16_t ap2_d = (q16_16_t)temp_d2;

  q31_t stage1 = dsp_allpass_process(&ctx->ap1, loop_in, ap1_d);
  q31_t stage2 = dsp_allpass_process(&ctx->ap2, stage1, ap2_d);
  
  // --- AP4 Modulation (Slower/Decorrelated) ---
  // Simple fixed modulation offset for now or reused mod
  int32_t temp_d4 = ((int32_t)(TE_AP4_SIZE / 2) << 16) + (mod_val >> 1);
  if (temp_d4 < 0x10000) temp_d4 = 0x10000;
  if (temp_d4 > ((TE_AP4_SIZE - 2) << 16)) temp_d4 = ((TE_AP4_SIZE - 2) << 16);
  q31_t stage3 = dsp_allpass_process(&ctx->ap4, stage2, (q16_16_t)temp_d4);
  
  // 7. Main Delay with Modulation (for shimmer effect)
  // Write to main delay
  dsp_delay_write(&ctx->main_delay, stage3);

  // Read from main delay using SMOOTHED time parameter + modulation
  // Use larger buffer range (minus headroom for modulation)
  size_t max_d = TE_MAIN_DELAY_SIZE - 400;
  
  // Convert smoothed Q31 time (0..1) to sample count
  int32_t d_samp_int = (int32_t)(((int64_t)ctx->p_time_smoothed * max_d) >> 31);
  if (d_samp_int < MIN_DELAY_SAMPLES)
    d_samp_int = MIN_DELAY_SAMPLES;
  
  // Add modulation to main delay - REDUZIDO para remover chorus audível
  // Boss Tera Echo: ±40 samples (~0.8ms) em vez de ±200 samples
  int32_t main_delay_mod = (int32_t)(((int64_t)mod_val * 40) >> 31); // ±40 samples
  int32_t d_samp_modulated = d_samp_int + main_delay_mod;
  if (d_samp_modulated < MIN_DELAY_SAMPLES)
    d_samp_modulated = MIN_DELAY_SAMPLES;
  if (d_samp_modulated > (int32_t)max_d)
    d_samp_modulated = max_d;

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
      
      // Apply Tone Filter
      q31_t lp = dsp_onepole_lp(&ctx->fb_lp, delay_out);
      if (ctx->p_tone > FLOAT_TO_Q31(0.55f)) {
          filter_out = q31_sub_sat(delay_out, lp); // HP
      } else {
          filter_out = lp; // LP
      }
      
      // Apply Saturation (Gentle)
      q31_t fb_sat = dsp_soft_saturate_gentle(filter_out);
      
      // Apply Pitch Smear (if enabled)
      q31_t fb_with_smear = fb_sat;
      if (ctx->p_shimmer > FLOAT_TO_Q31(0.3f)) { 
        q31_t smear_amount = q31_mul(rnd, FLOAT_TO_Q31(0.0017f)); 
        q31_t pitched_fb = dsp_pitch_process(&ctx->pitch_shifter, fb_sat, smear_amount);
        fb_with_smear = q31_add_sat(q31_mul(fb_sat, FLOAT_TO_Q31(0.95f)),
                                     q31_mul(pitched_fb, FLOAT_TO_Q31(0.05f)));
      }
      fb_processed = fb_with_smear;
      
      // Apply Wobble and Feedback Amount
      q31_t fb_wobble = q31_mul(rnd, ctx->p_wobble);
      fb_wobble = q31_mul(fb_wobble, FLOAT_TO_Q31(0.15f));
      q31_t mod_fb = q31_add_sat(effective_feedback, fb_wobble);
      
      // Clamp
      if (mod_fb > FLOAT_TO_Q31(0.98f)) mod_fb = FLOAT_TO_Q31(0.98f);
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
  // Reduce wet signal when input is strong
  if (ctx->p_ducking > 0) {
    q31_t env_level = dsp_env_process(&ctx->envelope, q31_abs(dry));
    
    // Invert envelope: more input = less wet
    q31_t duck_amount = q31_sub_sat(Q31_MAX, env_level);
    duck_amount = q31_mul(duck_amount, ctx->p_ducking);  // Scale by ducking parameter
    duck_amount = q31_add_sat(duck_amount, q31_sub_sat(Q31_MAX, ctx->p_ducking));  // Ensure minimum
    
    wet = q31_mul(wet, duck_amount);
  }

  // 12. Output Mixing & Decorrelation
  // Right channel: decorrelated through AP3
  q31_t wet_r = dsp_allpass_process(&ctx->ap3, wet, (TE_AP3_SIZE / 2) << 16);

  // Variable Dry/Wet Mix (controlled by p_mix parameter)
  q31_t dry_gain = q31_sub_sat(Q31_MAX, ctx->p_mix);
  q31_t wet_gain = ctx->p_mix;

  *out_l = q31_add_sat(q31_mul(dry, dry_gain), q31_mul(wet, wet_gain));
  *out_r = q31_add_sat(q31_mul(dry, dry_gain), q31_mul(wet_r, wet_gain));
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
  // Map rate 0..1 to step size
  q31_t step = rate >> 8;
  dsp_rand_walk_set_step(&ctx->modulator, step);
  ctx->p_depth = depth;
}

void te2350_set_tone(te2350_t *ctx, q31_t tone) {
  ctx->p_tone = tone;
  
  // One-Pole Filter Coefficients
  // Range 0..0.45: Lowpass (Dark -> Open)
  // Range 0.55..1: Highpass (Open -> Thin)
  
  if (tone < FLOAT_TO_Q31(0.45f)) {
      // Lowpass: Coeff 0.02 (Dark) to 0.5 (Open)
      // tone (0..0.45) * 1.1 = (0..0.5)
      q31_t c = q31_mul(tone, FLOAT_TO_Q31(1.1f)); 
      c = q31_add_sat(c, FLOAT_TO_Q31(0.02f));
      if (c > FLOAT_TO_Q31(0.5f)) c = FLOAT_TO_Q31(0.5f);
      ctx->fb_lp.coeff = c;
  } else if (tone > FLOAT_TO_Q31(0.55f)) {
      // Highpass: Coeff 0.02 (Open) to 0.4 (Thin)
      q31_t t = q31_sub_sat(tone, FLOAT_TO_Q31(0.55f));
      q31_t c = q31_mul(t, FLOAT_TO_Q31(0.9f));
      c = q31_add_sat(c, FLOAT_TO_Q31(0.02f));
      if (c > FLOAT_TO_Q31(0.4f)) c = FLOAT_TO_Q31(0.4f);
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
