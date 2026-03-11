#include "hs_config_params.h"

static void hs_update_pass(
    flow_t u_in[MAX_HEIGHT][MAX_WIDTH],
    flow_t v_in[MAX_HEIGHT][MAX_WIDTH],
    flow_t u_out[MAX_HEIGHT][MAX_WIDTH],
    flow_t v_out[MAX_HEIGHT][MAX_WIDTH],
    inv_t inv_den[MAX_HEIGHT][MAX_WIDTH],
    grad_t Ix[MAX_HEIGHT][MAX_WIDTH],
    grad_t Iy[MAX_HEIGHT][MAX_WIDTH],
    grad_t It[MAX_HEIGHT][MAX_WIDTH]
) {
    #pragma HLS INLINE

    update_y: for (int i = 0; i < MAX_HEIGHT; i++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        int im1 = (i > 0) ? (i - 1) : 0;
        int ip1 = (i < (MAX_HEIGHT - 1)) ? (i + 1) : (MAX_HEIGHT - 1);

        {
            int j = 0;
            calc_t sum_u = (calc_t)u_in[im1][j] + (calc_t)u_in[ip1][j] + (calc_t)u_in[i][j] + (calc_t)u_in[i][j + 1];
            calc_t sum_v = (calc_t)v_in[im1][j] + (calc_t)v_in[ip1][j] + (calc_t)v_in[i][j] + (calc_t)v_in[i][j + 1];

            flow_t u_avg = (flow_t)(sum_u * (calc_t)0.25);
            flow_t v_avg = (flow_t)(sum_v * (calc_t)0.25);

            grad_t ix = Ix[i][j];
            grad_t iy = Iy[i][j];
            grad_t it = It[i][j];

            calc_t num_term = ix * u_avg + iy * v_avg + it;
            calc_t update_val = num_term * inv_den[i][j];

            u_out[i][j] = u_avg - ix * update_val;
            v_out[i][j] = v_avg - iy * update_val;
        }

        update_x_interior: for (int j = 1; j < (MAX_WIDTH - 1); j++) {
            #pragma HLS LOOP_TRIPCOUNT min=254 max=254
            #pragma HLS PIPELINE II=1

            calc_t sum_u = (calc_t)u_in[im1][j] + (calc_t)u_in[ip1][j] + (calc_t)u_in[i][j - 1] + (calc_t)u_in[i][j + 1];
            calc_t sum_v = (calc_t)v_in[im1][j] + (calc_t)v_in[ip1][j] + (calc_t)v_in[i][j - 1] + (calc_t)v_in[i][j + 1];

            flow_t u_avg = (flow_t)(sum_u * (calc_t)0.25);
            flow_t v_avg = (flow_t)(sum_v * (calc_t)0.25);

            grad_t ix = Ix[i][j];
            grad_t iy = Iy[i][j];
            grad_t it = It[i][j];

            calc_t num_term = ix * u_avg + iy * v_avg + it;
            calc_t update_val = num_term * inv_den[i][j];

            u_out[i][j] = u_avg - ix * update_val;
            v_out[i][j] = v_avg - iy * update_val;
        }

        {
            int j = MAX_WIDTH - 1;
            calc_t sum_u = (calc_t)u_in[im1][j] + (calc_t)u_in[ip1][j] + (calc_t)u_in[i][j - 1] + (calc_t)u_in[i][j];
            calc_t sum_v = (calc_t)v_in[im1][j] + (calc_t)v_in[ip1][j] + (calc_t)v_in[i][j - 1] + (calc_t)v_in[i][j];

            flow_t u_avg = (flow_t)(sum_u * (calc_t)0.25);
            flow_t v_avg = (flow_t)(sum_v * (calc_t)0.25);

            grad_t ix = Ix[i][j];
            grad_t iy = Iy[i][j];
            grad_t it = It[i][j];

            calc_t num_term = ix * u_avg + iy * v_avg + it;
            calc_t update_val = num_term * inv_den[i][j];

            u_out[i][j] = u_avg - ix * update_val;
            v_out[i][j] = v_avg - iy * update_val;
        }
    }
}

