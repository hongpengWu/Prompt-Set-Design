#ifndef HS_CONFIG_PARAMS_H
#define HS_CONFIG_PARAMS_H

#include "ap_fixed.h"
#include "ap_int.h"

#define MAX_HEIGHT 128
#define MAX_WIDTH 256

#define ALPHA_VAL 1
#define N_ITER 20

#define FLOW_FRAC_BITS 10

#define FLOW_VIS_STEP 10
#define FLOW_ARROW_SCALE 5.0f

typedef ap_fixed<16, 6, AP_RND, AP_SAT> flow_t;
typedef unsigned short pixel_t;
typedef ap_int<12> grad_t;
typedef ap_int<24> den_t;
typedef ap_fixed<32, 16, AP_RND, AP_SAT> calc_t;

int hls_HS(unsigned short int inp1_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int inp2_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vx_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vy_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int height,
           unsigned short int width);

#endif
