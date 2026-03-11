#include "hs_config_params.h"

static pixel_t buf_in1[MAX_HEIGHT][MAX_WIDTH];
static pixel_t buf_in2[MAX_HEIGHT][MAX_WIDTH];
static grad_i16_t buf_Ix[MAX_HEIGHT][MAX_WIDTH];
static grad_i16_t buf_Iy[MAX_HEIGHT][MAX_WIDTH];
static grad_i16_t buf_It[MAX_HEIGHT][MAX_WIDTH];
static flow_t buf_u[MAX_HEIGHT][MAX_WIDTH];
static flow_t buf_v[MAX_HEIGHT][MAX_WIDTH];
static flow_t buf_u_new[MAX_HEIGHT][MAX_WIDTH];
static flow_t buf_v_new[MAX_HEIGHT][MAX_WIDTH];

static inline grad_i16_t sobel_x(
    pixel_t p00, pixel_t p01, pixel_t p02,
    pixel_t p10, pixel_t p11, pixel_t p12,
    pixel_t p20, pixel_t p21, pixel_t p22
) {
    int gx = -((int)p00) + ((int)p02) - (2 * (int)p10) + (2 * (int)p12) - ((int)p20) + ((int)p22);
    return (grad_i16_t)gx;
}

static inline grad_i16_t sobel_y(
    pixel_t p00, pixel_t p01, pixel_t p02,
    pixel_t p10, pixel_t p11, pixel_t p12,
    pixel_t p20, pixel_t p21, pixel_t p22
) {
    int gy = -((int)p00) - (2 * (int)p01) - ((int)p02) + ((int)p20) + (2 * (int)p21) + ((int)p22);
    return (grad_i16_t)gy;
}

int hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT * MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT * MAX_WIDTH],
    signed short int vx_img[MAX_HEIGHT * MAX_WIDTH],
    signed short int vy_img[MAX_HEIGHT * MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
) {
    #pragma HLS INTERFACE m_axi port=inp1_img offset=slave depth=32768 bundle=gmem0
    #pragma HLS INTERFACE m_axi port=inp2_img offset=slave depth=32768 bundle=gmem1
    #pragma HLS INTERFACE m_axi port=vx_img   offset=slave depth=32768 bundle=gmem2
    #pragma HLS INTERFACE m_axi port=vy_img   offset=slave depth=32768 bundle=gmem3
    #pragma HLS INTERFACE s_axilite port=height
    #pragma HLS INTERFACE s_axilite port=width
    #pragma HLS INTERFACE s_axilite port=return

    unsigned short rows = (height < MAX_HEIGHT) ? height : MAX_HEIGHT;
    unsigned short cols = (width < MAX_WIDTH) ? width : MAX_WIDTH;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            #pragma HLS PIPELINE II=1
            buf_in1[i][j] = (pixel_t)inp1_img[i * cols + j];
            buf_in2[i][j] = (pixel_t)inp2_img[i * cols + j];
        }
    }

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            #pragma HLS PIPELINE II=1
            int im1 = (i > 0) ? (i - 1) : 0;
            int ip1 = (i < (int)rows - 1) ? (i + 1) : ((int)rows - 1);
            int jm1 = (j > 0) ? (j - 1) : 0;
            int jp1 = (j < (int)cols - 1) ? (j + 1) : ((int)cols - 1);

            pixel_t p00 = buf_in1[im1][jm1];
            pixel_t p01 = buf_in1[im1][j];
            pixel_t p02 = buf_in1[im1][jp1];
            pixel_t p10 = buf_in1[i][jm1];
            pixel_t p11 = buf_in1[i][j];
            pixel_t p12 = buf_in1[i][jp1];
            pixel_t p20 = buf_in1[ip1][jm1];
            pixel_t p21 = buf_in1[ip1][j];
            pixel_t p22 = buf_in1[ip1][jp1];

            buf_Ix[i][j] = sobel_x(p00, p01, p02, p10, p11, p12, p20, p21, p22);
            buf_Iy[i][j] = sobel_y(p00, p01, p02, p10, p11, p12, p20, p21, p22);
            buf_It[i][j] = (grad_i16_t)((int)buf_in2[i][j] - (int)buf_in1[i][j]);
        }
    }

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            #pragma HLS PIPELINE II=1
            buf_u[i][j] = 0;
            buf_v[i][j] = 0;
        }
    }

    for (int iter = 0; iter < N_ITER; iter++) {
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                #pragma HLS PIPELINE II=1
                int im1 = (i > 0) ? (i - 1) : 0;
                int ip1 = (i < (int)rows - 1) ? (i + 1) : ((int)rows - 1);
                int jm1 = (j > 0) ? (j - 1) : 0;
                int jp1 = (j < (int)cols - 1) ? (j + 1) : ((int)cols - 1);

                flow_t u_bar = (buf_u[im1][j] + buf_u[ip1][j] + buf_u[i][jm1] + buf_u[i][jp1]) * (flow_t)0.25;
                flow_t v_bar = (buf_v[im1][j] + buf_v[ip1][j] + buf_v[i][jm1] + buf_v[i][jp1]) * (flow_t)0.25;

                grad_i16_t ix_i = buf_Ix[i][j];
                grad_i16_t iy_i = buf_Iy[i][j];
                grad_i16_t it_i = buf_It[i][j];

                acc_t ix = (acc_t)ix_i;
                acc_t iy = (acc_t)iy_i;
                acc_t it = (acc_t)it_i;

                acc_t den = (acc_t)ALPHA_SQUARED + ix * ix + iy * iy;
                acc_t num = ix * (acc_t)u_bar + iy * (acc_t)v_bar + it;
                acc_t factor = num / den;

                buf_u_new[i][j] = u_bar - (flow_t)(ix * factor);
                buf_v_new[i][j] = v_bar - (flow_t)(iy * factor);
            }
        }

        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                #pragma HLS PIPELINE II=1
                buf_u[i][j] = buf_u_new[i][j];
                buf_v[i][j] = buf_v_new[i][j];
            }
        }
    }

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            #pragma HLS PIPELINE II=1
            vx_img[i * cols + j] = (signed short)buf_u[i][j].range(15, 0);
            vy_img[i * cols + j] = (signed short)buf_v[i][j].range(15, 0);
        }
    }

    return 0;
}

