#include "hs_config_params.h"

void hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
) {
    #pragma HLS INTERFACE port=inp1_img m_axi depth=32768 offset=slave bundle=gmem1
    #pragma HLS INTERFACE port=inp2_img m_axi depth=32768 offset=slave bundle=gmem2
    #pragma HLS INTERFACE port=vx_img m_axi depth=32768 offset=slave bundle=gmem3
    #pragma HLS INTERFACE port=vy_img m_axi depth=32768 offset=slave bundle=gmem4
    #pragma HLS INTERFACE s_axilite port=height
    #pragma HLS INTERFACE s_axilite port=width
    #pragma HLS INTERFACE s_axilite port=return

    // Local Buffers
    grad_t buff_Ix[MAX_HEIGHT][MAX_WIDTH];
    grad_t buff_Iy[MAX_HEIGHT][MAX_WIDTH];
    grad_t buff_It[MAX_HEIGHT][MAX_WIDTH];
    
    flow_t buff_u[MAX_HEIGHT][MAX_WIDTH];
    flow_t buff_v[MAX_HEIGHT][MAX_WIDTH];
    flow_t buff_u_next[MAX_HEIGHT][MAX_WIDTH];
    flow_t buff_v_next[MAX_HEIGHT][MAX_WIDTH];
    
    unsigned short img1[MAX_HEIGHT][MAX_WIDTH];
    unsigned short img2[MAX_HEIGHT][MAX_WIDTH];

    #pragma HLS BRAM variable=buff_Ix
    #pragma HLS BRAM variable=buff_Iy
    #pragma HLS BRAM variable=buff_It
    #pragma HLS BRAM variable=buff_u
    #pragma HLS BRAM variable=buff_v
    #pragma HLS BRAM variable=buff_u_next
    #pragma HLS BRAM variable=buff_v_next
    #pragma HLS BRAM variable=img1
    #pragma HLS BRAM variable=img2

    // 1. Load Images
    LOAD_LOOP: for(int i=0; i<MAX_HEIGHT; i++) {
        for(int j=0; j<MAX_WIDTH; j++) {
            #pragma HLS PIPELINE II=1
            if(i < height && j < width) {
                img1[i][j] = inp1_img[i*width + j];
                img2[i][j] = inp2_img[i*width + j];
            } else {
                img1[i][j] = 0;
                img2[i][j] = 0;
            }
        }
    }

    // 2. Compute Gradients and Initialize Flow
    GRAD_LOOP: for(int i=0; i<MAX_HEIGHT; i++) {
        for(int j=0; j<MAX_WIDTH; j++) {
            #pragma HLS PIPELINE II=1
            if (i >= height || j >= width) continue;

            // Replicate boundary
            int im1 = (i > 0) ? i - 1 : 0;
            int ip1 = (i < height - 1) ? i + 1 : height - 1;
            int jm1 = (j > 0) ? j - 1 : 0;
            int jp1 = (j < width - 1) ? j + 1 : width - 1;

            grad_t val_m1_m1 = img1[im1][jm1];
            grad_t val_m1_p1 = img1[im1][jp1];
            grad_t val_0_m1  = img1[i][jm1];
            grad_t val_0_p1  = img1[i][jp1];
            grad_t val_p1_m1 = img1[ip1][jm1];
            grad_t val_p1_p1 = img1[ip1][jp1];
            grad_t val_m1_0  = img1[im1][j];
            grad_t val_p1_0  = img1[ip1][j];

            // Sobel X
            grad_t gx = -val_m1_m1 + val_m1_p1 - 2*val_0_m1 + 2*val_0_p1 - val_p1_m1 + val_p1_p1;
            
            // Sobel Y
            grad_t gy = -val_m1_m1 - 2*val_m1_0 - val_m1_p1 + val_p1_m1 + 2*val_p1_0 + val_p1_p1;
            
            // It = I2 - I1
            grad_t gt = (grad_t)img2[i][j] - (grad_t)img1[i][j];

            buff_Ix[i][j] = gx;
            buff_Iy[i][j] = gy;
            buff_It[i][j] = gt;
            
            buff_u[i][j] = 0;
            buff_v[i][j] = 0;
        }
    }

    // 3. Iterations
    ITER_LOOP: for(int k=0; k<N_ITER; k++) {
        PIXEL_LOOP: for(int i=0; i<MAX_HEIGHT; i++) {
            for(int j=0; j<MAX_WIDTH; j++) {
                #pragma HLS PIPELINE II=1
                if (i >= height || j >= width) continue;

                int im1 = (i > 0) ? i - 1 : 0;
                int ip1 = (i < height - 1) ? i + 1 : height - 1;
                int jm1 = (j > 0) ? j - 1 : 0;
                int jp1 = (j < width - 1) ? j + 1 : width - 1;

                // 4-neighbor average
                flow_t u_avg = (buff_u[im1][j] + buff_u[ip1][j] + buff_u[i][jm1] + buff_u[i][jp1]) * (flow_t)0.25;
                flow_t v_avg = (buff_v[im1][j] + buff_v[ip1][j] + buff_v[i][jm1] + buff_v[i][jp1]) * (flow_t)0.25;

                grad_t ix = buff_Ix[i][j];
                grad_t iy = buff_Iy[i][j];
                grad_t it = buff_It[i][j];

                accum_t ix2 = ix * ix;
                accum_t iy2 = iy * iy;
                accum_t alpha2 = ALPHA_VAL * ALPHA_VAL;
                accum_t den = alpha2 + ix2 + iy2;
                
                accum_t num = ix * u_avg + iy * v_avg + it;
                
                accum_t div_res;
                if (den == 0) div_res = 0; 
                else div_res = num / den;

                buff_u_next[i][j] = u_avg - ix * div_res;
                buff_v_next[i][j] = v_avg - iy * div_res;
            }
        }
        
        // Copy next to curr
        COPY_LOOP: for(int i=0; i<MAX_HEIGHT; i++) {
            for(int j=0; j<MAX_WIDTH; j++) {
                #pragma HLS PIPELINE II=1
                if (i >= height || j >= width) continue;
                buff_u[i][j] = buff_u_next[i][j];
                buff_v[i][j] = buff_v_next[i][j];
            }
        }
    }

    // 4. Write Output
    WRITE_LOOP: for(int i=0; i<MAX_HEIGHT; i++) {
        for(int j=0; j<MAX_WIDTH; j++) {
            #pragma HLS PIPELINE II=1
            if (i < height && j < width) {
                // Return raw bits
                vx_img[i*width + j] = buff_u[i][j].range(15, 0);
                vy_img[i*width + j] = buff_v[i][j].range(15, 0);
            }
        }
    }
}
