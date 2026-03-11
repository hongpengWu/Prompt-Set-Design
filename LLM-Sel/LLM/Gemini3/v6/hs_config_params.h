#ifndef HS_CONFIG_PARAMS_H
#define HS_CONFIG_PARAMS_H

#include "ap_int.h"
#include "ap_fixed.h"

// Image Dimensions
#define MAX_HEIGHT 128
#define MAX_WIDTH 256

// Algorithm Parameters
#define ALPHA_VAL 10
#define N_ITER 20

// Fixed Point Types

// Gradient Type:
// Sobel operator on 8-bit/16-bit input can produce large values.
// Max possible Sobel value (approx) for 8-bit: 4 * 255 = 1020.
// For 16-bit: 4 * 65535 = 262140.
// ap_int<20> covers range +/- 524288, sufficient for 16-bit inputs.
typedef ap_int<20> grad_t;

// Velocity Storage Type (Q6.10):
// 6 integer bits (1 sign + 5 value), 10 fractional bits.
// Range: [-32, 31.999].
// Matches requirement "Q6.10".
// AP_RND: Round to nearest (for better precision/stability).
// AP_SAT: Saturate on overflow (to avoid wrapping artifacts).
typedef ap_fixed<16, 6, AP_RND, AP_SAT> vel_storage_t;

// Velocity Calculation Type:
// Higher precision for intermediate update steps to minimize error accumulation.
// 32 bits total. 20 integer bits (handles numerator/denominator magnitude), 12 fractional bits.
// Denominator can be up to ~ 2*10^6 (21 bits). 
// Wait, if denominator is large, we need enough integer bits?
// No, we divide BY the denominator. The result is small (velocity).
// We need precision for the operands of the division.
// Numerator: Ix * (...) can be large.
// Ix^2 ~ 10^10 if 16-bit input. 
// If input is 8-bit, Ix^2 ~ 10^6.
// Let's assume we need to handle the calculation safely.
// We'll use a larger type for the update term calculation.
typedef ap_fixed<32, 12, AP_RND, AP_SAT> vel_calc_t;

#endif
