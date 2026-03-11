#include "hs_config_params.h"

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
            // Formula: (Right Column) - (Left Column)
            // Right: (i-1, j+1), (i, j+1), (i+1, j+1) -> m1_p1, 0_p1, p1_p1
            // Left:  (i-1, j-1), (i, j-1), (i+1, j-1) -> m1_m1, 0_m1, p1_m1
            Ix[i][j] = (val_m1_p1 + (val_0_p1 << 1) + val_p1_p1) - (val_m1_m1 + (val_0_m1 << 1) + val_p1_m1);

            // Compute Iy (Sobel Y)
            // -1 -2 -1
            //  0  0  0
            //  1  2  1
            // Formula: (Bottom Row) - (Top Row)
            // Bottom: (i+1, j-1), (i+1, j), (i+1, j+1) -> p1_m1, p1_0, p1_p1
            // Top:    (i-1, j-1), (i-1, j), (i-1, j+1) -> m1_m1, m1_0, m1_p1
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
            // Initialize flow
            u_curr[i][j] = 0;
            v_curr[i][j] = 0;
        }
    }

    // Compute Gradients
    compute_gradients(height, width);

    // Iteration loop
    iter_loop: for(int iter=0; iter<N_ITER; iter++) {
        #pragma HLS LOOP_TRIPCOUNT min=10 max=10
        
        update_loop_y: for(int i=0; i<height; i++) {
            #pragma HLS LOOP_TRIPCOUNT min=1 max=128
            update_loop_x: for(int j=0; j<width; j++) {
                #pragma HLS LOOP_TRIPCOUNT min=1 max=256
                #pragma HLS PIPELINE II=1

                // Compute local average (4-neighbor)
                // Use clamp or copy boundary values
                // Boundary pixels: average of available neighbors or copy center?
                // Standard HS: average of neighbors. At boundary, we can replicate border or assume 0.
                // Replicating border is safer for continuity.
                
                int i_m1 = (i > 0) ? i - 1 : 0;
                int i_p1 = (i < height - 1) ? i + 1 : height - 1;
                int j_m1 = (j > 0) ? j - 1 : 0;
                int j_p1 = (j < width - 1) ? j + 1 : width - 1;
                
                // Average u
                flow_t u_avg = (u_curr[i_m1][j] + u_curr[i_p1][j] + u_curr[i][j_m1] + u_curr[i][j_p1]) * (flow_t)0.25;
                
                // Average v
                flow_t v_avg = (v_curr[i_m1][j] + v_curr[i_p1][j] + v_curr[i][j_m1] + v_curr[i][j_p1]) * (flow_t)0.25;
                
                // Compute update
                // num = Ix * u_avg + Iy * v_avg + It
                // den = alpha^2 + Ix^2 + Iy^2
                
                accum_t ix_val = (accum_t)Ix[i][j];
                accum_t iy_val = (accum_t)Iy[i][j];
                accum_t it_val = (accum_t)It[i][j];
                
                accum_t num_term = ix_val * u_avg + iy_val * v_avg + it_val;
                accum_t den_term = (accum_t)ALPHA_SQ_VAL + ix_val * ix_val + iy_val * iy_val;
                
                // Division
                // We need a division here. HLS will synthesize a divider.
                // To avoid divide by zero, den_term is at least alpha^2 (400), so it's safe.
                accum_t update_factor = num_term / den_term;
                
                // Update u, v
                u_next[i][j] = u_avg - ix_val * update_factor;
                v_next[i][j] = v_avg - iy_val * update_factor;
            }
        }
        
        // Copy next to curr
        copy_loop_y: for(int i=0; i<height; i++) {
            #pragma HLS LOOP_TRIPCOUNT min=1 max=128
            copy_loop_x: for(int j=0; j<width; j++) {
                #pragma HLS LOOP_TRIPCOUNT min=1 max=256
                #pragma HLS PIPELINE II=1
                u_curr[i][j] = u_next[i][j];
                v_curr[i][j] = v_next[i][j];
            }
        }
    }

    // Write outputs
    write_loop: for(int i=0; i<height; i++) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=128
        for(int j=0; j<width; j++) {
            #pragma HLS LOOP_TRIPCOUNT min=1 max=256
            #pragma HLS PIPELINE II=1
            // Convert fixed point to signed short (Q8.8 -> 16-bit integer representation of fixed point)
            // The output is expecting "signed short int". 
            // Usually this means the raw bits of the fixed point, or the integer part?
            // "采用 signed short int 定点格式（需明确缩放因子）"
            // If I return the raw bits of Q8.8, the scaling factor is 256 (2^8).
            // ap_fixed stores data as an integer scaled by 2^F.
            // casting ap_fixed to short directly usually converts to integer (truncates).
            // To get the raw bits, we should use .range() or internal representation.
            // However, typical HLS flow: if interface is short, and we want to pass Q8.8,
            // we should multiply by 256 and cast to short, OR just interpret the bits.
            // Let's multiply by 256 (shift left 8) if we treat the output as "integer representation of the fixed value".
            // Wait, ap_fixed<16,8> *is* a 16-bit container.
            // If I assign `vx_img[k] = u_curr[i][j]`, it will perform integer conversion (loss of precision).
            // I need to preserve the fractional part.
            // So I should assign `vx_img[k] = u_curr[i][j].range(15, 0);`?
            // No, standard way to export fixed point to integer array is:
            // vx_img[k] = (short)(u_curr[i][j] * 256);
            // OR use `.to_short()` if it exists? No.
            // Safest way: `vx_img[k] = (short)((u_curr[i][j] * 256).to_int());`?
            // Actually, ap_fixed has a raw bits access.
            // But to be portable and clear:
            // "Output vx_img / vy_img as dense flow field... signed short int fixed point format (need to specify scaling factor)"
            // I will use scaling factor 256 (Q8.8).
            
            // Explicit scaling to preserve fractional bits in the output integer container
            // We want the bit pattern of Q8.8 in the short.
            // Since flow_t is Q8.8, its underlying integer IS the value * 256.
            // But direct cast `(short)flow_t` returns the INTEGER part.
            // We need `(short)(flow_t_val.range(15, 0))` or equivalent.
            // Since we can't use .range() on the type easily in C sim without correct headers sometimes...
            // Let's just multiply by 256.
            // But u_curr is ap_fixed.
            // ap_fixed<16,8> val;
            // val * 256 might overflow if we are not careful?
            // No, val is 16 bit. 8 int, 8 frac.
            // If we cast to ap_fixed<24,16> then multiply...
            // Easiest: use .range() if allowed. ap_fixed allows .range().
            // vx_img[i*width+j] = (signed short int) u_curr[i][j].range(15, 0);
            
            vx_img[i*width + j] = (signed short int) u_curr[i][j].range(15, 0);
            vy_img[i*width + j] = (signed short int) v_curr[i][j].range(15, 0);
        }
    }
}
