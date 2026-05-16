#ifndef DSP_FDN_H
#define DSP_FDN_H

#include <stddef.h>
#include <stdint.h>

#include "dsp_delay.h"
#include "dsp_filters.h"
#include "dsp_math.h"

#define DSP_FDN4_LINES 4

/**
 * @brief Four-line Feedback Delay Network for atmospheric diffusion.
 *
 * The network uses power-of-two delay buffers for fast wrapping, prime-spaced
 * read taps for modal decorrelation, and a normalized 4x4 Hadamard feedback
 * matrix (scale = 1/2) with 6 dB of fixed-point headroom before the
 * matrix sum. The sum saturates only as a wrap-around safety net; decay is
 * controlled by feedback gain, absorption filters, and optional soft saturation.
 */
typedef struct {
  dsp_delay_t delay[DSP_FDN4_LINES];
  dsp_onepole_t air_lp[DSP_FDN4_LINES];
  dsp_onepole_t floor_lp[DSP_FDN4_LINES];

  q16_16_t base_delay_q16[DSP_FDN4_LINES];
  uint32_t mod_phase[DSP_FDN4_LINES];
  uint32_t mod_inc[DSP_FDN4_LINES];
  uint32_t mod_rate_scale_q16;
  q31_t last_read[DSP_FDN4_LINES];

  size_t line_size;
  q31_t feedback;
  q31_t input_gain;
  q31_t mod_depth_samples_q16;
  q31_t wet_gain;
} dsp_fdn4_t;

/**
 * @brief Initialize a 4x4 FDN with four external buffers of equal power-of-two size.
 */
void dsp_fdn4_init(dsp_fdn4_t *fdn,
                   q31_t *buf0,
                   q31_t *buf1,
                   q31_t *buf2,
                   q31_t *buf3,
                   size_t line_size,
                   float sample_rate);

/**
 * @brief Update musical controls. All parameters are Q31 0..1.
 */
void dsp_fdn4_set_params(dsp_fdn4_t *fdn,
                         q31_t feedback,
                         q31_t tone,
                         q31_t diffusion,
                         q31_t mod_rate,
                         q31_t mod_depth);

/**
 * @brief Process a mono sample through the FDN and return stereo wet taps.
 */
void dsp_fdn4_process(dsp_fdn4_t *fdn, q31_t input, q31_t *out_l, q31_t *out_r);

#endif // DSP_FDN_H
