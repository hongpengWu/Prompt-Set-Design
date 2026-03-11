#include "hs_config_params.h"
#include <ap_int.h>

int hls_HS(unsigned short int inp1_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int inp2_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vx_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vy_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int height,
           unsigned short int width) {
#pragma HLS TOP name=hls_HS
  typedef ap_fixed<48, 28, AP_RND, AP_SAT> calc_w_t;

  static grad_t ix_buf[MAX_HEIGHT * MAX_WIDTH];
  static grad_t iy_buf[MAX_HEIGHT * MAX_WIDTH];
  static grad_t it_buf[MAX_HEIGHT * MAX_WIDTH];

  static flow_t u_curr[MAX_HEIGHT * MAX_WIDTH];
  static flow_t v_curr[MAX_HEIGHT * MAX_WIDTH];
  static flow_t u_next[MAX_HEIGHT * MAX_WIDTH];
  static flow_t v_next[MAX_HEIGHT * MAX_WIDTH];

  const int h = (int)height;
  const int w = (int)width;

grad_loop_y:
  for (int y = 0; y < MAX_HEIGHT; y++) {
grad_loop_x:
    for (int x = 0; x < MAX_WIDTH; x++) {
#pragma HLS PIPELINE
      if (y < h && x < w) {
        int ym1 = (y > 0) ? (y - 1) : 0;
        int yp1 = (y < h - 1) ? (y + 1) : (h - 1);
        int xm1 = (x > 0) ? (x - 1) : 0;
        int xp1 = (x < w - 1) ? (x + 1) : (w - 1);

        int idx = y * w + x;

        int val_m1_m1 = (int)inp1_img[ym1 * w + xm1];
        int val_m1_p1 = (int)inp1_img[ym1 * w + xp1];
        int val_0_m1 = (int)inp1_img[y * w + xm1];
        int val_0_p1 = (int)inp1_img[y * w + xp1];
        int val_p1_m1 = (int)inp1_img[yp1 * w + xm1];
        int val_p1_p1 = (int)inp1_img[yp1 * w + xp1];
        int val_m1_0 = (int)inp1_img[ym1 * w + x];
        int val_p1_0 = (int)inp1_img[yp1 * w + x];

        int gx = -val_m1_m1 + val_m1_p1 - 2 * val_0_m1 + 2 * val_0_p1 - val_p1_m1 + val_p1_p1;
        int gy = -val_m1_m1 - 2 * val_m1_0 - val_m1_p1 + val_p1_m1 + 2 * val_p1_0 + val_p1_p1;

        ix_buf[idx] = (grad_t)gx;
        iy_buf[idx] = (grad_t)gy;
        it_buf[idx] = (grad_t)((int)inp2_img[idx] - (int)inp1_img[idx]);

        u_curr[idx] = 0;
        v_curr[idx] = 0;
        u_next[idx] = 0;
        v_next[idx] = 0;
      }
    }
  }

iter_loop:
  for (int k = 0; k < N_ITER; k++) {
update_loop_y:
    for (int y = 0; y < MAX_HEIGHT; y++) {
update_loop_x:
      for (int x = 0; x < MAX_WIDTH; x++) {
#pragma HLS PIPELINE
        if (y < h && x < w) {
          int ym1 = (y > 0) ? (y - 1) : 0;
          int yp1 = (y < h - 1) ? (y + 1) : (h - 1);
          int xm1 = (x > 0) ? (x - 1) : 0;
          int xp1 = (x < w - 1) ? (x + 1) : (w - 1);

          int idx = y * w + x;

          flow_t u_avg = (u_curr[ym1 * w + x] + u_curr[yp1 * w + x] + u_curr[y * w + xm1] + u_curr[y * w + xp1]) *
                         (flow_t)0.25;
          flow_t v_avg = (v_curr[ym1 * w + x] + v_curr[yp1 * w + x] + v_curr[y * w + xm1] + v_curr[y * w + xp1]) *
                         (flow_t)0.25;

          calc_w_t ix = (calc_w_t)ix_buf[idx];
          calc_w_t iy = (calc_w_t)iy_buf[idx];
          calc_w_t it = (calc_w_t)it_buf[idx];

          calc_w_t den = (calc_w_t)(ALPHA_VAL * ALPHA_VAL) + ix * ix + iy * iy;
          calc_w_t num = ix * (calc_w_t)u_avg + iy * (calc_w_t)v_avg + it;
          calc_w_t frac = num / den;

          u_next[idx] = u_avg - (flow_t)(ix * frac);
          v_next[idx] = v_avg - (flow_t)(iy * frac);
        }
      }
    }

copy_loop_y:
    for (int y = 0; y < MAX_HEIGHT; y++) {
copy_loop_x:
      for (int x = 0; x < MAX_WIDTH; x++) {
#pragma HLS PIPELINE
        if (y < h && x < w) {
          int idx = y * w + x;
          u_curr[idx] = u_next[idx];
          v_curr[idx] = v_next[idx];
        }
      }
    }
  }

write_loop_y:
  for (int y = 0; y < MAX_HEIGHT; y++) {
write_loop_x:
    for (int x = 0; x < MAX_WIDTH; x++) {
#pragma HLS PIPELINE
      if (y < h && x < w) {
        int idx = y * w + x;
        ap_int<16> u_raw = u_curr[idx].range(15, 0);
        ap_int<16> v_raw = v_curr[idx].range(15, 0);
        vx_img[idx] = (signed short)u_raw;
        vy_img[idx] = (signed short)v_raw;
      }
    }
  }

  return 0;
}
