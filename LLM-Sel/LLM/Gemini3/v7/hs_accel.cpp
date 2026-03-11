#include "hs_config_params.h"

// Helper function to get pixel with replication boundary
static calc_t get_pixel(unsigned short *img, int y, int x, int height, int width) {
    #pragma HLS INLINE
    int r = y;
    int c = x;
    if (r < 0) r = 0;
    if (r >= height) r = height - 1;
    if (c < 0) c = 0;
    if (c >= width) c = width - 1;
    return (calc_t)img[r * MAX_WIDTH + c]; 
}

static calc_t get_val(flow_t buf[MAX_HEIGHT][MAX_WIDTH], int y, int x, int height, int width) {
    #pragma HLS INLINE
    int r = y;
    int c = x;
    if (r < 0) r = 0;
    if (r >= height) r = height - 1;
    if (c < 0) c = 0;
    if (c >= width) c = width - 1;
    return (calc_t)buf[r][c];
}

int hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
) {
    // Interface Directives
    // Standard Vitis HLS AXI Master + Slave Control interface
    #pragma HLS INTERFACE m_axi port=inp1_img offset=slave bundle=gmem0 depth=32768
    #pragma HLS INTERFACE m_axi port=inp2_img offset=slave bundle=gmem1 depth=32768
    #pragma HLS INTERFACE m_axi port=vx_img   offset=slave bundle=gmem2 depth=32768
    #pragma HLS INTERFACE m_axi port=vy_img   offset=slave bundle=gmem3 depth=32768
    #pragma HLS INTERFACE s_axilite port=height
    #pragma HLS INTERFACE s_axilite port=width
    #pragma HLS INTERFACE s_axilite port=return

    // Local buffers - All 16-bit to fit in Zynq-7020 BRAM (Total ~576KB)
    
    // Gradients (16-bit int)
    static grad_t Ix[MAX_HEIGHT][MAX_WIDTH];
    static grad_t Iy[MAX_HEIGHT][MAX_WIDTH];
    static grad_t It[MAX_HEIGHT][MAX_WIDTH];
    #pragma HLS BIND_STORAGE variable=Ix type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=Iy type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=It type=ram_2p impl=bram

    // Flow buffers (16-bit fixed point Q6.10)
    static flow_t u_curr[MAX_HEIGHT][MAX_WIDTH];
    static flow_t v_curr[MAX_HEIGHT][MAX_WIDTH];
    static flow_t u_next[MAX_HEIGHT][MAX_WIDTH];
    static flow_t v_next[MAX_HEIGHT][MAX_WIDTH];
    #pragma HLS BIND_STORAGE variable=u_curr type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=v_curr type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=u_next type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=v_next type=ram_2p impl=bram

    // Input Image Buffers (16-bit)
    static unsigned short buf_im1[MAX_HEIGHT][MAX_WIDTH];
    static unsigned short buf_im2[MAX_HEIGHT][MAX_WIDTH];
    #pragma HLS BIND_STORAGE variable=buf_im1 type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=buf_im2 type=ram_2p impl=bram

    // 1. Copy Inputs
    LOAD_LOOP: for(int i=0; i<height; i++) {
        for(int j=0; j<width; j++) {
            #pragma HLS PIPELINE II=1
            buf_im1[i][j] = inp1_img[i*MAX_WIDTH + j];
            buf_im2[i][j] = inp2_img[i*MAX_WIDTH + j];
            // Initialize flow
            u_curr[i][j] = 0;
            v_curr[i][j] = 0;
        }
    }

    // 2. Compute Gradients
    GRAD_LOOP: for(int i=0; i<height; i++) {
        for(int j=0; j<width; j++) {
            #pragma HLS PIPELINE II=1
            
            // Replicate boundary for Sobel
            calc_t p00 = get_pixel((unsigned short*)buf_im1, i-1, j-1, height, width);
            calc_t p01 = get_pixel((unsigned short*)buf_im1, i-1, j,   height, width);
            calc_t p02 = get_pixel((unsigned short*)buf_im1, i-1, j+1, height, width);
            
            calc_t p10 = get_pixel((unsigned short*)buf_im1, i,   j-1, height, width);
            // p11 center
            calc_t p12 = get_pixel((unsigned short*)buf_im1, i,   j+1, height, width);
            
            calc_t p20 = get_pixel((unsigned short*)buf_im1, i+1, j-1, height, width);
            calc_t p21 = get_pixel((unsigned short*)buf_im1, i+1, j,   height, width);
            calc_t p22 = get_pixel((unsigned short*)buf_im1, i+1, j+1, height, width);

            // Gx
            calc_t gx = (p02 + (calc_t)2*p12 + p22) - (p00 + (calc_t)2*p10 + p20);
            
            // Gy
            calc_t gy = (p20 + (calc_t)2*p21 + p22) - (p00 + (calc_t)2*p01 + p02);

            // Gt = I2 - I1
            calc_t t1 = (calc_t)buf_im1[i][j];
            calc_t t2 = (calc_t)buf_im2[i][j];
            calc_t gt = t2 - t1;

            Ix[i][j] = (grad_t)gx;
            Iy[i][j] = (grad_t)gy;
            It[i][j] = (grad_t)gt;
        }
    }

    // 3. Iteration
    ITER_LOOP: for(int iter=0; iter<N_ITER; iter++) {
        // Pixel update loop
        PIXEL_LOOP: for(int i=0; i<height; i++) {
            for(int j=0; j<width; j++) {
                #pragma HLS PIPELINE II=1
                
                // 4-neighbor average
                calc_t u_up = get_val(u_curr, i-1, j, height, width);
                calc_t u_dn = get_val(u_curr, i+1, j, height, width);
                calc_t u_lf = get_val(u_curr, i, j-1, height, width);
                calc_t u_rt = get_val(u_curr, i, j+1, height, width);
                
                calc_t v_up = get_val(v_curr, i-1, j, height, width);
                calc_t v_dn = get_val(v_curr, i+1, j, height, width);
                calc_t v_lf = get_val(v_curr, i, j-1, height, width);
                calc_t v_rt = get_val(v_curr, i, j+1, height, width);

                calc_t u_avg = (u_up + u_dn + u_lf + u_rt) * (calc_t)0.25;
                calc_t v_avg = (v_up + v_dn + v_lf + v_rt) * (calc_t)0.25;

                // Update
                calc_t ix_val = (calc_t)Ix[i][j];
                calc_t iy_val = (calc_t)Iy[i][j];
                calc_t it_val = (calc_t)It[i][j];

                calc_t num = ix_val * u_avg + iy_val * v_avg + it_val;
                calc_t den = (calc_t)ALPHA_SQ + ix_val * ix_val + iy_val * iy_val;
                
                if(den != 0) {
                     calc_t ratio = num / den;
                     u_next[i][j] = (flow_t)(u_avg - ix_val * ratio);
                     v_next[i][j] = (flow_t)(v_avg - iy_val * ratio);
                } else {
                     u_next[i][j] = (flow_t)u_avg;
                     v_next[i][j] = (flow_t)v_avg;
                }
            }
        }

        // Copy next to curr
        COPY_LOOP: for(int i=0; i<height; i++) {
            for(int j=0; j<width; j++) {
                #pragma HLS PIPELINE II=1
                u_curr[i][j] = u_next[i][j];
                v_curr[i][j] = v_next[i][j];
            }
        }
    }

    // 4. Output
    STORE_LOOP: for(int i=0; i<height; i++) {
        for(int j=0; j<width; j++) {
            #pragma HLS PIPELINE II=1
            calc_t u_final = (calc_t)u_curr[i][j];
            calc_t v_final = (calc_t)v_curr[i][j];
            
            // Scale and Store
            // Q6.10 -> 16-bit integer (scaled by 1024)
            // Just reinterpret bits? No, value changes. 
            // 1.0 (float) -> 1024 (int)
            // flow_t 1.0 is represented as 000001 0000000000 (binary) = 1024 raw.
            // So we can just use .range() or cast?
            // If we multiply by 1024, we might overflow if not careful, but calc_t is 32-bit.
            calc_t u_scaled = u_final * OUT_SCALE;
            calc_t v_scaled = v_final * OUT_SCALE;

            vx_img[i*MAX_WIDTH + j] = (signed short int)u_scaled;
            vy_img[i*MAX_WIDTH + j] = (signed short int)v_scaled;
        }
    }

    return 0;
}
