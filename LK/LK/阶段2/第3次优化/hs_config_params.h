#ifndef HS_CONFIG_PARAMS_H
#define HS_CONFIG_PARAMS_H

#include "ap_int.h"
#include "ap_fixed.h"

#define MAX_HEIGHT 128
#define MAX_WIDTH 256

// Algorithm parameters
// Alpha: Regularization parameter. Defined as compile-time constant.
// Using value 1 as per Matlab baseline default.
#define ALPHA_VAL 1

// Iterations: Fixed number of iterations.
// Prompt suggested 10/20/50. Using 20.
#define N_ITER 20

// Fixed Point Configuration

// Output format: signed short int.
// We interpret this as a fixed-point number.
// Choosing Q6.10 format:
// - 1 sign bit
// - 5 integer bits (Range approx +/- 32 pixels)
// - 10 fractional bits (Precision 1/1024)
#define FLOW_FRAC_BITS 10
typedef ap_fixed<16, 6, AP_RND, AP_SAT> flow_t;

// Input pixel type
typedef unsigned short pixel_t; // Standard input type

// Gradient type
// Sobel gradients on 8-bit images:
// Max val approx 4 * 255 = 1020.
// Min val approx -1020.
// 12-bit signed integer is sufficient (+/- 2047).
typedef ap_int<12> grad_t;

// Denominator type for update equation: alpha^2 + Ix^2 + Iy^2
// Max Ix^2 ~ 1000^2 = 1,000,000 ~ 2^20.
// Need at least 21 bits.
typedef ap_int<24> den_t;

// Temporary calculation type
typedef ap_fixed<32, 16, AP_RND, AP_SAT> calc_t;

typedef ap_ufixed<32, 24, AP_RND, AP_SAT> den_fx_t;
typedef ap_ufixed<24, 2, AP_RND, AP_SAT> inv_t;

#endif
