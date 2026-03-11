#ifndef HS_CONFIG_PARAMS_H
#define HS_CONFIG_PARAMS_H

#include "ap_int.h"
#include "ap_fixed.h"

// Image Dimensions
#define MAX_HEIGHT 128
#define MAX_WIDTH 256

// Algorithm parameters
// Alpha: Regularization weight. 
// Using 1 (represented in fixed point) as per Matlab baseline default.
// Lower alpha allows more flow discontinuity/magnitude.
#define ALPHA_VAL 1

// Iterations: 20 is standard for HS convergence on small images.
#define N_ITER 20

// Fixed Point Configuration

// Output Flow Format: Q8.8
// - 16 bits total
// - 8 fractional bits -> Precision ~0.004 pixels
// - 8 integer bits (including sign) -> Range +/- 128 pixels (Prompt requires covering +/- 64)
#define FLOW_FRAC_BITS 8

// Input pixel type (8-bit grayscale stored in unsigned short)
typedef unsigned short pixel_t;

// Gradient type (Ix, Iy, It)
// Max Gradient (Sobel) ~ 4 * 255 = 1020.
// Min Gradient ~ -1020.
// It = I2 - I1 -> +/- 255.
// 12 bits signed is sufficient (+/- 2048).
// Using ap_fixed<16, 12> to align with 16-bit words and allow easy math.
// 12 integer bits, 4 fractional bits (unused for raw gradients but useful if we filter).
typedef ap_fixed<16, 12, AP_RND, AP_SAT> grad_t;

// Internal Flow Type
// Q8.8 format as decided above.
typedef ap_fixed<16, 8, AP_RND, AP_SAT> flow_t;

// Denominator Type
// D = alpha^2 + Ix^2 + Iy^2
// alpha^2 = 100.
// Ix^2 max = 1020^2 ~= 1,040,400.
// Sum ~= 2.1 * 10^6.
// Needs ~22 bits integer.
// Using ap_fixed<32, 22> to be safe and efficient (32-bit word).
typedef ap_fixed<32, 22, AP_RND, AP_SAT> den_t;

// Intermediate Calculation Type (Accumulators, Numerator)
// Term: Ix * u_avg
// Ix (12 bit) * u (16 bit) -> ~28 bits needed?
// Value range: 1020 * 64 = 65280. 
// Numerator: Ix * (Ix*u + Iy*v + It)
// Inner term: ~65280 + 255.
// Outer mult: 1020 * 65500 ~= 6.7 * 10^7.
// Needs ~27 bits integer.
// Using ap_fixed<40, 28> to cover the range with margin and keep fractional precision.
// 28 integer bits, 12 fractional bits.
typedef ap_fixed<40, 28, AP_RND, AP_SAT> calc_t;

#endif
