#ifndef HS_CONFIG_PARAMS_H
#define HS_CONFIG_PARAMS_H

#include "ap_int.h"
#include "ap_fixed.h"

// Image Dimensions
#define MAX_WIDTH  256
#define MAX_HEIGHT 128

// Iteration Count
#define N_ITER 10

// Alpha for Regularization
// Using Q8.8 fixed point representation
// Value: 20.0
// Alpha squared = 400.0
#define ALPHA_VAL 20.0
#define ALPHA_SQ_VAL (ALPHA_VAL * ALPHA_VAL)

// Fixed point types
// flow_t: 16-bit, 8 integer bits, 8 fractional bits (Q8.8)
// Range: -128 to +127.996
typedef ap_fixed<16, 8, AP_RND, AP_SAT> flow_t;

// grad_t: Gradient type. Can be -255 to +255 for It, and larger for Sobel.
// Sobel kernel [-1 0 1], max value around 4*255 = 1020.
// Need at least 11 bits signed.
// Let's use 16-bit for simplicity.
typedef ap_fixed<16, 11, AP_RND, AP_SAT> grad_t;

// Accumulator type for intermediate calculations
// Core memory suggests Q24.16 (40-bit)
typedef ap_fixed<40, 24, AP_RND, AP_SAT> accum_t;

// Pixel type
typedef unsigned char pixel_t;

#endif
