#include "hs_config_params.h"

// Helper to compute gradients with line buffering (to save BRAM)
static void compute_gradients(
    unsigned short int* inp1,
    unsigned short int* inp2,
    grad_t Ix[MAX_HEIGHT][MAX_WIDTH],
    grad_t Iy[MAX_HEIGHT][MAX_WIDTH],
    grad_t It[MAX_HEIGHT][MAX_WIDTH],
    int height,
    int width
) {
    #pragma HLS INLINE off
    
    // Line buffers for im1: 3 rows
    unsigned short line_buff[3][MAX_WIDTH];
    #pragma HLS ARRAY_PARTITION variable=line_buff complete dim=1
    
    // Line buffer for im2: 1 row
    unsigned short line_im2[MAX_WIDTH];

    // Prefill for first row (i=0)
    // We need row -1 (replicate 0), row 0, row 1.
    
    // Read row 0
    for(int c=0; c<width; c++) {
        #pragma HLS PIPELINE II=1
        unsigned short val = inp1[0 * width + c];
        line_buff[0][c] = val; // row -1 -> 0
        line_buff[1][c] = val; // row 0
    }
    // Read row 1 (if exists)
    for(int c=0; c<width; c++) {
        #pragma HLS PIPELINE II=1
        if (height > 1)
            line_buff[2][c] = inp1[1 * width + c];
        else
            line_buff[2][c] = line_buff[1][c];
    }

    // Main Loop
    for (int i = 0; i < MAX_HEIGHT; i++) {
        
        if (i >= height) break;

        // Indices
        int b_prev = (i) % 3;      // row i-1
        int b_curr = (i + 1) % 3;  // row i
        int b_next = (i + 2) % 3;  // row i+1
        
        // Read im2 row i
        for(int c=0; c<width; c++) {
             #pragma HLS PIPELINE II=1
             line_im2[c] = inp2[i * width + c];
        }

        // Process row i
        for (int j = 0; j < MAX_WIDTH; j++) {
            #pragma HLS PIPELINE II=1
            if (j >= width) {
                Ix[i][j] = 0; Iy[i][j] = 0; It[i][j] = 0;
                continue;
            }

            int jm1 = (j > 0) ? j - 1 : 0;
            int jp1 = (j < width - 1) ? j + 1 : width - 1;

            // Access via line_buff
            int val_m1_m1 = line_buff[b_prev][jm1];
            int val_m1_0  = line_buff[b_prev][j];
            int val_m1_p1 = line_buff[b_prev][jp1];
            
            int val_0_m1  = line_buff[b_curr][jm1];
            // int val_0_0   = line_buff[b_curr][j];
            int val_0_p1  = line_buff[b_curr][jp1];
            
            int val_p1_m1 = line_buff[b_next][jm1];
            int val_p1_0  = line_buff[b_next][j];
            int val_p1_p1 = line_buff[b_next][jp1];

            int gx = -val_m1_m1 + val_m1_p1 
                     -2 * val_0_m1 + 2 * val_0_p1 
                     -val_p1_m1 + val_p1_p1;
            
            int gy = -val_m1_m1 - 2 * val_m1_0 - val_m1_p1 
                     + val_p1_m1 + 2 * val_p1_0 + val_p1_p1;
            
            int gt = (int)line_im2[j] - (int)line_buff[b_curr][j];

            Ix[i][j] = (grad_t)gx;
            Iy[i][j] = (grad_t)gy;
            It[i][j] = (grad_t)gt;
        }

        // Prepare for next iteration (i+1)
        // We need to read row (i+1)+1 = i+2.
        // Into buffer that was prev (b_prev).
        int load_idx = i + 2;
        int buf_idx_to_update = b_prev; 
        
        if (i < height - 1) {
            int read_row = (load_idx < height) ? load_idx : (height - 1);
            for(int c=0; c<width; c++) {
                #pragma HLS PIPELINE II=1
                line_buff[buf_idx_to_update][c] = inp1[read_row * width + c];
            }
        }
    }
}

