#ifndef HS_CONFIG_PARAMS_H
#define HS_CONFIG_PARAMS_H

#include "ap_int.h"
#include "ap_fixed.h"

// Image Dimensions
#define MAX_HEIGHT 128
#define MAX_WIDTH 256

// Algorithm Parameters
// Alpha: Regularization parameter. Defined as compile-time constant.
#define ALPHA_VAL 1
// Fixed number of iterations
#define N_ITER 20

// Fractional bits for output scaling (Q8.8)
#define FLOW_FRAC_BITS 8

// Data Types

// Input pixel: 8-bit data stored in 16-bit unsigned
typedef unsigned short pixel_t;

// Output Flow: 16-bit fixed point Q8.8
// 1 sign bit, 7 integer bits, 8 fractional bits (Range +/- 128)
typedef ap_fixed<16, 8, AP_RND, AP_SAT> flow_t;

// Gradient Type
// Sobel output range approx +/- 1020. 
// 16-bit signed integer is sufficient and BRAM friendly.
typedef ap_int<16> grad_t;

// Internal Calculation Type
// To avoid overflow during intermediate MAC operations
// Ix (1020) * u_avg (128) can be ~ 130,000.
// Need > 17 integer bits.
// Using 40-bit width with 24 integer bits to ensure safety and good precision.
typedef ap_fixed<40, 24, AP_RND, AP_SAT> calc_t;

#endif
