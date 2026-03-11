#ifndef HS_CONFIG_PARAMS_H
#define HS_CONFIG_PARAMS_H

#include "ap_int.h"
#include "ap_fixed.h"

// Image Dimensions
#define MAX_HEIGHT 128
#define MAX_WIDTH 256

// Algorithm Parameters
#define N_ITER 20

// Fixed Point Types

// Velocity: ap_fixed<16, 6>
// 1 sign bit, 5 integer bits (+/- 32 range), 10 fractional bits (precision ~0.001)
// This reduces memory usage to fit in Zynq 7020 BRAM.
typedef ap_fixed<16, 6> vel_t;

// Gradient: Sobel output is approx +/- 1020. Fits in 12 bits signed.
// We use 16 bits.
typedef ap_int<16> grad_t; 

// Input Pixel: 0-255
typedef unsigned char pixel_t;

// Alpha Squared (Regularization)
// Matlab alpha=1. We use 1 here.
#define ALPHA_SQUARED 1

// Output Scaling
// We output signed short.
// Let's use Q8.8 format for output (scale by 256).
// Range +/- 128. Precision 1/256.
// This is standard for fixed point image processing.
#define OUT_FRAC_BITS 8
#define OUT_SCALE (1 << OUT_FRAC_BITS)

#endif
