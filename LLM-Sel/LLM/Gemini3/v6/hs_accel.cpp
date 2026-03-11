#include "hs_config_params.h"

void hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
) {
    #pragma HLS INTERFACE m_axi port=inp1_img offset=slave bundle=gmem1 depth=32768
    #pragma HLS INTERFACE m_axi port=inp2_img offset=slave bundle=gmem2 depth=32768
    #pragma HLS INTERFACE m_axi port=vx_img offset=slave bundle=gmem3 depth=32768
    #pragma HLS INTERFACE m_axi port=vy_img offset=slave bundle=gmem4 depth=32768
    #pragma HLS INTERFACE s_axilite port=height
    #pragma HLS INTERFACE s_axilite port=width
    #pragma HLS INTERFACE s_axilite port=return

    // Buffers for Gradients
    static grad_t Ix[MAX_HEIGHT][MAX_WIDTH];
    static grad_t Iy[MAX_HEIGHT][MAX_WIDTH];
    static grad_t It[MAX_HEIGHT][MAX_WIDTH];
    #pragma HLS BIND_STORAGE variable=Ix type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=Iy type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=It type=ram_2p impl=bram

    // Buffers for Velocity (Ping-Pong)
    static vel_storage_t u_curr[MAX_HEIGHT][MAX_WIDTH];
    static vel_storage_t v_curr[MAX_HEIGHT][MAX_WIDTH];
    static vel_storage_t u_next[MAX_HEIGHT][MAX_WIDTH];
    static vel_storage_t v_next[MAX_HEIGHT][MAX_WIDTH];
    #pragma HLS BIND_STORAGE variable=u_curr type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=v_curr type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=u_next type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=v_next type=ram_2p impl=bram

    // Line Buffers for Gradient Computation
    unsigned short lb1[3][MAX_WIDTH];
    unsigned short lb2[3][MAX_WIDTH];
    #pragma HLS ARRAY_PARTITION variable=lb1 complete dim=1
    #pragma HLS ARRAY_PARTITION variable=lb2 complete dim=1

    // Window registers
    unsigned short win1[3][3];
    unsigned short win2[3][3];
    #pragma HLS ARRAY_PARTITION variable=win1 complete dim=0
    #pragma HLS ARRAY_PARTITION variable=win2 complete dim=0

    // 1. Compute Gradients and Initialize Velocity
    grad_loop_y: for(int y = 0; y < height + 1; y++) {
        grad_loop_x: for(int x = 0; x < width + 1; x++) {
            #pragma HLS PIPELINE II=1

            // --- Read Input & Update Line Buffer ---
            unsigned short p1 = 0;
            unsigned short p2 = 0;
            
            if (y < height && x < width) {
                p1 = inp1_img[y * width + x];
                p2 = inp2_img[y * width + x];
            }

            if (x < width) {
                lb1[0][x] = lb1[1][x];
                lb1[1][x] = lb1[2][x];
                lb1[2][x] = p1;

                lb2[0][x] = lb2[1][x];
                lb2[1][x] = lb2[2][x];
                lb2[2][x] = p2;
            }

            // --- Update Window ---
            for(int i=0; i<3; i++) {
                win1[i][0] = win1[i][1];
                win1[i][1] = win1[i][2];
                if (x < width) {
                    win1[i][2] = lb1[i][x];
                } else {
                    win1[i][2] = 0;
                }
                
                win2[i][0] = win2[i][1];
                win2[i][1] = win2[i][2];
                if (x < width) {
                    win2[i][2] = lb2[i][x];
                } else {
                    win2[i][2] = 0;
                }
            }

            // --- Compute Gradient ---
            int r = y - 1;
            int c = x - 1;

            if (r >= 0 && r < height && c >= 0 && c < width) {
                u_curr[r][c] = 0;
                v_curr[r][c] = 0;
                
                if (r > 0 && r < height - 1 && c > 0 && c < width - 1) {
                    // Ix: Sobel X
                    int val_Ix = 
                        (int)win1[0][2] - (int)win1[0][0] + 
                        2*((int)win1[1][2] - (int)win1[1][0]) + 
                        (int)win1[2][2] - (int)win1[2][0];
                        
                    // Iy: Sobel Y
                    int val_Iy = 
                        (int)win1[2][0] + 2*((int)win1[2][1]) + (int)win1[2][2] -
                        ((int)win1[0][0] + 2*((int)win1[0][1]) + (int)win1[0][2]);
                        
                    // It: I2 - I1 (Center)
                    int val_It = (int)win2[1][1] - (int)win1[1][1];
                    
                    Ix[r][c] = val_Ix;
                    Iy[r][c] = val_Iy;
                    It[r][c] = val_It;
                } else {
                    Ix[r][c] = 0;
                    Iy[r][c] = 0;
                    It[r][c] = 0;
                }
            }
        }
    }

    // 2. Iterative Update
    const int alpha2 = ALPHA_VAL * ALPHA_VAL;

    iter_loop: for(int k=0; k<N_ITER; k++) {
        bool use_curr_as_src = (k % 2 == 0);

        update_loop_y: for(int y=0; y<height; y++) {
            update_loop_x: for(int x=0; x<width; x++) {
                #pragma HLS PIPELINE II=1
                
                grad_t dx = Ix[y][x];
                grad_t dy = Iy[y][x];
                grad_t dt = It[y][x];
                
                // Neighbor Access with Clamp
                int r_up = (y > 0) ? y - 1 : 0;
                int r_dn = (y < height - 1) ? y + 1 : height - 1;
                int c_lf = (x > 0) ? x - 1 : 0;
                int c_rt = (x < width - 1) ? x + 1 : width - 1;
                
                vel_storage_t u_up, u_dn, u_lf, u_rt;
                vel_storage_t v_up, v_dn, v_lf, v_rt;

                if (use_curr_as_src) {
                    u_up = u_curr[r_up][x]; u_dn = u_curr[r_dn][x];
                    u_lf = u_curr[y][c_lf]; u_rt = u_curr[y][c_rt];
                    v_up = v_curr[r_up][x]; v_dn = v_curr[r_dn][x];
                    v_lf = v_curr[y][c_lf]; v_rt = v_curr[y][c_rt];
                } else {
                    u_up = u_next[r_up][x]; u_dn = u_next[r_dn][x];
                    u_lf = u_next[y][c_lf]; u_rt = u_next[y][c_rt];
                    v_up = v_next[r_up][x]; v_dn = v_next[r_dn][x];
                    v_lf = v_next[y][c_lf]; v_rt = v_next[y][c_rt];
                }
                
                // Average = sum / 4
                // Use larger type for sum to avoid overflow before division
                vel_calc_t sum_u = (vel_calc_t)u_up + (vel_calc_t)u_dn + (vel_calc_t)u_lf + (vel_calc_t)u_rt;
                vel_calc_t sum_v = (vel_calc_t)v_up + (vel_calc_t)v_dn + (vel_calc_t)v_lf + (vel_calc_t)v_rt;
                
                vel_storage_t u_avg = (vel_storage_t)(sum_u * (vel_calc_t)0.25);
                vel_storage_t v_avg = (vel_storage_t)(sum_v * (vel_calc_t)0.25);
                
                long long dx2 = (long long)dx * dx;
                long long dy2 = (long long)dy * dy;
                long long denom_int = alpha2 + dx2 + dy2;
                
                vel_calc_t term = (vel_calc_t)dx * u_avg + (vel_calc_t)dy * v_avg + (vel_calc_t)dt;
                vel_calc_t factor = term / denom_int;
                
                vel_calc_t u_res = u_avg - (vel_calc_t)dx * factor;
                vel_calc_t v_res = v_avg - (vel_calc_t)dy * factor;
                
                if (use_curr_as_src) {
                    u_next[y][x] = u_res;
                    v_next[y][x] = v_res;
                } else {
                    u_curr[y][x] = u_res;
                    v_curr[y][x] = v_res;
                }
            }
        }
    }

    // 3. Write Output
    bool result_in_curr = (N_ITER % 2 == 0); 
    
    write_loop: for(int i=0; i<height; i++) {
        for(int j=0; j<width; j++) {
            #pragma HLS PIPELINE II=1
            vel_storage_t u_val, v_val;
            if (result_in_curr) {
                u_val = u_curr[i][j];
                v_val = v_curr[i][j];
            } else {
                u_val = u_next[i][j];
                v_val = v_next[i][j];
            }
            
            vx_img[i*width + j] = (short)u_val.range(15, 0);
            vy_img[i*width + j] = (short)v_val.range(15, 0);
        }
    }
}
