#ifndef DSP_MATH_H
#define DSP_MATH_H

#include <stdbool.h>
#include <stdint.h>

// Q31 Constants
#define Q31_MAX ((int32_t)0x7FFFFFFF)
#define Q31_MIN ((int32_t)0x80000000)

typedef int32_t q31_t;
typedef uint32_t q16_16_t;

// --- Saturating Arithmetic ---

static inline q31_t q31_add_sat(q31_t a, q31_t b) {
  q31_t res;
  if (__builtin_add_overflow(a, b, &res)) {
    return (a < 0) ? Q31_MIN : Q31_MAX;
  }
  return res;
}

static inline q31_t q31_sub_sat(q31_t a, q31_t b) {
  q31_t res;
  if (__builtin_sub_overflow(a, b, &res)) {
    return (a < 0) ? Q31_MIN : Q31_MAX;
  }
  return res;
}

static inline q31_t q31_mul(q31_t a, q31_t b) {
  if (a == Q31_MIN && b == Q31_MIN) {
    return Q31_MAX;
  }
  // Cast to int64_t for full precision then shift
  return (q31_t)(((int64_t)a * b) >> 31);
}

static inline q31_t q31_abs(q31_t x) {
  if (x == Q31_MIN)
    return Q31_MAX;
  return (x < 0) ? -x : x;
}

// --- Conversion Macros ---
// Fixed: Use 2147483647.0f to avoid overflow for positive values near 1.0
#define FLOAT_TO_Q31(x) ((q31_t)((x) * 2147483647.0f))
#define Q31_TO_FLOAT(x) ((float)(x) / 2147483648.0f)

// Safe conversion with clamping
static inline q31_t float_to_q31_safe(float x) {
  if (x >= 1.0f) return Q31_MAX;
  if (x <= -1.0f) return Q31_MIN;
  return (q31_t)(x * 2147483647.0f);
}

// Multiply with gain (useful for mixing)
static inline q31_t q31_mul_gain(q31_t x, q31_t gain_q31) {
  return q31_mul(x, gain_q31);
}

#endif // DSP_MATH_H
