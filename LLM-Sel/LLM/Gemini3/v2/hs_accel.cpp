#include "hs_config_params.h"

// Helper function to get pixel value with boundary check (Replicate Border)
static inline unsigned short get_pixel(unsigned short *img, int y, int x, int height, int width) {
    #pragma HLS INLINE
    int r = y;
    int c = x;
    if (r < 0) r = 0;
    if (r >= height) r = height - 1;
    if (c < 0) c = 0;
    if (c >= width) c = width - 1;
    return img[r * width + c];
}

void hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
) {
    // Port Interfaces
    #pragma HLS INTERFACE m_axi port=inp1_img offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=inp2_img offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=vx_img   offset=slave bundle=gmem2
    #pragma HLS INTERFACE m_axi port=vy_img   offset=slave bundle=gmem3
    #pragma HLS INTERFACE s_axilite port=height
    #pragma HLS INTERFACE s_axilite port=width
    #pragma HLS INTERFACE s_axilite port=return

    // Local Buffers (BRAM)
    static grad_t buf_Ix[MAX_HEIGHT * MAX_WIDTH];
    static grad_t buf_Iy[MAX_HEIGHT * MAX_WIDTH];
    static grad_t buf_It[MAX_HEIGHT * MAX_WIDTH];
    
    static flow_t buf_u[MAX_HEIGHT * MAX_WIDTH];
    static flow_t buf_v[MAX_HEIGHT * MAX_WIDTH];
    static flow_t buf_u_next[MAX_HEIGHT * MAX_WIDTH];
    static flow_t buf_v_next[MAX_HEIGHT * MAX_WIDTH];

    // 1. Compute Gradients and Initialize Flow
    GRAD_LOOP: for (int i = 0; i < MAX_HEIGHT; i++) {
        for (int j = 0; j < MAX_WIDTH; j++) {
            #pragma HLS PIPELINE II=1
            if (i >= height || j >= width) continue;
            
            int idx = i * width + j;

            // Replicate border handling
            int rm1 = (i > 0) ? i - 1 : 0;
            int rp1 = (i < height - 1) ? i + 1 : height - 1;
            int cm1 = (j > 0) ? j - 1 : 0;
            int cp1 = (j < width - 1) ? j + 1 : width - 1;

            unsigned short val_m1_m1 = inp1_img[rm1 * width + cm1];
            unsigned short val_m1_0  = inp1_img[rm1 * width + j];
            unsigned short val_m1_p1 = inp1_img[rm1 * width + cp1];
            
            unsigned short val_0_m1  = inp1_img[i * width + cm1];
            unsigned short val_0_p1  = inp1_img[i * width + cp1];
            
            unsigned short val_p1_m1 = inp1_img[rp1 * width + cm1];
            unsigned short val_p1_0  = inp1_img[rp1 * width + j];
            unsigned short val_p1_p1 = inp1_img[rp1 * width + cp1];

            // Sobel X
            int gx = -val_m1_m1 + val_m1_p1 - 2*val_0_m1 + 2*val_0_p1 - val_p1_m1 + val_p1_p1;
            
            // Sobel Y
            int gy = -val_m1_m1 - 2*val_m1_0 - val_m1_p1 + val_p1_m1 + 2*val_p1_0 + val_p1_p1;

            // It = I2 - I1
            int gt = (int)inp2_img[idx] - (int)inp1_img[idx];

            buf_Ix[idx] = (grad_t)gx;
            buf_Iy[idx] = (grad_t)gy;
            buf_It[idx] = (grad_t)gt;

            buf_u[idx] = 0;
            buf_v[idx] = 0;
        }
    }

    // 2. Iterative Update
    ITER_LOOP: for (int k = 0; k < N_ITER; k++) {
        UPDATE_LOOP: for (int i = 0; i < MAX_HEIGHT; i++) {
            for (int j = 0; j < MAX_WIDTH; j++) {
                #pragma HLS PIPELINE II=1
                if (i >= height || j >= width) continue;

                int idx = i * width + j;

                // Neighbor Average
                int rm1 = (i > 0) ? i - 1 : 0;
                int rp1 = (i < height - 1) ? i + 1 : height - 1;
                int cm1 = (j > 0) ? j - 1 : 0;
                int cp1 = (j < width - 1) ? j + 1 : width - 1;

                // Use calc_t for summation to avoid potential overflow in 16-bit
                calc_t u_sum = (calc_t)buf_u[rm1 * width + j] + 
                               (calc_t)buf_u[rp1 * width + j] + 
                               (calc_t)buf_u[i * width + cm1] + 
                               (calc_t)buf_u[i * width + cp1];
                
                calc_t v_sum = (calc_t)buf_v[rm1 * width + j] + 
                               (calc_t)buf_v[rp1 * width + j] + 
                               (calc_t)buf_v[i * width + cm1] + 
                               (calc_t)buf_v[i * width + cp1];

                // Average: sum * 0.25
                calc_t u_avg = u_sum * (calc_t)0.25;
                calc_t v_avg = v_sum * (calc_t)0.25;

                grad_t ix = buf_Ix[idx];
                grad_t iy = buf_Iy[idx];
                grad_t it = buf_It[idx];

                // Denominator
                // Ensure accumulation happens in den_t (32-bit)
                den_t den = (den_t)(ALPHA_VAL*ALPHA_VAL) + (den_t)ix*(den_t)ix + (den_t)iy*(den_t)iy;
                if (den == 0) den = 1;

                // Numerator term
                // Using calc_t (40,20) prevents overflow of Ix*u_avg
                calc_t term = (calc_t)ix * u_avg + (calc_t)iy * v_avg + (calc_t)it;
                
                // Division
                calc_t div_val = term / (calc_t)den;

                buf_u_next[idx] = (flow_t)(u_avg - (calc_t)ix * div_val);
                buf_v_next[idx] = (flow_t)(v_avg - (calc_t)iy * div_val);
            }
        }

        // Copy next to curr
        COPY_LOOP: for (int i = 0; i < MAX_HEIGHT * MAX_WIDTH; i++) {
             #pragma HLS PIPELINE II=1
             if (i < height * width) {
                buf_u[i] = buf_u_next[i];
                buf_v[i] = buf_v_next[i];
             }
        }
    }

    // 3. Write Output
    WRITE_LOOP: for (int i = 0; i < MAX_HEIGHT * MAX_WIDTH; i++) {
        #pragma HLS PIPELINE II=1
        if (i < height * width) {
             vx_img[i] = (short)buf_u[i].range(15, 0);
             vy_img[i] = (short)buf_v[i].range(15, 0);
        }
    }
}
