#include "hs_config_params.h"

int hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
) {
    #pragma HLS INTERFACE m_axi port=inp1_img offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=inp2_img offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=vx_img offset=slave bundle=gmem2
    #pragma HLS INTERFACE m_axi port=vy_img offset=slave bundle=gmem3
    #pragma HLS INTERFACE s_axilite port=height
    #pragma HLS INTERFACE s_axilite port=width
    #pragma HLS INTERFACE s_axilite port=return

    // Local Buffers
    // Gradients
    static grad_t Ix[MAX_HEIGHT][MAX_WIDTH];
    static grad_t Iy[MAX_HEIGHT][MAX_WIDTH];
    static grad_t It[MAX_HEIGHT][MAX_WIDTH];
    #pragma HLS BIND_STORAGE variable=Ix type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=Iy type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=It type=ram_2p impl=bram

    // Velocities (Double Buffering for Jacobi)
    static vel_t u[MAX_HEIGHT][MAX_WIDTH];
    static vel_t v[MAX_HEIGHT][MAX_WIDTH];
    static vel_t u_new[MAX_HEIGHT][MAX_WIDTH];
    static vel_t v_new[MAX_HEIGHT][MAX_WIDTH];
    #pragma HLS BIND_STORAGE variable=u type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=v type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=u_new type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=v_new type=ram_2p impl=bram
    
    // Input Image Buffers (Reuse u_new/v_new space temporarily? No, unsafe)
    // We need temporary storage for images to compute gradients.
    // 256*128*16bit * 2 = 128KB.
    // Total memory is tight.
    // We can compute gradients row-by-row with line buffers, but It requires both full images.
    // Or we can read full images, compute gradients, then discard images.
    // So we can reuse the memory of `u` and `v` for `img1` and `img2` initially?
    // `u` and `v` are initialized to 0 after gradient computation.
    // `img1` and `img2` are needed only for gradient computation.
    // So we can alias buffers?
    // HLS doesn't support pointer casting for BRAM aliasing easily.
    // But we can use a union or just reuse the array if types match?
    // `vel_t` is 16-bit. `unsigned short` is 16-bit.
    // So we can use `u` storage for `img1` and `v` storage for `img2`.
    // Then after computing `Ix, Iy, It`, we zero out `u` and `v`.
    
    // Step 1: Read Inputs into u/v buffers (casted)
    // We'll treat u and v as raw 16-bit containers for this phase.
    
    READ_LOOP: for(int i=0; i<height; i++) {
        #pragma HLS LOOP_TRIPCOUNT max=128
        for(int j=0; j<width; j++) {
            #pragma HLS LOOP_TRIPCOUNT max=256
            // Store raw bits. 
            // ap_fixed has constructors.
            unsigned short p1 = inp1_img[i*width + j];
            unsigned short p2 = inp2_img[i*width + j];
            
            // We store these in u and v.
            // We need to be careful about bit representation.
            // Direct assignment `u[i][j] = p1` converts integer p1 to fixed point u.
            // e.g. p1=255 -> u=255.0. 
            // If vel_t is Q6.10, max value is 31.99. 255 overflows!
            // So we CANNOT use `vel_t` to store pixel values (0-255).
            // We need separate buffers for images.
            // BUT we are out of memory?
            // 7 arrays * 64KB = 448KB.
            // If we add 2 image arrays: +128KB = 576KB.
            // Total 576KB. 
            // XC7Z020 BRAM is ~630KB.
            // It fits! 576KB < 630KB.
            // So we can define `img1`, `img2` explicitly.
        }
    }
    
    static unsigned short img1[MAX_HEIGHT][MAX_WIDTH];
    static unsigned short img2[MAX_HEIGHT][MAX_WIDTH];
    #pragma HLS BIND_STORAGE variable=img1 type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=img2 type=ram_2p impl=bram

    LOAD_IMG: for(int i=0; i<height; i++) {
        #pragma HLS LOOP_TRIPCOUNT max=128
        for(int j=0; j<width; j++) {
            #pragma HLS LOOP_TRIPCOUNT max=256
            #pragma HLS PIPELINE II=1
            img1[i][j] = inp1_img[i*width + j];
            img2[i][j] = inp2_img[i*width + j];
        }
    }

    // Step 2: Compute Gradients
    GRAD_LOOP: for(int i=0; i<height; i++) {
        #pragma HLS LOOP_TRIPCOUNT max=128
        for(int j=0; j<width; j++) {
            #pragma HLS LOOP_TRIPCOUNT max=256
            #pragma HLS PIPELINE II=1
            
            if(i==0 || i>=height-1 || j==0 || j>=width-1) {
                Ix[i][j] = 0;
                Iy[i][j] = 0;
                It[i][j] = 0;
            } else {
                // Sobel X
                // -1 0 1
                // -2 0 2
                // -1 0 1
                int p00 = img1[i-1][j-1]; int p01 = img1[i-1][j]; int p02 = img1[i-1][j+1];
                int p10 = img1[i][j-1];   /*p11*/                 int p12 = img1[i][j+1];
                int p20 = img1[i+1][j-1]; int p21 = img1[i+1][j]; int p22 = img1[i+1][j+1];

                int gx = (p02 - p00) + 2*(p12 - p10) + (p22 - p20);
                
                // Sobel Y
                // -1 -2 -1
                //  0  0  0
                //  1  2  1
                int gy = (p20 - p00) + 2*(p21 - p01) + (p22 - p02);
                
                Ix[i][j] = gx;
                Iy[i][j] = gy;
                
                // It = I2 - I1
                // Note: Simple difference
                It[i][j] = (int)img2[i][j] - (int)img1[i][j];
            }
        }
    }

    // Step 3: Initialize u, v
    INIT_LOOP: for(int i=0; i<height; i++) {
        #pragma HLS LOOP_TRIPCOUNT max=128
        for(int j=0; j<width; j++) {
            #pragma HLS LOOP_TRIPCOUNT max=256
            #pragma HLS PIPELINE II=1
            u[i][j] = 0;
            v[i][j] = 0;
        }
    }

    // Step 4: Iterations
    ITER_LOOP: for(int iter=0; iter<N_ITER; iter++) {
        #pragma HLS LOOP_TRIPCOUNT max=20
        
        // Update Loop
        UPDATE_LOOP: for(int i=0; i<height; i++) {
            #pragma HLS LOOP_TRIPCOUNT max=128
            for(int j=0; j<width; j++) {
                #pragma HLS LOOP_TRIPCOUNT max=256
                #pragma HLS PIPELINE II=1
                
                if(i==0 || i>=height-1 || j==0 || j>=width-1) {
                    u_new[i][j] = 0;
                    v_new[i][j] = 0;
                } else {
                    // 4-neighbor average
                    // We need to use explicit casts to ensure precision during sum
                    vel_t sum_u = u[i-1][j] + u[i+1][j] + u[i][j-1] + u[i][j+1];
                    vel_t sum_v = v[i-1][j] + v[i+1][j] + v[i][j-1] + v[i][j+1];
                    
                    // Multiply by 0.25 (shift right 2)
                    vel_t u_avg = sum_u >> 2; 
                    vel_t v_avg = sum_v >> 2;
                    // Or u_avg = sum_u * 0.25;
                    
                    grad_t ix = Ix[i][j];
                    grad_t iy = Iy[i][j];
                    grad_t it = It[i][j];
                    
                    // Denom
                    // alpha^2 + ix^2 + iy^2
                    // We use 32-bit int for intermediate calculation of denom to avoid overflow?
                    // ix is 16-bit. ix*ix is 32-bit (unsigned) or signed?
                    // grad_t is signed.
                    // We need a larger type for calculation.
                    ap_int<32> ix2 = ix * ix;
                    ap_int<32> iy2 = iy * iy;
                    ap_int<32> denom_int = ALPHA_SQUARED + ix2 + iy2;
                    
                    // Num
                    // ix * (ix*u_avg + iy*v_avg + it)
                    // ix*u_avg -> fixed point result.
                    // We should use ap_fixed with more integer bits for the numerator term?
                    // u_avg (Q6.10) * ix (Q16.0) -> Q22.10.
                    // Range of u_avg is small (+/- 32). ix is +/- 1000. Product +/- 32000. Fits in Q22.10 (16 int bits).
                    
                    ap_fixed<32, 16> term1 = ix * u_avg;
                    ap_fixed<32, 16> term2 = iy * v_avg;
                    ap_fixed<32, 16> term_sum = term1 + term2 + it;
                    
                    ap_fixed<32, 16> num_u = ix * term_sum;
                    ap_fixed<32, 16> num_v = iy * term_sum;
                    
                    // Division
                    // num / denom
                    // denom is integer.
                    // ap_fixed / ap_int -> ap_fixed.
                    ap_fixed<32, 16> update_u = num_u / denom_int;
                    ap_fixed<32, 16> update_v = num_v / denom_int;
                    
                    u_new[i][j] = u_avg - update_u;
                    v_new[i][j] = v_avg - update_v;
                }
            }
        }

        // Copy Back (or pointer swap if not unrolled, but here we just copy)
        // Since N_ITER is small, copying is negligible compared to calculation.
        COPY_LOOP: for(int i=0; i<height; i++) {
            #pragma HLS LOOP_TRIPCOUNT max=128
            for(int j=0; j<width; j++) {
                #pragma HLS LOOP_TRIPCOUNT max=256
                #pragma HLS PIPELINE II=1
                u[i][j] = u_new[i][j];
                v[i][j] = v_new[i][j];
            }
        }
    }

    // Step 5: Write Output
    WRITE_LOOP: for(int i=0; i<height; i++) {
        #pragma HLS LOOP_TRIPCOUNT max=128
        for(int j=0; j<width; j++) {
            #pragma HLS LOOP_TRIPCOUNT max=256
            #pragma HLS PIPELINE II=1
            
            // Convert vel_t (Q6.10) to Output format (Q8.8 stored in short)
            // Multiply by OUT_SCALE (256) -> shift left 8.
            // But vel_t has 10 fractional bits.
            // If we want Q8.8 (8 frac bits), we just shift right by 2?
            // No.
            // Value 1.0 in vel_t is represented as 1024 (1<<10).
            // Value 1.0 in output (Q8.8) should be 256 (1<<8).
            // So we need to divide by 4 (shift right 2).
            
            // Let's rely on ap_fixed casting or float conversion for clarity, then cast to short.
            // But "avoid float" in prompt.
            // ap_fixed<16,8> out_val = u[i][j];
            // short out_short = out_val.to_short(); ? No, that gives integer part.
            // We want the bits.
            // bit representation of Q8.8 is value * 256.
            // value = u[i][j].
            // result = u[i][j] * 256.
            
            ap_fixed<32, 16> val_u = u[i][j];
            ap_fixed<32, 16> val_v = v[i][j];
            
            // Multiply by 256
            val_u = val_u * OUT_SCALE;
            val_v = val_v * OUT_SCALE;
            
            vx_img[i*width + j] = (signed short int)val_u; // integer part of the scaled value
            vy_img[i*width + j] = (signed short int)val_v;
        }
    }
    return 0;
}
