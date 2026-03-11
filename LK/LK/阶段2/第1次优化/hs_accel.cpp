#include "hs_config_params.h"

// Horn-Schunck Optical Flow Accelerator
// Inputs: inp1_img, inp2_img (Grayscale, 8-bit stored in unsigned short)
// Outputs: vx_img, vy_img (Dense flow, signed short fixed-point Q6.10)
// Parameters: height, width
int hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
) {
    #pragma HLS INTERFACE m_axi port=inp1_img bundle=gmem0 offset=slave depth=32768
    #pragma HLS INTERFACE m_axi port=inp2_img bundle=gmem1 offset=slave depth=32768
    #pragma HLS INTERFACE m_axi port=vx_img bundle=gmem2 offset=slave depth=32768
    #pragma HLS INTERFACE m_axi port=vy_img bundle=gmem3 offset=slave depth=32768
    #pragma HLS INTERFACE s_axilite port=height
    #pragma HLS INTERFACE s_axilite port=width
    #pragma HLS INTERFACE s_axilite port=return

    static grad_t buff_Ix[MAX_HEIGHT][MAX_WIDTH];
    static grad_t buff_Iy[MAX_HEIGHT][MAX_WIDTH];
    static grad_t buff_It[MAX_HEIGHT][MAX_WIDTH];

    static flow_t buff_u[MAX_HEIGHT][MAX_WIDTH];
    static flow_t buff_v[MAX_HEIGHT][MAX_WIDTH];
    static flow_t buff_u_next[MAX_HEIGHT][MAX_WIDTH];
    static flow_t buff_v_next[MAX_HEIGHT][MAX_WIDTH];

    static pixel_t row_buf0[MAX_WIDTH];
    static pixel_t row_buf1[MAX_WIDTH];
    static pixel_t row_buf2[MAX_WIDTH];

    pixel_t* row_above = row_buf0;
    pixel_t* row_curr  = row_buf1;
    pixel_t* row_below = row_buf2;

    if (height != MAX_HEIGHT || width != MAX_WIDTH) {
        return 1;
    }

    init_rows_y: for (int i = 0; i < MAX_HEIGHT; i++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        init_rows_x: for (int j = 0; j < MAX_WIDTH; j++) {
            #pragma HLS LOOP_TRIPCOUNT min=256 max=256
            #pragma HLS PIPELINE II=1
            buff_u[i][j] = 0;
            buff_v[i][j] = 0;
        }
    }

    read_row0: for (int j = 0; j < MAX_WIDTH; j++) {
        #pragma HLS LOOP_TRIPCOUNT min=256 max=256
        #pragma HLS PIPELINE II=1
        unsigned short int p1 = inp1_img[j];
        unsigned short int p2 = inp2_img[j];
        row_curr[j] = (pixel_t)p1;
        buff_It[0][j] = (grad_t)((int)p2 - (int)p1);
    }

    copy_row0: for (int j = 0; j < MAX_WIDTH; j++) {
        #pragma HLS LOOP_TRIPCOUNT min=256 max=256
        #pragma HLS PIPELINE II=1
        row_above[j] = row_curr[j];
    }

    read_row1: for (int j = 0; j < MAX_WIDTH; j++) {
        #pragma HLS LOOP_TRIPCOUNT min=256 max=256
        #pragma HLS PIPELINE II=1
        unsigned short int p1 = inp1_img[MAX_WIDTH + j];
        unsigned short int p2 = inp2_img[MAX_WIDTH + j];
        row_below[j] = (pixel_t)p1;
        buff_It[1][j] = (grad_t)((int)p2 - (int)p1);
    }

    grad_row0: for (int j = 0; j < MAX_WIDTH; j++) {
        #pragma HLS LOOP_TRIPCOUNT min=256 max=256
        #pragma HLS PIPELINE II=1
        int jm1 = (j == 0) ? 0 : (j - 1);
        int jp1 = (j == (MAX_WIDTH - 1)) ? (MAX_WIDTH - 1) : (j + 1);

        int val_m1_m1 = row_above[jm1];
        int val_m1_0  = row_above[j];
        int val_m1_p1 = row_above[jp1];

        int val_0_m1  = row_curr[jm1];
        int val_0_p1  = row_curr[jp1];

        int val_p1_m1 = row_below[jm1];
        int val_p1_0  = row_below[j];
        int val_p1_p1 = row_below[jp1];

        int gx = -val_m1_m1 + val_m1_p1 - 2 * val_0_m1 + 2 * val_0_p1 - val_p1_m1 + val_p1_p1;
        int gy = -val_m1_m1 - 2 * val_m1_0 - val_m1_p1 + val_p1_m1 + 2 * val_p1_0 + val_p1_p1;

        buff_Ix[0][j] = (grad_t)gx;
        buff_Iy[0][j] = (grad_t)gy;
    }

    grad_rows_y: for (int i = 1; i < MAX_HEIGHT - 1; i++) {
        #pragma HLS LOOP_TRIPCOUNT min=126 max=126
        int next_row = i + 1;
        pixel_t* recycle = row_above;
        row_above = row_curr;
        row_curr = row_below;
        row_below = recycle;

        read_next_row: for (int j = 0; j < MAX_WIDTH; j++) {
            #pragma HLS LOOP_TRIPCOUNT min=256 max=256
            #pragma HLS PIPELINE II=1
            unsigned short int p1 = inp1_img[next_row * MAX_WIDTH + j];
            unsigned short int p2 = inp2_img[next_row * MAX_WIDTH + j];
            row_below[j] = (pixel_t)p1;
            buff_It[next_row][j] = (grad_t)((int)p2 - (int)p1);
        }

        grad_row: for (int j = 0; j < MAX_WIDTH; j++) {
            #pragma HLS LOOP_TRIPCOUNT min=256 max=256
            #pragma HLS PIPELINE II=1
            int jm1 = (j == 0) ? 0 : (j - 1);
            int jp1 = (j == (MAX_WIDTH - 1)) ? (MAX_WIDTH - 1) : (j + 1);

            int val_m1_m1 = row_above[jm1];
            int val_m1_0  = row_above[j];
            int val_m1_p1 = row_above[jp1];

            int val_0_m1  = row_curr[jm1];
            int val_0_p1  = row_curr[jp1];

            int val_p1_m1 = row_below[jm1];
            int val_p1_0  = row_below[j];
            int val_p1_p1 = row_below[jp1];

            int gx = -val_m1_m1 + val_m1_p1 - 2 * val_0_m1 + 2 * val_0_p1 - val_p1_m1 + val_p1_p1;
            int gy = -val_m1_m1 - 2 * val_m1_0 - val_m1_p1 + val_p1_m1 + 2 * val_p1_0 + val_p1_p1;

            buff_Ix[i][j] = (grad_t)gx;
            buff_Iy[i][j] = (grad_t)gy;
        }
    }

    pixel_t* tmp_above = row_above;
    row_above = row_curr;
    row_curr = row_below;
    row_below = tmp_above;

    copy_last_row: for (int j = 0; j < MAX_WIDTH; j++) {
        #pragma HLS LOOP_TRIPCOUNT min=256 max=256
        #pragma HLS PIPELINE II=1
        row_below[j] = row_curr[j];
    }

    grad_last_row: for (int j = 0; j < MAX_WIDTH; j++) {
        #pragma HLS LOOP_TRIPCOUNT min=256 max=256
        #pragma HLS PIPELINE II=1
        int jm1 = (j == 0) ? 0 : (j - 1);
        int jp1 = (j == (MAX_WIDTH - 1)) ? (MAX_WIDTH - 1) : (j + 1);

        int val_m1_m1 = row_above[jm1];
        int val_m1_0  = row_above[j];
        int val_m1_p1 = row_above[jp1];

        int val_0_m1  = row_curr[jm1];
        int val_0_p1  = row_curr[jp1];

        int val_p1_m1 = row_below[jm1];
        int val_p1_0  = row_below[j];
        int val_p1_p1 = row_below[jp1];

        int gx = -val_m1_m1 + val_m1_p1 - 2 * val_0_m1 + 2 * val_0_p1 - val_p1_m1 + val_p1_p1;
        int gy = -val_m1_m1 - 2 * val_m1_0 - val_m1_p1 + val_p1_m1 + 2 * val_p1_0 + val_p1_p1;

        buff_Ix[MAX_HEIGHT - 1][j] = (grad_t)gx;
        buff_Iy[MAX_HEIGHT - 1][j] = (grad_t)gy;
    }

    iter_loop: for (int k = 0; k < N_ITER; k++) {
        #pragma HLS LOOP_TRIPCOUNT min=20 max=20
        #pragma HLS LOOP_FLATTEN off
        if ((k & 1) == 0) {
            calc_loop_y_even: for (int i = 0; i < MAX_HEIGHT; i++) {
                #pragma HLS LOOP_TRIPCOUNT min=128 max=128
                calc_loop_x_even: for (int j = 0; j < MAX_WIDTH; j++) {
                    #pragma HLS LOOP_TRIPCOUNT min=256 max=256
                    #pragma HLS PIPELINE II=1
                    int im1 = (i > 0) ? i - 1 : 0;
                    int ip1 = (i < MAX_HEIGHT - 1) ? i + 1 : MAX_HEIGHT - 1;
                    int jm1 = (j > 0) ? j - 1 : 0;
                    int jp1 = (j < MAX_WIDTH - 1) ? j + 1 : MAX_WIDTH - 1;

                    calc_t sum_u = (calc_t)buff_u[im1][j] + (calc_t)buff_u[ip1][j] + (calc_t)buff_u[i][jm1] + (calc_t)buff_u[i][jp1];
                    calc_t sum_v = (calc_t)buff_v[im1][j] + (calc_t)buff_v[ip1][j] + (calc_t)buff_v[i][jm1] + (calc_t)buff_v[i][jp1];

                    flow_t u_avg = (flow_t)(sum_u * (calc_t)0.25);
                    flow_t v_avg = (flow_t)(sum_v * (calc_t)0.25);

                    grad_t ix = buff_Ix[i][j];
                    grad_t iy = buff_Iy[i][j];
                    grad_t it = buff_It[i][j];

                    den_t den = ALPHA_VAL * ALPHA_VAL + ix * ix + iy * iy;
                    calc_t num_term = ix * u_avg + iy * v_avg + it;
                    calc_t update_val = num_term / den;

                    buff_u_next[i][j] = u_avg - ix * update_val;
                    buff_v_next[i][j] = v_avg - iy * update_val;
                }
            }
        } else {
            calc_loop_y_odd: for (int i = 0; i < MAX_HEIGHT; i++) {
                #pragma HLS LOOP_TRIPCOUNT min=128 max=128
                calc_loop_x_odd: for (int j = 0; j < MAX_WIDTH; j++) {
                    #pragma HLS LOOP_TRIPCOUNT min=256 max=256
                    #pragma HLS PIPELINE II=1
                    int im1 = (i > 0) ? i - 1 : 0;
                    int ip1 = (i < MAX_HEIGHT - 1) ? i + 1 : MAX_HEIGHT - 1;
                    int jm1 = (j > 0) ? j - 1 : 0;
                    int jp1 = (j < MAX_WIDTH - 1) ? j + 1 : MAX_WIDTH - 1;

                    calc_t sum_u = (calc_t)buff_u_next[im1][j] + (calc_t)buff_u_next[ip1][j] + (calc_t)buff_u_next[i][jm1] + (calc_t)buff_u_next[i][jp1];
                    calc_t sum_v = (calc_t)buff_v_next[im1][j] + (calc_t)buff_v_next[ip1][j] + (calc_t)buff_v_next[i][jm1] + (calc_t)buff_v_next[i][jp1];

                    flow_t u_avg = (flow_t)(sum_u * (calc_t)0.25);
                    flow_t v_avg = (flow_t)(sum_v * (calc_t)0.25);

                    grad_t ix = buff_Ix[i][j];
                    grad_t iy = buff_Iy[i][j];
                    grad_t it = buff_It[i][j];

                    den_t den = ALPHA_VAL * ALPHA_VAL + ix * ix + iy * iy;
                    calc_t num_term = ix * u_avg + iy * v_avg + it;
                    calc_t update_val = num_term / den;

                    buff_u[i][j] = u_avg - ix * update_val;
                    buff_v[i][j] = v_avg - iy * update_val;
                }
            }
        }
    }

    write_loop_y: for (int i = 0; i < MAX_HEIGHT; i++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        write_loop_x: for (int j = 0; j < MAX_WIDTH; j++) {
            #pragma HLS LOOP_TRIPCOUNT min=256 max=256
            #pragma HLS PIPELINE II=1
            flow_t u_val = ((N_ITER & 1) == 0) ? buff_u[i][j] : buff_u_next[i][j];
            flow_t v_val = ((N_ITER & 1) == 0) ? buff_v[i][j] : buff_v_next[i][j];
            vx_img[i * MAX_WIDTH + j] = (short)u_val.range(15, 0);
            vy_img[i * MAX_WIDTH + j] = (short)v_val.range(15, 0);
        }
    }
    return 0;
}