// Structural Stage 1: Fused Pre-processing (S11)
static void load_and_compute_pre(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    grad_t buff_Ix[MAX_HEIGHT][MAX_WIDTH],
    grad_t buff_Iy[MAX_HEIGHT][MAX_WIDTH],
    grad_t buff_It[MAX_HEIGHT][MAX_WIDTH],
    inv_t  buff_invden[MAX_HEIGHT][MAX_WIDTH],
    flow_t buff_u[MAX_HEIGHT][MAX_WIDTH],
    flow_t buff_v[MAX_HEIGHT][MAX_WIDTH]
) {
    #pragma HLS INLINE off
    pixel_t row_buf[3][MAX_WIDTH];
    #pragma HLS ARRAY_PARTITION variable=row_buf complete dim=1

    // Initial Load (Row 0 and 1)
    for (int j = 0; j < MAX_WIDTH; j++) {
        #pragma HLS PIPELINE II=1
        pixel_t p1_0 = inp1_img[j];
        pixel_t p2_0 = inp2_img[j];
        row_buf[0][j] = p1_0;
        row_buf[1][j] = p1_0;
        buff_It[0][j] = (grad_t)((int)p2_0 - (int)p1_0);
    }
    for (int j = 0; j < MAX_WIDTH; j++) {
        #pragma HLS PIPELINE II=1
        pixel_t p1_1 = inp1_img[MAX_WIDTH + j];
        pixel_t p2_1 = inp2_img[MAX_WIDTH + j];
        row_buf[2][j] = p1_1;
        buff_It[1][j] = (grad_t)((int)p2_1 - (int)p1_1);
    }

    // Main fused loop
    for (int i = 0; i < MAX_HEIGHT; i++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        
        pixel_t a_l, a_c, a_r;
        pixel_t c_l, c_c, c_r;
        pixel_t b_l, b_c, b_r;

        a_l = row_buf[0][0]; a_c = row_buf[0][0]; a_r = row_buf[0][1];
        c_l = row_buf[1][0]; c_c = row_buf[1][0]; c_r = row_buf[1][1];
        b_l = row_buf[2][0]; b_c = row_buf[2][0]; b_r = row_buf[2][1];

        for (int j = 0; j < MAX_WIDTH; j++) {
            #pragma HLS PIPELINE II=1
            
            int gx = -(int)a_l + (int)a_r - 2*(int)c_l + 2*(int)c_r - (int)b_l + (int)b_r;
            int gy = -(int)a_l - 2*(int)a_c - (int)a_r + (int)b_l + 2*(int)b_c + (int)b_r;

            buff_Ix[i][j] = (grad_t)gx;
            buff_Iy[i][j] = (grad_t)gy;
            buff_u[i][j] = 0;
            buff_v[i][j] = 0;

            den_t den = ALPHA_VAL * ALPHA_VAL + gx * gx + gy * gy;
            den_fx_t den_fx = (den_fx_t)den;
            ap_ufixed<32, 2, AP_RND, AP_SAT> one = 1.0;
            buff_invden[i][j] = (inv_t)(one / den_fx);

            int next_j = (j + 2 < MAX_WIDTH) ? (j + 2) : (MAX_WIDTH - 1);
            a_l = a_c; a_c = a_r; a_r = row_buf[0][next_j];
            c_l = c_c; c_c = c_r; c_r = row_buf[1][next_j];
            b_l = b_c; b_c = b_r; b_r = row_buf[2][next_j];
        }

        if (i < MAX_HEIGHT - 2) {
            int next_row_idx = i + 2;
            for (int j = 0; j < MAX_WIDTH; j++) {
                #pragma HLS PIPELINE II=1
                pixel_t p1 = inp1_img[next_row_idx * MAX_WIDTH + j];
                pixel_t p2 = inp2_img[next_row_idx * MAX_WIDTH + j];
                row_buf[0][j] = row_buf[1][j];
                row_buf[1][j] = row_buf[2][j];
                row_buf[2][j] = p1;
                buff_It[next_row_idx][j] = (grad_t)((int)p2 - (int)p1);
            }
        } else {
            for (int j = 0; j < MAX_WIDTH; j++) {
                #pragma HLS PIPELINE II=1
                row_buf[0][j] = row_buf[1][j];
                row_buf[1][j] = row_buf[2][j];
            }
        }
    }
}

static void compute_iterations(
    grad_t buff_Ix[MAX_HEIGHT][MAX_WIDTH],
    grad_t buff_Iy[MAX_HEIGHT][MAX_WIDTH],
    grad_t buff_It[MAX_HEIGHT][MAX_WIDTH],
    inv_t  buff_invden[MAX_HEIGHT][MAX_WIDTH],
    flow_t buff_u[MAX_HEIGHT][MAX_WIDTH],
    flow_t buff_v[MAX_HEIGHT][MAX_WIDTH],
    flow_t buff_u_next[MAX_HEIGHT][MAX_WIDTH],
    flow_t buff_v_next[MAX_HEIGHT][MAX_WIDTH]
) {
    #pragma HLS INLINE off
    iter_loop: for (int k = 0; k < N_ITER; k++) {
        #pragma HLS LOOP_TRIPCOUNT min=20 max=20
        #pragma HLS LOOP_FLATTEN off
        if ((k & 1) == 0) {
            hs_update_pass(buff_u, buff_v, buff_u_next, buff_v_next, buff_invden, buff_Ix, buff_Iy, buff_It);
        } else {
            hs_update_pass(buff_u_next, buff_v_next, buff_u, buff_v, buff_invden, buff_Ix, buff_Iy, buff_It);
        }
    }
}

