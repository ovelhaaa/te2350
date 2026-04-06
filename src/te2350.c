#include "../include/te2350.h"

static void update_tone_filter(te2350_t *ctx);
static void build_time_lut(te2350_t *ctx);
static int32_t map_time_samples(const te2350_t *ctx, q31_t time_q31);
static q31_t feedback_condition(te2350_t *ctx,
                                q31_t loop_src,
                                q31_t shimmer_return,
                                q31_t env_level,
                                q31_t effective_feedback);

static inline q31_t q31_lerp(q31_t a, q31_t b, q31_t t) {
  return q31_add_sat(q31_mul(a, q31_sub_sat(Q31_MAX, t)), q31_mul(b, t));
}

bool te2350_init(te2350_t *ctx, void *memory_block, size_t total_bytes, float sample_rate) {
  if (sample_rate <= 0.0f) {
    sample_rate = 48000.0f;
  }
  ctx->sample_rate = sample_rate;

  float sr_ratio = sample_rate / 48000.0f;
  ctx->time_smooth_coeff = float_to_q31_safe(0.002f * (48000.0f / sample_rate));

  ctx->max_delay_samples = TE_MAIN_DELAY_SIZE - 320;
  ctx->min_delay_samples = (int32_t)(80 * sr_ratio);
  if (ctx->min_delay_samples < 1) {
    ctx->min_delay_samples = 1;
  }

  ctx->wobble_mod_base = (int32_t)(10 * sr_ratio);
  ctx->wobble_mod_scale = (int32_t)(45 * sr_ratio);

  q31_t *mem = (q31_t *)memory_block;
  size_t available_words = total_bytes / sizeof(q31_t);
  size_t offset = 0;

#define ALLOC_BUF(size)                                                        \
  if (offset + (size) > available_words)                                       \
    return false;                                                              \
  q31_t *buf_##size = &mem[offset];                                            \
  offset += (size);

  ALLOC_BUF(TE_MAIN_DELAY_SIZE);
  dsp_delay_init(&ctx->main_delay, buf_TE_MAIN_DELAY_SIZE, TE_MAIN_DELAY_SIZE);

  ALLOC_BUF(TE_AP1_SIZE);
  dsp_allpass_init(&ctx->ap1, buf_TE_AP1_SIZE, TE_AP1_SIZE, FLOAT_TO_Q31(0.46f));

  ALLOC_BUF(TE_AP2_SIZE);
  dsp_allpass_init(&ctx->ap2, buf_TE_AP2_SIZE, TE_AP2_SIZE, FLOAT_TO_Q31(0.38f));

  ALLOC_BUF(TE_AP3_SIZE);
  dsp_allpass_init(&ctx->ap3, buf_TE_AP3_SIZE, TE_AP3_SIZE, FLOAT_TO_Q31(0.33f));

  ALLOC_BUF(TE_AP4_SIZE);
  dsp_allpass_init(&ctx->ap4, buf_TE_AP4_SIZE, TE_AP4_SIZE, FLOAT_TO_Q31(0.29f));

  ALLOC_BUF(TE_PITCH_SIZE);
  dsp_pitch_init(&ctx->pitch_shifter, buf_TE_PITCH_SIZE, TE_PITCH_SIZE);

  ALLOC_BUF(TE_OCTAVE_PITCH_SIZE);
  dsp_pitch_init(&ctx->octave_shifter, buf_TE_OCTAVE_PITCH_SIZE, TE_OCTAVE_PITCH_SIZE);

  dsp_dc_blocker_init(&ctx->dc_block);
  dsp_dc_blocker_init(&ctx->fb_dc);

  dsp_svf_init(&ctx->damping_filter, FLOAT_TO_Q31(0.25f), FLOAT_TO_Q31(0.5f));

  dsp_rand_walk_init(&ctx->space_mod, 12345, FLOAT_TO_Q31(0.00008f));
  dsp_rand_walk_init(&ctx->time_mod, 54321, FLOAT_TO_Q31(0.00002f));
  dsp_env_init(&ctx->envelope, FLOAT_TO_Q31(0.05f), FLOAT_TO_Q31(0.0004f));

  dsp_onepole_init(&ctx->fb_lp, FLOAT_TO_Q31(0.10f));
  dsp_onepole_init(&ctx->fb_hp, FLOAT_TO_Q31(0.015f));
  dsp_onepole_init(&ctx->presence_hp, FLOAT_TO_Q31(0.030f)); // gentle low-mud removal
  dsp_onepole_init(&ctx->presence_lp, FLOAT_TO_Q31(0.210f)); // gentle harshness control
  ctx->presence_gain_smooth = FLOAT_TO_Q31(0.12f);

  dsp_melody_init(&ctx->melody);
  dsp_melody_set_volume(&ctx->melody, FLOAT_TO_Q31(0.18f));
  dsp_melody_set_density(&ctx->melody, FLOAT_TO_Q31(0.45f));
  dsp_melody_set_decay(&ctx->melody, FLOAT_TO_Q31(0.55f));

  ctx->p_melody_enabled = false;
  ctx->p_melody_only = false;
  ctx->octave_feedback_enabled = false;
  ctx->octave_feedback_amount = 0;

  ctx->p_feedback = FLOAT_TO_Q31(0.88f);
  ctx->p_time = FLOAT_TO_Q31(0.74f);
  ctx->p_rate = FLOAT_TO_Q31(0.45f);
  ctx->p_depth = FLOAT_TO_Q31(0.32f);
  ctx->p_tone = FLOAT_TO_Q31(0.12f);
  ctx->p_mix = FLOAT_TO_Q31(0.5f);
  ctx->p_shimmer = 0;
  ctx->p_diffusion = FLOAT_TO_Q31(0.8f);
  ctx->p_chaos = FLOAT_TO_Q31(0.2f);
  ctx->p_ducking = FLOAT_TO_Q31(0.15f);
  ctx->p_wobble = FLOAT_TO_Q31(0.3f);

  ctx->p_time_smoothed = ctx->p_time;
  ctx->p_feedback_smoothed = ctx->p_feedback;
  ctx->p_mix_smoothed = ctx->p_mix;
  ctx->p_tone_smoothed = ctx->p_tone;
  ctx->p_diffusion_smoothed = ctx->p_diffusion;

  ctx->feedback_state = 0;
  ctx->bloom_state = 0;
  ctx->chaos_seed = 98765;
  ctx->freeze_mode = false;
  ctx->freeze_crossfade = 0;

  const int early_ms[TE_NUM_EARLY_TAPS] = {8, 13, 21, 33, 47, 65};
  const float early_gain_f[TE_NUM_EARLY_TAPS] = {0.32f, 0.28f, 0.24f, 0.20f, 0.16f, 0.12f};
  const int late_ms[TE_NUM_LATE_TAPS] = {148, 236};
  const float late_gain_f[TE_NUM_LATE_TAPS] = {0.12f, 0.08f};

  for (int i = 0; i < TE_NUM_EARLY_TAPS; ++i) {
    size_t d = (size_t)((early_ms[i] * 0.001f) * sample_rate);
    if (d >= TE_MAIN_DELAY_SIZE) d = TE_MAIN_DELAY_SIZE - 1;
    ctx->early_tap_delays[i] = d;
    ctx->early_tap_gains[i] = FLOAT_TO_Q31(early_gain_f[i]);
  }

  for (int i = 0; i < TE_NUM_LATE_TAPS; ++i) {
    size_t d = (size_t)((late_ms[i] * 0.001f) * sample_rate);
    if (d >= TE_MAIN_DELAY_SIZE) d = TE_MAIN_DELAY_SIZE - 1;
    ctx->late_tap_delays[i] = d;
    ctx->late_tap_gains[i] = FLOAT_TO_Q31(late_gain_f[i]);
  }

  build_time_lut(ctx);
  return true;
}

