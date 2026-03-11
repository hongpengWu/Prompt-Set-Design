#ifndef HS_CONFIG_PARAMS_H
#define HS_CONFIG_PARAMS_H

#include <ap_int.h>
#include <ap_fixed.h>

#define MAX_HEIGHT 128
#define MAX_WIDTH 256
#define N_ITER 20
#define ALPHA_VAL 10

// Fixed point definitions
// Q6.10: 16 bits total, 6 integer bits (1 sign + 5 magnitude), 10 fractional bits
// Range: [-32, 31.999]
typedef ap_fixed<16, 6, AP_RND, AP_SAT> flow_t;

// Intermediate types
// Gradients: 8-bit diff. Sobel max ~1020.
// Q12.4: 16 bits, 12 int. Range [-2048, 2047].
typedef ap_fixed<16, 12, AP_RND, AP_SAT> grad_t;

// Accumulators for numerator/denominator
// den = alpha^2 + ix^2 + iy^2. Max ~ 100 + 1020^2 + 1020^2 approx 2.1 * 10^6.
// Need ~22 integer bits (including sign).
// We also need precision for the division result (num/den) which is small.
// Let's use 48 bits: 24 integer, 24 fractional.
typedef ap_fixed<48, 24, AP_RND, AP_SAT> accum_t;

#endif
