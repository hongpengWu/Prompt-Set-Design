#include "hs_config_params.h"

#include <ap_fixed.h>
#include <ap_int.h>

static inline ap_int<16> clamp_s16(int v) {
  if (v < -32768) return (ap_int<16>)-32768;
  if (v > 32767) return (ap_int<16>)32767;
  return (ap_int<16>)v;
}

static inline int idx_xy(unsigned short int width, unsigned short int x, unsigned short int y) {
  return (int)y * (int)width + (int)x;
}

static inline ap_uint<16> load_pix_u16(const ap_uint<16> *img,
                                       unsigned short int width,
                                       unsigned short int height,
                                       int x,
                                       int y) {
  int xx = x;
  int yy = y;
  if (xx < 0) xx = 0;
  if (yy < 0) yy = 0;
  if (xx >= (int)width) xx = (int)width - 1;
  if (yy >= (int)height) yy = (int)height - 1;
  return img[idx_xy(width, (unsigned short int)xx, (unsigned short int)yy)];
}

int hls_HS(unsigned short int inp1_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int inp2_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vx_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vy_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int height,
           unsigned short int width) {
#pragma HLS INTERFACE ap_memory port = inp1_img
#pragma HLS INTERFACE ap_memory port = inp2_img
#pragma HLS INTERFACE ap_memory port = vx_img
#pragma HLS INTERFACE ap_memory port = vy_img
#pragma HLS INTERFACE s_axilite port = height bundle = control
#pragma HLS INTERFACE s_axilite port = width bundle = control
#pragma HLS INTERFACE s_axilite port = return bundle = control

  if (height == 0 || width == 0) return -1;
  if (height > MAX_HEIGHT || width > MAX_WIDTH) return -2;

  typedef ap_fixed<48, 32, AP_RND, AP_SAT> calc_wide_t;

  static ap_uint<16> img1_buf[MAX_PIXELS];
  static ap_uint<16> img2_buf[MAX_PIXELS];
  static grad_t ix_buf[MAX_PIXELS];
  static grad_t iy_buf[MAX_PIXELS];
  static grad_t it_buf[MAX_PIXELS];
  static flow_t u0[MAX_PIXELS];
  static flow_t v0[MAX_PIXELS];
  static flow_t u1[MAX_PIXELS];
  static flow_t v1[MAX_PIXELS];

#pragma HLS BIND_STORAGE variable = img1_buf type = ram_2p impl = bram
#pragma HLS BIND_STORAGE variable = img2_buf type = ram_2p impl = bram
#pragma HLS BIND_STORAGE variable = ix_buf type = ram_2p impl = bram
#pragma HLS BIND_STORAGE variable = iy_buf type = ram_2p impl = bram
#pragma HLS BIND_STORAGE variable = it_buf type = ram_2p impl = bram
#pragma HLS BIND_STORAGE variable = u0 type = ram_2p impl = bram
#pragma HLS BIND_STORAGE variable = v0 type = ram_2p impl = bram
#pragma HLS BIND_STORAGE variable = u1 type = ram_2p impl = bram
#pragma HLS BIND_STORAGE variable = v1 type = ram_2p impl = bram

  const int npix = (int)height * (int)width;

  for (int i = 0; i < npix; i++) {
#pragma HLS PIPELINE II = 1
    img1_buf[i] = (ap_uint<16>)inp1_img[i];
    img2_buf[i] = (ap_uint<16>)inp2_img[i];
    u0[i] = 0;
    v0[i] = 0;
    u1[i] = 0;
    v1[i] = 0;
  }

  for (unsigned short int y = 0; y < height; y++) {
    for (unsigned short int x = 0; x < width; x++) {
#pragma HLS PIPELINE II = 1
      ap_uint<16> p00 = load_pix_u16(img1_buf, width, height, (int)x - 1, (int)y - 1);
      ap_uint<16> p01 = load_pix_u16(img1_buf, width, height, (int)x + 0, (int)y - 1);
      ap_uint<16> p02 = load_pix_u16(img1_buf, width, height, (int)x + 1, (int)y - 1);
      ap_uint<16> p10 = load_pix_u16(img1_buf, width, height, (int)x - 1, (int)y + 0);
      ap_uint<16> p12 = load_pix_u16(img1_buf, width, height, (int)x + 1, (int)y + 0);
      ap_uint<16> p20 = load_pix_u16(img1_buf, width, height, (int)x - 1, (int)y + 1);
      ap_uint<16> p21 = load_pix_u16(img1_buf, width, height, (int)x + 0, (int)y + 1);
      ap_uint<16> p22 = load_pix_u16(img1_buf, width, height, (int)x + 1, (int)y + 1);

      int gx = -(int)p00 + (int)p02 - 2 * (int)p10 + 2 * (int)p12 - (int)p20 + (int)p22;
      int gy = -(int)p00 - 2 * (int)p01 - (int)p02 + (int)p20 + 2 * (int)p21 + (int)p22;

      int ix = gx >> SOBEL_NORM_SHIFT;
      int iy = gy >> SOBEL_NORM_SHIFT;

      ap_uint<16> i1 = img1_buf[idx_xy(width, x, y)];
      ap_uint<16> i2 = img2_buf[idx_xy(width, x, y)];
      int it = (int)i2 - (int)i1;

      int idx = idx_xy(width, x, y);
      ix_buf[idx] = (grad_t)ix;
      iy_buf[idx] = (grad_t)iy;
      it_buf[idx] = (grad_t)it;
    }
  }

  const calc_t alpha = (calc_t)ALPHA_VAL;
  const calc_t alpha2 = alpha * alpha;

  for (int iter = 0; iter < N_ITER; iter++) {
    const bool even = ((iter & 1) == 0);
    flow_t *u_in = even ? u0 : u1;
    flow_t *v_in = even ? v0 : v1;
    flow_t *u_out = even ? u1 : u0;
    flow_t *v_out = even ? v1 : v0;

    for (unsigned short int y = 0; y < height; y++) {
      for (unsigned short int x = 0; x < width; x++) {
#pragma HLS PIPELINE II = 1
        int idx = idx_xy(width, x, y);

        unsigned short int xm1 = (x == 0) ? 0 : (unsigned short int)(x - 1);
        unsigned short int xp1 = (x + 1 >= width) ? (unsigned short int)(width - 1) : (unsigned short int)(x + 1);
        unsigned short int ym1 = (y == 0) ? 0 : (unsigned short int)(y - 1);
        unsigned short int yp1 = (y + 1 >= height) ? (unsigned short int)(height - 1) : (unsigned short int)(y + 1);

        int idx_l = idx_xy(width, xm1, y);
        int idx_r = idx_xy(width, xp1, y);
        int idx_u = idx_xy(width, x, ym1);
        int idx_d = idx_xy(width, x, yp1);

        calc_wide_t ubar = (calc_wide_t)(u_in[idx_l] + u_in[idx_r] + u_in[idx_u] + u_in[idx_d]) * (calc_wide_t)0.25;
        calc_wide_t vbar = (calc_wide_t)(v_in[idx_l] + v_in[idx_r] + v_in[idx_u] + v_in[idx_d]) * (calc_wide_t)0.25;

        calc_wide_t Ix = (calc_wide_t)ix_buf[idx];
        calc_wide_t Iy = (calc_wide_t)iy_buf[idx];
        calc_wide_t It = (calc_wide_t)it_buf[idx];

        calc_wide_t p = Ix * ubar + Iy * vbar + It;
        calc_wide_t denom = (calc_wide_t)alpha2 + Ix * Ix + Iy * Iy;
        calc_wide_t frac = p / denom;

        u_out[idx] = (flow_t)(ubar - Ix * frac);
        v_out[idx] = (flow_t)(vbar - Iy * frac);
      }
    }
  }

  flow_t *u_final = (N_ITER & 1) ? u1 : u0;
  flow_t *v_final = (N_ITER & 1) ? v1 : v0;

  for (int i = 0; i < npix; i++) {
#pragma HLS PIPELINE II = 1
    ap_int<16> raw_u = u_final[i].range(15, 0);
    ap_int<16> raw_v = v_final[i].range(15, 0);
    vx_img[i] = (signed short int)raw_u;
    vy_img[i] = (signed short int)raw_v;
  }

  return 0;
}