void te2350_process(te2350_t *ctx, q31_t in_mono, q31_t *out_l, q31_t *out_r) {
  dsp_melody_set_enabled(&ctx->melody, ctx->p_melody_enabled);
  q31_t melody_sig = dsp_melody_process(&ctx->melody);

  q31_t external_in = ctx->p_melody_only ? 0 : in_mono;
  q31_t mixed_in = q31_add_sat(external_in, melody_sig);
  q31_t dry = dsp_dc_blockProcess(&ctx->dc_block, mixed_in);

#define SMOOTH_PARAM(target, current, coeff) \
  do { \
    q31_t diff = q31_sub_sat((target), (current)); \
    q31_t delta = q31_mul(diff, (coeff)); \
    if (diff > 0 && delta == 0) delta = 1; \
    if (diff < 0 && delta == 0) delta = -1; \
    (current) = q31_add_sat((current), delta); \
  } while (0)

  SMOOTH_PARAM(ctx->p_time, ctx->p_time_smoothed, ctx->time_smooth_coeff);

  q31_t fast_smooth = ctx->time_smooth_coeff * 5;
  if (fast_smooth > FLOAT_TO_Q31(0.1f)) fast_smooth = FLOAT_TO_Q31(0.1f);

  SMOOTH_PARAM(ctx->p_feedback, ctx->p_feedback_smoothed, fast_smooth);
  SMOOTH_PARAM(ctx->p_mix, ctx->p_mix_smoothed, fast_smooth);
  SMOOTH_PARAM(ctx->p_tone, ctx->p_tone_smoothed, fast_smooth);
  SMOOTH_PARAM(ctx->p_diffusion, ctx->p_diffusion_smoothed, fast_smooth);

  q31_t env_level = dsp_env_process(&ctx->envelope, q31_abs(dry));

  if (env_level > ctx->bloom_state) {
    ctx->bloom_state = q31_add_sat(ctx->bloom_state, q31_mul(q31_sub_sat(env_level, ctx->bloom_state), FLOAT_TO_Q31(0.04f)));
  } else {
    ctx->bloom_state = q31_sub_sat(ctx->bloom_state, FLOAT_TO_Q31(0.00006f));
  }

  q31_t base_space_step = ctx->p_rate >> 7;
  if (base_space_step < FLOAT_TO_Q31(0.00004f)) base_space_step = FLOAT_TO_Q31(0.00004f);

  q31_t base_time_step = base_space_step >> 2;
  if (base_time_step < FLOAT_TO_Q31(0.00001f)) base_time_step = FLOAT_TO_Q31(0.00001f);

  ctx->chaos_seed = ctx->chaos_seed * 1664525u + 1013904223u;
  q31_t chaos_rand = (q31_t)(ctx->chaos_seed >> 1);
  q31_t chaos_amt = q31_mul(ctx->p_chaos, FLOAT_TO_Q31(0.45f));
  q31_t chaos_scale = q31_add_sat(FLOAT_TO_Q31(0.8f), q31_mul(chaos_rand, chaos_amt));

  dsp_rand_walk_set_step(&ctx->space_mod, q31_mul(base_space_step, chaos_scale));
  dsp_rand_walk_set_step(&ctx->time_mod, q31_mul(base_time_step, chaos_scale));

  q31_t space_rnd = dsp_rand_walk_process(&ctx->space_mod);
  q31_t time_rnd = ctx->freeze_mode ? 0 : dsp_rand_walk_process(&ctx->time_mod);

  q31_t space_mod = q31_mul(space_rnd, q31_mul(ctx->p_depth, FLOAT_TO_Q31(0.22f)));
  q31_t time_mod = q31_mul(time_rnd, q31_mul(ctx->p_depth, FLOAT_TO_Q31(0.08f)));

  if (ctx->freeze_mode) {
    ctx->freeze_crossfade = q31_add_sat(ctx->freeze_crossfade, FLOAT_TO_Q31(0.0012f));
  } else {
    ctx->freeze_crossfade = q31_sub_sat(ctx->freeze_crossfade, FLOAT_TO_Q31(0.0010f));
  }

  q31_t input_gain = q31_sub_sat(Q31_MAX, q31_mul(ctx->freeze_crossfade, FLOAT_TO_Q31(0.985f)));
  q31_t loop_input = q31_add_sat(q31_mul(dry, input_gain), ctx->feedback_state);

  q31_t diff = ctx->p_diffusion_smoothed;
  q31_t bloom = ctx->bloom_state;
  q31_t bloom_diff_boost = q31_mul(bloom, FLOAT_TO_Q31(0.30f));
  q31_t diff_eff = q31_add_sat(diff, bloom_diff_boost);

  ctx->ap1.gain = q31_mul(FLOAT_TO_Q31(0.45f), diff_eff);
  ctx->ap2.gain = q31_mul(FLOAT_TO_Q31(0.34f), diff_eff);
  ctx->ap3.gain = q31_mul(FLOAT_TO_Q31(0.32f), diff_eff);
  ctx->ap4.gain = q31_mul(FLOAT_TO_Q31(0.28f), diff_eff);

  q31_t pre_mod = q31_mul(space_mod, diff_eff);
  int32_t ap1_d_i = ((int32_t)(TE_AP1_SIZE / 2) << 16) + (pre_mod >> 5);
  int32_t ap2_d_i = ((int32_t)(TE_AP2_SIZE / 2) << 16) - (pre_mod >> 5);
  if (ap1_d_i < 0x10000) ap1_d_i = 0x10000;
  if (ap1_d_i > ((TE_AP1_SIZE - 2) << 16)) ap1_d_i = ((TE_AP1_SIZE - 2) << 16);
  if (ap2_d_i < 0x10000) ap2_d_i = 0x10000;
  if (ap2_d_i > ((TE_AP2_SIZE - 2) << 16)) ap2_d_i = ((TE_AP2_SIZE - 2) << 16);

  q31_t pre1 = dsp_allpass_process(&ctx->ap1, loop_input, (q16_16_t)ap1_d_i);
  q31_t pre2 = dsp_allpass_process(&ctx->ap2, pre1, (q16_16_t)ap2_d_i);

  dsp_delay_write(&ctx->main_delay, pre2);

  int32_t d_base = map_time_samples(ctx, ctx->p_time_smoothed);
  int32_t wobble_depth = ctx->wobble_mod_base + (int32_t)(((int64_t)ctx->p_wobble * ctx->wobble_mod_scale) >> 31);
  int32_t d_mod = (int32_t)(((int64_t)time_mod * wobble_depth) >> 31);
  int32_t d_samp_mod = d_base + d_mod;
  if (d_samp_mod < ctx->min_delay_samples) d_samp_mod = ctx->min_delay_samples;
  if (d_samp_mod > ctx->max_delay_samples) d_samp_mod = ctx->max_delay_samples;

  q31_t delay_out = dsp_delay_read_hermite(&ctx->main_delay, ((q16_16_t)d_samp_mod) << 16);

  q31_t early_cloud = 0;
  for (int i = 0; i < TE_NUM_EARLY_TAPS; ++i) {
    int32_t wobble = (int32_t)(((int64_t)space_mod * (4 + i * 3)) >> 31);
    int32_t tap_d = (int32_t)ctx->early_tap_delays[i] + wobble;
    if (tap_d < 1) tap_d = 1;
    if (tap_d >= TE_MAIN_DELAY_SIZE) tap_d = TE_MAIN_DELAY_SIZE - 1;
    q31_t tap = dsp_delay_read(&ctx->main_delay, (size_t)tap_d);
    early_cloud = q31_add_sat(early_cloud, q31_mul(tap, ctx->early_tap_gains[i]));
  }

  q31_t late_accents = 0;
  q31_t late_freeze_scale = q31_sub_sat(Q31_MAX, q31_mul(ctx->freeze_crossfade, FLOAT_TO_Q31(0.9f)));
  for (int i = 0; i < TE_NUM_LATE_TAPS; ++i) {
    q31_t tap = dsp_delay_read(&ctx->main_delay, ctx->late_tap_delays[i]);
    q31_t g = q31_mul(ctx->late_tap_gains[i], late_freeze_scale);
    late_accents = q31_add_sat(late_accents, q31_mul(tap, g));
  }

  q31_t post_in = q31_add_sat(delay_out, q31_mul(early_cloud, FLOAT_TO_Q31(0.55f)));
  int32_t ap4_d_i = ((int32_t)(TE_AP4_SIZE / 2) << 16) + (pre_mod >> 6);
  int32_t ap3_d_i = ((int32_t)(TE_AP3_SIZE / 2) << 16) - (pre_mod >> 6);
  if (ap4_d_i < 0x10000) ap4_d_i = 0x10000;
  if (ap4_d_i > ((TE_AP4_SIZE - 2) << 16)) ap4_d_i = ((TE_AP4_SIZE - 2) << 16);
  if (ap3_d_i < 0x10000) ap3_d_i = 0x10000;
  if (ap3_d_i > ((TE_AP3_SIZE - 2) << 16)) ap3_d_i = ((TE_AP3_SIZE - 2) << 16);

  q31_t post1 = dsp_allpass_process(&ctx->ap4, post_in, (q16_16_t)ap4_d_i);
  q31_t post2 = dsp_allpass_process(&ctx->ap3, post1, (q16_16_t)ap3_d_i);

  q31_t shimmer_parallel = 0;
  if (ctx->p_shimmer > 0) {
    // dsp_pitch_process expects 0..Q31_MAX -> 0.5x..2.0x.
    // Map shimmer control to a musically useful 1.0x..2.0x range:
    // q31 = 1/3 + (2/3 * p_shimmer). This keeps shimmer as an octave-up halo
    // instead of collapsing near the 0.5x floor.
    q31_t shimmer_pitch = q31_add_sat(FLOAT_TO_Q31(0.33333334f),
                                      q31_mul(ctx->p_shimmer, FLOAT_TO_Q31(0.6666667f)));
    shimmer_pitch = q31_add_sat(shimmer_pitch, q31_mul(space_rnd, FLOAT_TO_Q31(0.002f)));
    q31_t shifted = dsp_pitch_process(&ctx->pitch_shifter, post2, shimmer_pitch);
    q31_t halo = q31_lerp(shifted, post2, FLOAT_TO_Q31(0.65f));
    shimmer_parallel = q31_mul(halo, q31_mul(ctx->p_shimmer, FLOAT_TO_Q31(0.23f)));
  }

  q31_t effective_feedback = ctx->p_feedback_smoothed;
  if (ctx->freeze_crossfade > 0) {
    q31_t freeze_fb = FLOAT_TO_Q31(0.995f);
    effective_feedback = q31_lerp(effective_feedback, freeze_fb, ctx->freeze_crossfade);
  }

  ctx->feedback_state = feedback_condition(ctx, post2, shimmer_parallel, env_level, effective_feedback);

  q31_t wet_mid = q31_add_sat(q31_mul(post2, FLOAT_TO_Q31(0.40f)), q31_mul(early_cloud, FLOAT_TO_Q31(0.20f)));
  wet_mid = q31_add_sat(wet_mid, q31_mul(late_accents, FLOAT_TO_Q31(0.20f)));
  wet_mid = q31_add_sat(wet_mid, q31_mul(shimmer_parallel, FLOAT_TO_Q31(0.20f)));

  // Dedicated presence rail: derived from short/clear content only (no loop writeback).
  q31_t presence_src = q31_add_sat(q31_mul(delay_out, FLOAT_TO_Q31(0.60f)),
                                   q31_mul(early_cloud, FLOAT_TO_Q31(0.40f)));
  q31_t presence_low_ref = dsp_onepole_lp(&ctx->presence_hp, presence_src);
  q31_t presence_hp = q31_sub_sat(presence_src, presence_low_ref);
  q31_t presence_band = dsp_onepole_lp(&ctx->presence_lp, presence_hp);
  q31_t presence_sat = dsp_soft_saturate_gentle(presence_band);

  q31_t presence_target_gain = FLOAT_TO_Q31(0.16f); // subtle but audible
  q31_t presence_gain_delta = q31_sub_sat(presence_target_gain, ctx->presence_gain_smooth);
  ctx->presence_gain_smooth = q31_add_sat(ctx->presence_gain_smooth,
                                          q31_mul(presence_gain_delta, FLOAT_TO_Q31(0.03f)));
  wet_mid = q31_add_sat(wet_mid, q31_mul(presence_sat, ctx->presence_gain_smooth));

  q31_t side_from_taps = 0;
  side_from_taps = q31_add_sat(side_from_taps,
                               q31_mul(dsp_delay_read(&ctx->main_delay, ctx->early_tap_delays[1]), FLOAT_TO_Q31(0.14f)));
  side_from_taps = q31_sub_sat(side_from_taps,
                               q31_mul(dsp_delay_read(&ctx->main_delay, ctx->early_tap_delays[4]), FLOAT_TO_Q31(0.12f)));

  q31_t side_diff = q31_sub_sat(post1 >> 1, post2 >> 1);
  q31_t wet_side = q31_add_sat(q31_mul(side_diff, FLOAT_TO_Q31(0.55f)), q31_mul(side_from_taps, FLOAT_TO_Q31(0.45f)));

  q31_t width = q31_add_sat(FLOAT_TO_Q31(0.34f), q31_mul(diff_eff, FLOAT_TO_Q31(0.30f)));
  width = q31_add_sat(width, q31_mul(bloom, FLOAT_TO_Q31(0.28f)));
  if (width > FLOAT_TO_Q31(0.90f)) width = FLOAT_TO_Q31(0.90f);

  q31_t bloom_tone_trim = q31_sub_sat(Q31_MAX, q31_mul(bloom, FLOAT_TO_Q31(0.18f)));
  wet_mid = q31_mul(wet_mid, bloom_tone_trim);

  q31_t amp_env = env_level > (Q31_MAX >> 2) ? Q31_MAX : env_level << 2;
  q31_t duck_reduction = q31_mul(q31_sub_sat(Q31_MAX, q31_sub_sat(Q31_MAX, amp_env)), ctx->p_ducking);
  q31_t final_duck_gain = q31_sub_sat(Q31_MAX, duck_reduction);

  q31_t wet_l = q31_add_sat(wet_mid >> 1, q31_mul(wet_side, width) >> 1);
  q31_t wet_r = q31_sub_sat(wet_mid >> 1, q31_mul(wet_side, width) >> 1);
  wet_l = q31_mul(wet_l, final_duck_gain);
  wet_r = q31_mul(wet_r, final_duck_gain);

  q31_t dry_gain = q31_sub_sat(Q31_MAX, ctx->p_mix_smoothed);
  q31_t wet_gain = ctx->p_mix_smoothed;

  *out_l = q31_add_sat(q31_mul(dry, dry_gain), q31_mul(wet_l, wet_gain));
  *out_r = q31_add_sat(q31_mul(dry, dry_gain), q31_mul(wet_r, wet_gain));
}

