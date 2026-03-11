#include "hs_config_params.h"

// Gradient Kernels are implicit in the code
// Gx: [-1 0 1], [-2 0 2], [-1 0 1]
// Gy: [-1 -2 -1], [0 0 0], [1 2 1]

// Internal memory for input images, gradients and flow
// Using static to map to BRAM/URAM and avoid stack usage
static unsigned short img1_buf[MAX_HEIGHT][MAX_WIDTH];
static unsigned short img2_buf[MAX_HEIGHT][MAX_WIDTH];
static grad_t Ix[MAX_HEIGHT][MAX_WIDTH];
static grad_t Iy[MAX_HEIGHT][MAX_WIDTH];
static grad_t It[MAX_HEIGHT][MAX_WIDTH];
static flow_t u_curr[MAX_HEIGHT][MAX_WIDTH];
static flow_t v_curr[MAX_HEIGHT][MAX_WIDTH];
static flow_t u_next[MAX_HEIGHT][MAX_WIDTH];
static flow_t v_next[MAX_HEIGHT][MAX_WIDTH];

void compute_gradients(int height, int width) {
    #pragma HLS INLINE off
    
    // Compute gradients
    // Loop bounds are variable but bounded by MAX_HEIGHT/MAX_WIDTH
    grad_loop_y: for(int i=0; i<height; i++) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=128
        grad_loop_x: for(int j=0; j<width; j++) {
            #pragma HLS LOOP_TRIPCOUNT min=1 max=256
            #pragma HLS PIPELINE II=1
            
            // Boundary clamping for 3x3 window
            int i_m1 = (i > 0) ? i - 1 : 0;
            int i_p1 = (i < height - 1) ? i + 1 : height - 1;
            int j_m1 = (j > 0) ? j - 1 : 0;
            int j_p1 = (j < width - 1) ? j + 1 : width - 1;

            // Read samples for Sobel
            // Row i-1
            grad_t val_m1_m1 = (grad_t)img1_buf[i_m1][j_m1];
            grad_t val_m1_0  = (grad_t)img1_buf[i_m1][j];
            grad_t val_m1_p1 = (grad_t)img1_buf[i_m1][j_p1];
            
            // Row i
            grad_t val_0_m1  = (grad_t)img1_buf[i][j_m1];
            grad_t val_0_p1  = (grad_t)img1_buf[i][j_p1];
            
            // Row i+1
            grad_t val_p1_m1 = (grad_t)img1_buf[i_p1][j_m1];
            grad_t val_p1_0  = (grad_t)img1_buf[i_p1][j];
            grad_t val_p1_p1 = (grad_t)img1_buf[i_p1][j_p1];

            // Compute Ix (Sobel X)
            // -1 0 1
            // -2 0 2
            // -1 0 1
            Ix[i][j] = (val_m1_p1 + (val_0_p1 << 1) + val_p1_p1) - (val_m1_m1 + (val_0_m1 << 1) + val_m1_p1);

            // Compute Iy (Sobel Y)
            // -1 -2 -1
            //  0  0  0
            //  1  2  1
            Iy[i][j] = (val_p1_m1 + (val_p1_0 << 1) + val_p1_p1) - (val_m1_m1 + (val_m1_0 << 1) + val_m1_p1);
            
            // Compute It (Temporal)
            // It = I2 - I1
            It[i][j] = (grad_t)img2_buf[i][j] - (grad_t)img1_buf[i][j];
        }
    }
}

