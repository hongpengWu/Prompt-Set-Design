#ifndef HS_CONFIG_PARAMS_H
#define HS_CONFIG_PARAMS_H

#include "ap_int.h"
#include "ap_fixed.h"

// Image Dimensions
#define MAX_WIDTH 256
#define MAX_HEIGHT 128

// Algorithm Parameters
#define N_ITER 20
#define ALPHA_VAL 1.0
#define ALPHA_SQ 1

// Fixed Point Types
// Optimized for Zynq-7020 BRAM capacity (~630KB)
// Velocity: 16 bits total. 
// Range +/- 32 pixels is sufficient for optical flow.
// 6 integer bits (1 sign + 5 int) gives [-32, 31.99].
// 10 fractional bits gives precision ~0.001.
typedef ap_fixed<16, 6> flow_t;

// Gradient: Sobel output is max ~1020.
// 16-bit integer is sufficient and saves logic/memory compared to fixed.
typedef ap_int<16> grad_t;

// Intermediate Calculation:
// Squares (grad^2) can be 1020^2 ~= 1M. 
// 1M requires 20 bits.
// We need more precision for the denominator.
// ap_fixed<32, 20> ? 
// Let's use 32-bit fixed point for accumulation/calculation.
typedef ap_fixed<32, 20> calc_t;

// Output Scaling (Q6.10 to integer)
// We output signed short (16-bit).
// We want to preserve fractional part.
// Q6.10 means 10 fractional bits.
#define FLOW_FRAC_BITS 10
#define OUT_SCALE (1 << FLOW_FRAC_BITS)

#endif // HS_CONFIG_PARAMS_H
