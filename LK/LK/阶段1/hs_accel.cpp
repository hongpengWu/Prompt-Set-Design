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
    // Local Buffers
    // Using static to ensure they are allocated in BRAM (and avoid stack issues in C simulation)
    static pixel_t buff_im1[MAX_HEIGHT][MAX_WIDTH];
    static pixel_t buff_im2[MAX_HEIGHT][MAX_WIDTH];
    
    static grad_t buff_Ix[MAX_HEIGHT][MAX_WIDTH];
    static grad_t buff_Iy[MAX_HEIGHT][MAX_WIDTH];
    static grad_t buff_It[MAX_HEIGHT][MAX_WIDTH];
    
    static flow_t buff_u[MAX_HEIGHT][MAX_WIDTH];
    static flow_t buff_v[MAX_HEIGHT][MAX_WIDTH];
    static flow_t buff_u_next[MAX_HEIGHT][MAX_WIDTH];
    static flow_t buff_v_next[MAX_HEIGHT][MAX_WIDTH];

    // Partitioning for parallel access if needed.
    // Default BRAM is dual port.
    
    // 1. Read Inputs
    read_loop_y: for (int i = 0; i < MAX_HEIGHT; i++) {
        if (i < height) {
            read_loop_x: for (int j = 0; j < MAX_WIDTH; j++) {
                if (j < width) {
                    buff_im1[i][j] = inp1_img[i*width + j];
                    buff_im2[i][j] = inp2_img[i*width + j];
                    // Initialize flow to 0
                    buff_u[i][j] = 0;
                    buff_v[i][j] = 0;
                }
            }
        }
    }

    // 2. Compute Gradients
    // It = I2 - I1
    // Ix, Iy = Sobel(I1)
    grad_loop_y: for (int i = 0; i < MAX_HEIGHT; i++) {
        if (i < height) {
            grad_loop_x: for (int j = 0; j < MAX_WIDTH; j++) {
                if (j < width) {
                    
                    // Temporal Gradient
                    int px1 = buff_im1[i][j];
                    int px2 = buff_im2[i][j];
                    buff_It[i][j] = px2 - px1;
                    
                    // Spatial Gradients (Sobel)
                    // Boundary handling: Replicate border pixels
                    int im1 = (i > 0) ? i - 1 : 0;
                    int ip1 = (i < height - 1) ? i + 1 : height - 1;
                    int jm1 = (j > 0) ? j - 1 : 0;
                    int jp1 = (j < width - 1) ? j + 1 : width - 1;

                    // Read 3x3 neighborhood from im1
                    int val_m1_m1 = buff_im1[im1][jm1];
                    int val_m1_0  = buff_im1[im1][j];
                    int val_m1_p1 = buff_im1[im1][jp1];
                    
                    int val_0_m1  = buff_im1[i][jm1];
                    // int val_0_0   = buff_im1[i][j]; // Center not used in Sobel
                    int val_0_p1  = buff_im1[i][jp1];
                    
                    int val_p1_m1 = buff_im1[ip1][jm1];
                    int val_p1_0  = buff_im1[ip1][j];
                    int val_p1_p1 = buff_im1[ip1][jp1];

                    // Ix Kernel
                    // -1 0 1
                    // -2 0 2
                    // -1 0 1
                    int gx = -val_m1_m1 + val_m1_p1 - 2*val_0_m1 + 2*val_0_p1 - val_p1_m1 + val_p1_p1;
                    
                    // Iy Kernel
                    // -1 -2 -1
                    //  0  0  0
                    //  1  2  1
                    int gy = -val_m1_m1 - 2*val_m1_0 - val_m1_p1 + val_p1_m1 + 2*val_p1_0 + val_p1_p1;

                    buff_Ix[i][j] = gx;
                    buff_Iy[i][j] = gy;
                }
            }
        }
    }

    // 3. Iteration
    iter_loop: for (int k = 0; k < N_ITER; k++) {
        
        calc_loop_y: for (int i = 0; i < MAX_HEIGHT; i++) {
            if (i < height) {
                calc_loop_x: for (int j = 0; j < MAX_WIDTH; j++) {

                    if (j < width) {
                        
                        // Neighbors for Average
                        int im1 = (i > 0) ? i - 1 : 0;
                        int ip1 = (i < height - 1) ? i + 1 : height - 1;
                        int jm1 = (j > 0) ? j - 1 : 0;
                        int jp1 = (j < width - 1) ? j + 1 : width - 1;
                        
                        // 4-neighbor average
                        calc_t sum_u = (calc_t)buff_u[im1][j] + (calc_t)buff_u[ip1][j] + (calc_t)buff_u[i][jm1] + (calc_t)buff_u[i][jp1];
                        calc_t sum_v = (calc_t)buff_v[im1][j] + (calc_t)buff_v[ip1][j] + (calc_t)buff_v[i][jm1] + (calc_t)buff_v[i][jp1];
                        
                        flow_t u_avg = (flow_t)(sum_u * (calc_t)0.25);
                        flow_t v_avg = (flow_t)(sum_v * (calc_t)0.25);
                        
                        grad_t ix = buff_Ix[i][j];
                        grad_t iy = buff_Iy[i][j];
                        grad_t it = buff_It[i][j];
                        
                        // Denominator: alpha^2 + Ix^2 + Iy^2
                        den_t den = ALPHA_VAL*ALPHA_VAL + ix*ix + iy*iy;
                        
                        // Numerator term: Ix*u_avg + Iy*v_avg + It
                        calc_t num_term = ix*u_avg + iy*v_avg + it;
                        
                        // Update
                        // Use explicit cast for division to ensure correct fixed point behavior if needed
                        calc_t update_val = num_term / den;
                        
                        buff_u_next[i][j] = u_avg - ix * update_val;
                        buff_v_next[i][j] = v_avg - iy * update_val;
                    }
                }
            }
        }
        
        // Update buffers
        update_buff_loop_y: for(int i=0; i<MAX_HEIGHT; i++) {
             if (i < height) {
                 update_buff_loop_x: for(int j=0; j<MAX_WIDTH; j++) {
                     if (j < width) {
                         buff_u[i][j] = buff_u_next[i][j];
                         buff_v[i][j] = buff_v_next[i][j];
                     }
                 }
             }
        }
    }

    // 4. Output
    write_loop_y: for (int i = 0; i < MAX_HEIGHT; i++) {
        if (i < height) {
            write_loop_x: for (int j = 0; j < MAX_WIDTH; j++) {
                if (j < width) {
                    flow_t u_val = buff_u[i][j];
                    flow_t v_val = buff_v[i][j];
                    
                    // Return raw bits of the fixed point number
                    vx_img[i*width + j] = (short)u_val.range(15, 0);
                    vy_img[i*width + j] = (short)v_val.range(15, 0);
                }
            }
        }
    }
    return 0;
}
