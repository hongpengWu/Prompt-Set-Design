#include "hs_config_params.h"

void hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
) {
    // Port Interfaces
    // Using AXI4 Master for memory access
    #pragma HLS INTERFACE m_axi port=inp1_img offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=inp2_img offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=vx_img   offset=slave bundle=gmem2
    #pragma HLS INTERFACE m_axi port=vy_img   offset=slave bundle=gmem3
    #pragma HLS INTERFACE s_axilite port=height
    #pragma HLS INTERFACE s_axilite port=width
    #pragma HLS INTERFACE s_axilite port=return

    // Local Buffers (BRAM) to store gradients and flow fields
    static grad_t buf_Ix[MAX_HEIGHT * MAX_WIDTH];
    static grad_t buf_Iy[MAX_HEIGHT * MAX_WIDTH];
    static grad_t buf_It[MAX_HEIGHT * MAX_WIDTH];
    
    static flow_t buf_u[MAX_HEIGHT * MAX_WIDTH];
    static flow_t buf_v[MAX_HEIGHT * MAX_WIDTH];
    static flow_t buf_u_next[MAX_HEIGHT * MAX_WIDTH];
    static flow_t buf_v_next[MAX_HEIGHT * MAX_WIDTH];

    // Local Image Buffers
    static pixel_t local_im1[MAX_HEIGHT * MAX_WIDTH];
    static pixel_t local_im2[MAX_HEIGHT * MAX_WIDTH];

    // Cache height/width
    int rows = (int)height;
    int cols = (int)width;
    if (rows > MAX_HEIGHT) rows = MAX_HEIGHT;
    if (cols > MAX_WIDTH) cols = MAX_WIDTH;

    // 1. Load Input Images
    LOAD_IMG: for(int i=0; i<MAX_HEIGHT*MAX_WIDTH; i++) {
        #pragma HLS PIPELINE II=1
        if(i < rows*cols) {
            local_im1[i] = inp1_img[i];
            local_im2[i] = inp2_img[i];
        }
    }

    // 2. Compute Gradients (Ix, Iy, It) and Initialize Flow
    CALC_GRAD: for (int i = 0; i < MAX_HEIGHT; i++) {
        for (int j = 0; j < MAX_WIDTH; j++) {
            #pragma HLS PIPELINE II=1
            if (i >= rows || j >= cols) continue;
            
            int idx = i * cols + j;

            // Boundary indices (Replication)
            int rm1 = (i > 0) ? i - 1 : 0;
            int rp1 = (i < rows - 1) ? i + 1 : rows - 1;
            int cm1 = (j > 0) ? j - 1 : 0;
            int cp1 = (j < cols - 1) ? j + 1 : cols - 1;

            // Access from local BRAM
            int val_m1_m1 = local_im1[rm1 * cols + cm1];
            int val_m1_0  = local_im1[rm1 * cols + j];
            int val_m1_p1 = local_im1[rm1 * cols + cp1];
            
            int val_0_m1  = local_im1[i * cols + cm1];
            int val_0_p1  = local_im1[i * cols + cp1];
            
            int val_p1_m1 = local_im1[rp1 * cols + cm1];
            int val_p1_0  = local_im1[rp1 * cols + j];
            int val_p1_p1 = local_im1[rp1 * cols + cp1];

            // Sobel X
            // -1 0 1
            // -2 0 2
            // -1 0 1
            int gx = -val_m1_m1 + val_m1_p1 - 2*val_0_m1 + 2*val_0_p1 - val_p1_m1 + val_p1_p1;
            
            // Sobel Y
            // -1 -2 -1
            //  0  0  0
            //  1  2  1
            int gy = -val_m1_m1 - 2*val_m1_0 - val_m1_p1 + val_p1_m1 + 2*val_p1_0 + val_p1_p1;

            // It = I2 - I1
            int gt = (int)local_im2[idx] - (int)local_im1[idx];

            buf_Ix[idx] = (grad_t)gx;
            buf_Iy[idx] = (grad_t)gy;
            buf_It[idx] = (grad_t)gt;

            // Initialize Flow to 0
            buf_u[idx] = 0;
            buf_v[idx] = 0;
        }
    }

    // 3. Iterative Update
    ITER_LOOP: for (int k = 0; k < N_ITER; k++) {
        UPDATE_LOOP: for (int i = 0; i < MAX_HEIGHT; i++) {
            for (int j = 0; j < MAX_WIDTH; j++) {
                #pragma HLS PIPELINE II=1
                if (i >= rows || j >= cols) continue;

                int idx = i * cols + j;

                // Neighbor indices
                int rm1 = (i > 0) ? i - 1 : 0;
                int rp1 = (i < rows - 1) ? i + 1 : rows - 1;
                int cm1 = (j > 0) ? j - 1 : 0;
                int cp1 = (j < cols - 1) ? j + 1 : cols - 1;

                // 4-neighbor average (Up, Down, Left, Right)
                flow_t u_up    = buf_u[rm1 * cols + j];
                flow_t u_down  = buf_u[rp1 * cols + j];
                flow_t u_left  = buf_u[i * cols + cm1];
                flow_t u_right = buf_u[i * cols + cp1];

                flow_t v_up    = buf_v[rm1 * cols + j];
                flow_t v_down  = buf_v[rp1 * cols + j];
                flow_t v_left  = buf_v[i * cols + cm1];
                flow_t v_right = buf_v[i * cols + cp1];

                // Average calculation
                calc_t u_avg_val = ((calc_t)u_up + (calc_t)u_down + (calc_t)u_left + (calc_t)u_right) * (calc_t)0.25;
                calc_t v_avg_val = ((calc_t)v_up + (calc_t)v_down + (calc_t)v_left + (calc_t)v_right) * (calc_t)0.25;

                // Load Gradients
                grad_t ix = buf_Ix[idx];
                grad_t iy = buf_Iy[idx];
                grad_t it = buf_It[idx];

                // Denominator: alpha^2 + Ix^2 + Iy^2
                den_t den = (den_t)(ALPHA_VAL * ALPHA_VAL) + (den_t)ix * (den_t)ix + (den_t)iy * (den_t)iy;
                if (den == 0) den = 1; // Avoid division by zero

                // Numerator: Ix*u_avg + Iy*v_avg + It
                calc_t num = (calc_t)ix * u_avg_val + (calc_t)iy * v_avg_val + (calc_t)it;

                // Division factor
                calc_t div_factor = num / (calc_t)den;

                // Update
                buf_u_next[idx] = (flow_t)(u_avg_val - (calc_t)ix * div_factor);
                buf_v_next[idx] = (flow_t)(v_avg_val - (calc_t)iy * div_factor);
            }
        }

        // Copy back
        COPY_LOOP: for (int i = 0; i < MAX_HEIGHT * MAX_WIDTH; i++) {
             #pragma HLS PIPELINE II=1
             if (i < rows * cols) {
                buf_u[i] = buf_u_next[i];
                buf_v[i] = buf_v_next[i];
             }
        }
    }

    // 4. Write Output
    // Convert Q8.8 fixed point to signed short raw representation
    WRITE_LOOP: for (int i = 0; i < MAX_HEIGHT * MAX_WIDTH; i++) {
        #pragma HLS PIPELINE II=1
        if (i < rows * cols) {
             // range(15, 0) extracts the raw bits of the fixed point number.
             // This corresponds to integer value = real value * 256.
             vx_img[i] = (short)buf_u[i].range(15, 0);
             vy_img[i] = (short)buf_v[i].range(15, 0);
        }
    }
}
