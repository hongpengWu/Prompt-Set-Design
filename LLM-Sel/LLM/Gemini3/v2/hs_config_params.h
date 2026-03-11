#ifndef HS_CONFIG_PARAMS_H
#define HS_CONFIG_PARAMS_H

#include "ap_int.h"
#include "ap_fixed.h"

// Image Dimensions
#define MAX_HEIGHT 128
#define MAX_WIDTH 256

// Algorithm parameters
#define ALPHA_VAL 1
#define N_ITER 20

// Fixed Point Configuration

// Output format: Q6.10
// - 1 sign bit
// - 5 integer bits (Range +/- 32 pixels)
// - 10 fractional bits
#define FLOW_FRAC_BITS 10

// Internal Flow Type (ap_fixed<W, I>)
// Keeping this as is for interface/storage density, 
// but intermediate calcs will use higher precision.
typedef ap_fixed<16, 6, AP_RND, AP_SAT> flow_t;

// Input pixel type (8-bit grayscale)
typedef unsigned short pixel_t;

// Gradient type
// Sobel max ~1020. 
// Increased to 16-bit to be absolutely safe and align with standard types.
typedef ap_int<16> grad_t;

// Denominator type: alpha^2 + Ix^2 + Iy^2
// Max ~ 2*10^6. 
// Increased to 32-bit to avoid any overflow concerns.
typedef ap_int<32> den_t;

// Temporary calculation type
// Previously <32, 16>. Ix*u_avg could reach ~32000, risking overflow.
// Increased to <40, 20>:
// - 20 integer bits (Range +/- 524,288) -> Plenty for Ix*u_avg
// - 20 fractional bits -> Good precision for division result
typedef ap_fixed<40, 20, AP_RND, AP_SAT> calc_t;

#endif