// Helper for update step
static void update_flow(
    flow_t u_curr[MAX_HEIGHT][MAX_WIDTH],
    flow_t v_curr[MAX_HEIGHT][MAX_WIDTH],
    flow_t u_next[MAX_HEIGHT][MAX_WIDTH],
    flow_t v_next[MAX_HEIGHT][MAX_WIDTH],
    grad_t Ix[MAX_HEIGHT][MAX_WIDTH],
    grad_t Iy[MAX_HEIGHT][MAX_WIDTH],
    grad_t It[MAX_HEIGHT][MAX_WIDTH],
    int height,
    int width
) {
    #pragma HLS INLINE off
    
    // Constant Alpha squared
    const calc_t alpha2 = (calc_t)(ALPHA_VAL * ALPHA_VAL);

    for (int i = 0; i < MAX_HEIGHT; i++) {
        for (int j = 0; j < MAX_WIDTH; j++) {
            #pragma HLS PIPELINE II=1

            if (i >= height || j >= width) {
                u_next[i][j] = 0;
                v_next[i][j] = 0;
                continue;
            }

            // Neighbor Average (4-neighbor)
            int im1_idx = (i > 0) ? i - 1 : 0;
            int ip1_idx = (i < height - 1) ? i + 1 : height - 1;
            int jm1_idx = (j > 0) ? j - 1 : 0;
            int jp1_idx = (j < width - 1) ? j + 1 : width - 1;

            flow_t u_up = u_curr[im1_idx][j];
            flow_t u_dn = u_curr[ip1_idx][j];
            flow_t u_lf = u_curr[i][jm1_idx];
            flow_t u_rt = u_curr[i][jp1_idx];

            flow_t v_up = v_curr[im1_idx][j];
            flow_t v_dn = v_curr[ip1_idx][j];
            flow_t v_lf = v_curr[i][jm1_idx];
            flow_t v_rt = v_curr[i][jp1_idx];

            // Average
            calc_t u_avg = ((calc_t)u_up + (calc_t)u_dn + (calc_t)u_lf + (calc_t)u_rt) * (calc_t)0.25;
            calc_t v_avg = ((calc_t)v_up + (calc_t)v_dn + (calc_t)v_lf + (calc_t)v_rt) * (calc_t)0.25;

            // Gradients
            calc_t ix = (calc_t)Ix[i][j];
            calc_t iy = (calc_t)Iy[i][j];
            calc_t it = (calc_t)It[i][j];

            // Denominator = alpha^2 + Ix^2 + Iy^2
            calc_t den = alpha2 + ix*ix + iy*iy;
            
            // Numerator = Ix*u_avg + Iy*v_avg + It
            calc_t num = ix*u_avg + iy*v_avg + it;

            // Update term = num / den
            calc_t term = num / den;

            u_next[i][j] = (flow_t)(u_avg - ix * term);
            v_next[i][j] = (flow_t)(v_avg - iy * term);
        }
    }
}

static void write_output(
    signed short int* out_ptr,
    flow_t buff[MAX_HEIGHT][MAX_WIDTH],
    int height,
    int width
) {
    #pragma HLS INLINE off
    for (int i = 0; i < MAX_HEIGHT; i++) {
        for (int j = 0; j < MAX_WIDTH; j++) {
            #pragma HLS PIPELINE II=1
            if (i < height && j < width) {
                // Return integer representation of Q6.10
                // .range(15, 0) returns the raw bits
                out_ptr[i * width + j] = (signed short int)buff[i][j].range(15, 0);
            }
        }
    }
}

// Top Level Function
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

    // Local Buffers
    // Use 'static' to avoid stack overflow in C simulation
    
    // Gradients
    static grad_t Ix[MAX_HEIGHT][MAX_WIDTH];
    static grad_t Iy[MAX_HEIGHT][MAX_WIDTH];
    static grad_t It[MAX_HEIGHT][MAX_WIDTH];

    // Flow Buffers (Double Buffering)
    static flow_t u_buff[2][MAX_HEIGHT][MAX_WIDTH];
    static flow_t v_buff[2][MAX_HEIGHT][MAX_WIDTH];

    // 1. Read Inputs & Compute Gradients (Streaming)
    compute_gradients(inp1_img, inp2_img, Ix, Iy, It, height, width);

    // 2. Initialize Flow
    init_flow: for (int i = 0; i < MAX_HEIGHT; i++) {
        for (int j = 0; j < MAX_WIDTH; j++) {
            #pragma HLS PIPELINE II=1
            u_buff[0][i][j] = 0;
            v_buff[0][i][j] = 0;
            u_buff[1][i][j] = 0;
            v_buff[1][i][j] = 0;
        }
    }

    // 3. Iterations
    iter_loop: for (int k = 0; k < N_ITER; k++) {
        int curr = k % 2;
        int next = (k + 1) % 2;
        update_flow(u_buff[curr], v_buff[curr], u_buff[next], v_buff[next], Ix, Iy, It, height, width);
    }

    // 4. Write Output
    // Result is in u_buff[N_ITER % 2]
    int final_idx = N_ITER % 2;
    write_output(vx_img, u_buff[final_idx], height, width);
    write_output(vy_img, v_buff[final_idx], height, width);

    return 0;
}
