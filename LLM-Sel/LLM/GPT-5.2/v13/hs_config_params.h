#ifndef HS_CONFIG_PARAMS_H
#define HS_CONFIG_PARAMS_H

#include "ap_int.h"
#include "ap_fixed.h"

#define MAX_HEIGHT 128
#define MAX_WIDTH 256

#define N_ITER 100

#define ALPHA_VAL 1
#define ALPHA_SQUARED (ALPHA_VAL * ALPHA_VAL)

#define FLOW_FRAC_BITS 8
#define FLOW_SCALE (1 << FLOW_FRAC_BITS)

typedef unsigned short pixel_t;

typedef ap_int<16> grad_i16_t;
typedef ap_fixed<16, 8, AP_RND, AP_SAT> flow_t;
typedef ap_fixed<40, 24, AP_RND, AP_SAT> acc_t;

#endif
