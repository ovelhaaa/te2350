// Offline measurement bridge. Never linked into the plugin/audio callback.
#include "../../include/te2350.h"
#include <stdlib.h>
#include <math.h>
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
EXPORT int m8_render(const float *input, float *output, float *gain, int count,
                     float sr, int interval, float amount, float regen,
                     float bloom, float duck, float threshold, int freezeAt) {
  te2350_t ctx;
  void *pool = calloc(1, TE2350_REQUIRED_MEMORY_BYTES);
  if (!pool || !te2350_init(&ctx, pool, TE2350_REQUIRED_MEMORY_BYTES, sr)) {
    free(pool); return 0;
  }
  te2350_set_time_samples(&ctx, (int)(sr * .12f));
  te2350_set_mix(&ctx, Q31_MAX);
  te2350_set_feedback(&ctx, FLOAT_TO_Q31(.90f));
  te2350_set_tail(&ctx, float_to_q31_safe(bloom));
  te2350_set_tone(&ctx, FLOAT_TO_Q31(.75f));
  te2350_set_shimmer(&ctx, float_to_q31_safe(amount));
  te2350_set_shimmer_interval(&ctx, interval);
  te2350_set_octave_feedback_enabled(&ctx, amount > 0 && regen > 0);
  te2350_set_octave_feedback_amount(&ctx, float_to_q31_safe(regen));
  te2350_set_ducking(&ctx, float_to_q31_safe(duck));
  te2350_set_duck_threshold(&ctx, float_to_q31_safe(threshold));
  for (int n=0; n<count; ++n) {
    if (n == freezeAt) te2350_set_freeze(&ctx, true);
    q31_t l, r;
    te2350_process(&ctx, float_to_q31_safe(input[n]), &l, &r);
    output[n*2] = Q31_TO_FLOAT(l); output[n*2+1] = Q31_TO_FLOAT(r);
#ifdef M8_ADAPTIVE
    gain[n] = Q31_TO_FLOAT(q31_sub_sat(Q31_MAX, ctx.duck_reduction_state));
#else
    q31_t detector = ctx.envelope.envelope;
    q31_t t = ctx.p_duck_threshold_smoothed;
    if (t > 0) detector = detector <= t ? 0 : (q31_t)(((int64_t)(detector-t) << 31) / ((int64_t)Q31_MAX-t));
    q31_t amp = detector > (Q31_MAX>>2) ? Q31_MAX : detector << 2;
    gain[n] = Q31_TO_FLOAT(q31_sub_sat(Q31_MAX, q31_mul(amp,ctx.p_ducking)));
#endif
  }
  free(pool); return 1;
}