q31_t te2350_get_envelope(te2350_t *ctx) {
  return ctx->envelope.envelope;
}

q31_t te2350_get_modulator(te2350_t *ctx) {
  return ctx->space_mod.current_value;
}

void te2350_set_feedback(te2350_t *ctx, q31_t feedback) {
  ctx->p_feedback = feedback;
}

void te2350_set_time(te2350_t *ctx, q31_t time) {
  ctx->p_time = time;
}

void te2350_set_mod(te2350_t *ctx, q31_t rate, q31_t depth) {
  ctx->p_rate = rate;
  ctx->p_depth = depth;
}

void te2350_set_tone(te2350_t *ctx, q31_t tone) {
  ctx->p_tone = tone;
}

static void update_tone_filter(te2350_t *ctx) {
  q31_t tone = ctx->p_tone_smoothed;

  q31_t lp_coeff = q31_add_sat(FLOAT_TO_Q31(0.04f), q31_mul(tone, FLOAT_TO_Q31(0.22f)));
  q31_t hp_coeff = q31_add_sat(FLOAT_TO_Q31(0.006f), q31_mul(tone, FLOAT_TO_Q31(0.02f)));

  if (lp_coeff > FLOAT_TO_Q31(0.28f)) lp_coeff = FLOAT_TO_Q31(0.28f);
  if (hp_coeff > FLOAT_TO_Q31(0.035f)) hp_coeff = FLOAT_TO_Q31(0.035f);

  ctx->fb_lp.coeff = lp_coeff;
  ctx->fb_hp.coeff = hp_coeff;
}