static void store_results(
    flow_t buff_u[MAX_HEIGHT][MAX_WIDTH],
    flow_t buff_v[MAX_HEIGHT][MAX_WIDTH],
    flow_t buff_u_next[MAX_HEIGHT][MAX_WIDTH],
    flow_t buff_v_next[MAX_HEIGHT][MAX_WIDTH],
    signed short int vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int vy_img[MAX_HEIGHT*MAX_WIDTH]
) {
    #pragma HLS INLINE off
    write_loop_y: for (int i = 0; i < MAX_HEIGHT; i++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        write_loop_x: for (int j = 0; j < MAX_WIDTH; j++) {
            #pragma HLS PIPELINE II=1
            flow_t u_val = ((N_ITER & 1) == 0) ? buff_u[i][j] : buff_u_next[i][j];
            flow_t v_val = ((N_ITER & 1) == 0) ? buff_v[i][j] : buff_v_next[i][j];
            vx_img[i * MAX_WIDTH + j] = (short)u_val.range(15, 0);
            vy_img[i * MAX_WIDTH + j] = (short)v_val.range(15, 0);
        }
    }
}

int hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
) {
    #pragma HLS INTERFACE m_axi port=inp1_img bundle=gmem0 offset=slave depth=32768 max_widen_bitwidth=128
    #pragma HLS INTERFACE m_axi port=inp2_img bundle=gmem1 offset=slave depth=32768 max_widen_bitwidth=128
    #pragma HLS INTERFACE m_axi port=vx_img   bundle=gmem2 offset=slave depth=32768 max_widen_bitwidth=128
    #pragma HLS INTERFACE m_axi port=vy_img   bundle=gmem3 offset=slave depth=32768 max_widen_bitwidth=128
    #pragma HLS INTERFACE s_axilite port=inp1_img bundle=control
    #pragma HLS INTERFACE s_axilite port=inp2_img bundle=control
    #pragma HLS INTERFACE s_axilite port=vx_img   bundle=control
    #pragma HLS INTERFACE s_axilite port=vy_img   bundle=control
    #pragma HLS INTERFACE s_axilite port=height   bundle=control
    #pragma HLS INTERFACE s_axilite port=width    bundle=control
    #pragma HLS INTERFACE s_axilite port=return   bundle=control

    static grad_t buff_Ix[MAX_HEIGHT][MAX_WIDTH];
    static grad_t buff_Iy[MAX_HEIGHT][MAX_WIDTH];
    static grad_t buff_It[MAX_HEIGHT][MAX_WIDTH];
    static inv_t  buff_invden[MAX_HEIGHT][MAX_WIDTH];
    static flow_t buff_u[MAX_HEIGHT][MAX_WIDTH];
    static flow_t buff_v[MAX_HEIGHT][MAX_WIDTH];
    static flow_t buff_u_next[MAX_HEIGHT][MAX_WIDTH];
    static flow_t buff_v_next[MAX_HEIGHT][MAX_WIDTH];

    #pragma HLS ARRAY_PARTITION variable=buff_u cyclic factor=4 dim=1
    #pragma HLS ARRAY_PARTITION variable=buff_u cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=buff_v cyclic factor=4 dim=1
    #pragma HLS ARRAY_PARTITION variable=buff_v cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=buff_u_next cyclic factor=4 dim=1
    #pragma HLS ARRAY_PARTITION variable=buff_u_next cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=buff_v_next cyclic factor=4 dim=1
    #pragma HLS ARRAY_PARTITION variable=buff_v_next cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=buff_Ix cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=buff_Iy cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=buff_It cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=buff_invden cyclic factor=4 dim=2

    if (height != MAX_HEIGHT || width != MAX_WIDTH) {
        return 1;
    }

    #pragma HLS DATAFLOW
    load_and_compute_pre(inp1_img, inp2_img, buff_Ix, buff_Iy, buff_It, buff_invden, buff_u, buff_v);
    compute_iterations(buff_Ix, buff_Iy, buff_It, buff_invden, buff_u, buff_v, buff_u_next, buff_v_next);
    store_results(buff_u, buff_v, buff_u_next, buff_v_next, vx_img, vy_img);

    return 0;
}
