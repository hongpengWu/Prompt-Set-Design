#ifndef HS_CONFIG_PARAMS_H
#define HS_CONFIG_PARAMS_H

#include "ap_fixed.h"
#include "ap_int.h"

#define MAX_HEIGHT 128
#define MAX_WIDTH 256
#define MAX_PIXELS (MAX_WIDTH * MAX_HEIGHT)
#define ALPHA_VAL 1
#define N_ITER 20

#define FLOW_FRAC_BITS 10

typedef ap_fixed<16, 6, AP_RND, AP_SAT> flow_t;
typedef unsigned short pixel_t;

typedef ap_int<12> grad_t;
typedef ap_int<24> den_t;
typedef ap_fixed<32, 16, AP_RND, AP_SAT> calc_t;

typedef ap_ufixed<32, 24, AP_RND, AP_SAT> den_fx_t;
typedef ap_ufixed<24, 2, AP_RND, AP_SAT> inv_t;

#define SOBEL_NORM_SHIFT 0
#endif