// Top level function
void hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
) {
    #pragma HLS INTERFACE m_axi port=inp1_img offset=slave bundle=gmem0 depth=32768
    #pragma HLS INTERFACE m_axi port=inp2_img offset=slave bundle=gmem1 depth=32768
    #pragma HLS INTERFACE m_axi port=vx_img   offset=slave bundle=gmem2 depth=32768
    #pragma HLS INTERFACE m_axi port=vy_img   offset=slave bundle=gmem3 depth=32768
    #pragma HLS INTERFACE s_axilite port=height
    #pragma HLS INTERFACE s_axilite port=width
    #pragma HLS INTERFACE s_axilite port=return

    // Read inputs to local buffers
    read_loop: for(int i=0; i<height; i++) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=128
        for(int j=0; j<width; j++) {
            #pragma HLS LOOP_TRIPCOUNT min=1 max=256
            #pragma HLS PIPELINE II=1
            img1_buf[i][j] = inp1_img[i*width + j];
            img2_buf[i][j] = inp2_img[i*width + j];
        }
    }

    // Compute Gradients
    compute_gradients(height, width);

    // Initialize Flow to 0
    init_loop: for(int i=0; i<height; i++) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=128
        for(int j=0; j<width; j++) {
            #pragma HLS LOOP_TRIPCOUNT min=1 max=256
            #pragma HLS PIPELINE II=1
            u_curr[i][j] = 0;
            v_curr[i][j] = 0;
        }
    }

    // Iterations
    iter_loop: for(int iter=0; iter<N_ITER; iter++) {
        #pragma HLS LOOP_TRIPCOUNT min=20 max=20
        
        update_loop: for(int i=0; i<height; i++) {
            #pragma HLS LOOP_TRIPCOUNT min=1 max=128
            for(int j=0; j<width; j++) {
                #pragma HLS LOOP_TRIPCOUNT min=1 max=256
                #pragma HLS PIPELINE II=1
                
                // Boundary clamping for average
                int i_m1 = (i > 0) ? i - 1 : 0;
                int i_p1 = (i < height - 1) ? i + 1 : height - 1;
                int j_m1 = (j > 0) ? j - 1 : 0;
                int j_p1 = (j < width - 1) ? j + 1 : width - 1;
                
                flow_t u_up    = u_curr[i_m1][j];
                flow_t u_down  = u_curr[i_p1][j];
                flow_t u_left  = u_curr[i][j_m1];
                flow_t u_right = u_curr[i][j_p1];
                
                flow_t v_up    = v_curr[i_m1][j];
                flow_t v_down  = v_curr[i_p1][j];
                flow_t v_left  = v_curr[i][j_m1];
                flow_t v_right = v_curr[i][j_p1];

                // Average
                flow_t u_avg = (u_up + u_down + u_left + u_right) * (flow_t)0.25;
                flow_t v_avg = (v_up + v_down + v_left + v_right) * (flow_t)0.25;

                // Update
                accum_t ix_val = Ix[i][j];
                accum_t iy_val = Iy[i][j];
                accum_t it_val = It[i][j];
                
                accum_t denom = (accum_t)ALPHA_SQ_VAL + ix_val*ix_val + iy_val*iy_val;
                
                // Avoid division by zero (though denom >= ALPHA_SQ_VAL > 0)
                // Using ap_fixed division
                accum_t num_u = ix_val * u_avg + iy_val * v_avg + it_val;
                accum_t update_term = num_u / denom;
                
                u_next[i][j] = u_avg - ix_val * update_term;
                v_next[i][j] = v_avg - iy_val * update_term;
            }
        }
        
        // Copy back
        copy_loop: for(int i=0; i<height; i++) {
            #pragma HLS LOOP_TRIPCOUNT min=1 max=128
            for(int j=0; j<width; j++) {
                #pragma HLS LOOP_TRIPCOUNT min=1 max=256
                #pragma HLS PIPELINE II=1
                u_curr[i][j] = u_next[i][j];
                v_curr[i][j] = v_next[i][j];
            }
        }
    }

    // Output Flow
    write_loop: for(int i=0; i<height; i++) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=128
        for(int j=0; j<width; j++) {
            #pragma HLS LOOP_TRIPCOUNT min=1 max=256
            #pragma HLS PIPELINE II=1
            // Return raw bits of Q8.8 format
            // User should interpret output as Q8.8 fixed point
            // Scaling factor = 256
            vx_img[i*width + j] = (signed short int)u_curr[i][j].range(15, 0);
            vy_img[i*width + j] = (signed short int)v_curr[i][j].range(15, 0);
        }
    }
}