void te2350_set_mix(te2350_t *ctx, q31_t mix) {
  ctx->p_mix = mix;
}

void te2350_set_freeze(te2350_t *ctx, bool freeze) {
  ctx->freeze_mode = freeze;
}

void te2350_set_melody_enabled(te2350_t *ctx, bool enabled) {
  ctx->p_melody_enabled = enabled;
  dsp_melody_set_enabled(&ctx->melody, enabled);
}

void te2350_set_melody_only(te2350_t *ctx, bool only) {
  ctx->p_melody_only = only;
}

void te2350_set_melody_volume(te2350_t *ctx, q31_t volume) {
  dsp_melody_set_volume(&ctx->melody, volume);
}

void te2350_set_melody_density(te2350_t *ctx, q31_t density) {
  dsp_melody_set_density(&ctx->melody, density);
}

void te2350_set_melody_decay(te2350_t *ctx, q31_t decay) {
  dsp_melody_set_decay(&ctx->melody, decay);
}

void te2350_set_octave_feedback_enabled(te2350_t *ctx, bool enabled) {
  ctx->octave_feedback_enabled = enabled;
}

void te2350_set_octave_feedback_amount(te2350_t *ctx, q31_t amount) {
  if (amount < 0) amount = 0;
  if (amount > Q31_MAX) amount = Q31_MAX;
  ctx->octave_feedback_amount = amount;
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

static void build_time_lut(te2350_t *ctx) {
  for (int i = 0; i < TE_TIME_LUT_SIZE; ++i) {
    float x = (float)i / (float)(TE_TIME_LUT_SIZE - 1);
    float shaped;
    if (x < 0.38f) {
      float t = x / 0.38f;
      shaped = 0.22f * t * t;
    } else if (x < 0.78f) {
      float t = (x - 0.38f) / 0.40f;
      shaped = 0.22f + 0.40f * (t * (0.7f + 0.3f * t));
    } else {
      float t = (x - 0.78f) / 0.22f;
      shaped = 0.62f + 0.38f * t;
    }

    int32_t samples = ctx->min_delay_samples + (int32_t)((ctx->max_delay_samples - ctx->min_delay_samples) * shaped);
    if (samples < 1) samples = 1;
    if (samples > 65535) samples = 65535;
    ctx->time_lut[i] = (uint16_t)samples;
  }
}

static int32_t map_time_samples(const te2350_t *ctx, q31_t time_q31) {
  if (time_q31 < 0) time_q31 = 0;
  if (time_q31 > Q31_MAX) time_q31 = Q31_MAX;

  uint32_t scaled = (uint32_t)(((uint64_t)time_q31 * (TE_TIME_LUT_SIZE - 1)) >> 31);
  uint32_t idx = scaled;
  if (idx >= (TE_TIME_LUT_SIZE - 1)) {
    return (int32_t)ctx->time_lut[TE_TIME_LUT_SIZE - 1];
  }

  uint32_t frac = (uint32_t)(((uint64_t)time_q31 * (TE_TIME_LUT_SIZE - 1)) & 0x7FFFFFFF);
  int32_t a = ctx->time_lut[idx];
  int32_t b = ctx->time_lut[idx + 1];
  int32_t diff = b - a;
  return a + (int32_t)(((int64_t)diff * frac) >> 31);
}

static q31_t feedback_condition(te2350_t *ctx,
                                q31_t loop_src,
                                q31_t shimmer_return,
                                q31_t env_level,
                                q31_t effective_feedback) {
  update_tone_filter(ctx);

  q31_t lp = dsp_onepole_lp(&ctx->fb_lp, loop_src);
  q31_t low_ref = dsp_onepole_lp(&ctx->fb_hp, loop_src);
  q31_t hp = q31_sub_sat(loop_src, low_ref);

  q31_t shaped = q31_add_sat(q31_mul(lp, FLOAT_TO_Q31(0.72f)), q31_mul(hp, FLOAT_TO_Q31(0.20f)));
  shaped = q31_add_sat(shaped, q31_mul(shimmer_return, FLOAT_TO_Q31(0.12f)));
  shaped = dsp_dc_blockProcess(&ctx->fb_dc, shaped);
  shaped = dsp_soft_saturate_gentle(shaped);

  if (ctx->octave_feedback_enabled && ctx->octave_feedback_amount > 0) {
    q31_t octave = dsp_pitch_process(&ctx->octave_shifter, shaped, Q31_MAX);
    shaped = q31_lerp(shaped, octave, q31_mul(ctx->octave_feedback_amount, FLOAT_TO_Q31(0.35f)));
  }

  q31_t trans_duck = q31_mul(q31_mul(env_level, ctx->p_ducking), FLOAT_TO_Q31(0.22f));
  q31_t fb_gain = q31_sub_sat(effective_feedback, trans_duck);
  if (fb_gain < 0) fb_gain = 0;
  if (fb_gain > FLOAT_TO_Q31(0.995f)) fb_gain = FLOAT_TO_Q31(0.995f);

  return q31_mul(shaped, fb_gain);
}
