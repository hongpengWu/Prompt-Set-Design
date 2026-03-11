#include "hs_config_params.h"

static inline int clamp_int(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static inline int div_round_nearest_i64(long long num, long long denom) {
  long long half = denom / 2LL;
  if (num >= 0) return (int)((num + half) / denom);
  return (int)(-(((-num) + half) / denom));
}

static inline unsigned short get_px_u16_zero(const unsigned short img[MAX_HEIGHT][MAX_WIDTH],
                                            int r,
                                            int c,
                                            unsigned short height,
                                            unsigned short width) {
  if (r < 0 || c < 0) return 0;
  if ((unsigned short)r >= height || (unsigned short)c >= width) return 0;
  return img[r][c];
}

static inline int get_q10_zero(const int img_q10[MAX_HEIGHT][MAX_WIDTH], int r, int c, unsigned short height, unsigned short width) {
  if (r < 0 || c < 0) return 0;
  if ((unsigned short)r >= height || (unsigned short)c >= width) return 0;
  return img_q10[r][c];
}

static inline int round_div4(int v) {
  if (v >= 0) return (v + 2) >> 2;
  return -(((-v) + 2) >> 2);
}

int hls_HS(unsigned short int inp1_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int inp2_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vx_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vy_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int height,
           unsigned short int width) {
#pragma HLS INTERFACE m_axi port = inp1_img offset = slave bundle = gmem0
#pragma HLS INTERFACE m_axi port = inp2_img offset = slave bundle = gmem1
#pragma HLS INTERFACE m_axi port = vx_img offset = slave bundle = gmem2
#pragma HLS INTERFACE m_axi port = vy_img offset = slave bundle = gmem3
#pragma HLS INTERFACE s_axilite port = height bundle = control
#pragma HLS INTERFACE s_axilite port = width bundle = control
#pragma HLS INTERFACE s_axilite port = return bundle = control

  if (height == 0 || width == 0 || height > MAX_HEIGHT || width > MAX_WIDTH) return -1;

  static unsigned short img1[MAX_HEIGHT][MAX_WIDTH];
  static unsigned short img2[MAX_HEIGHT][MAX_WIDTH];

  static int img1_sm_q10[MAX_HEIGHT][MAX_WIDTH];
  static int img2_sm_q10[MAX_HEIGHT][MAX_WIDTH];
  static int tmp1_q10[MAX_HEIGHT][MAX_WIDTH];
  static int tmp2_q10[MAX_HEIGHT][MAX_WIDTH];

  static int fx_q10[MAX_HEIGHT][MAX_WIDTH];
  static int fy_q10[MAX_HEIGHT][MAX_WIDTH];
  static int ft_q10[MAX_HEIGHT][MAX_WIDTH];

  static int u0[MAX_HEIGHT][MAX_WIDTH];
  static int v0[MAX_HEIGHT][MAX_WIDTH];
  static int u1[MAX_HEIGHT][MAX_WIDTH];
  static int v1[MAX_HEIGHT][MAX_WIDTH];

  VITIS_LOOP_LOAD_0: for (int r = 0; r < MAX_HEIGHT; r++) {
    VITIS_LOOP_LOAD_1: for (int c = 0; c < MAX_WIDTH; c++) {
#pragma HLS PIPELINE II = 1
      int idx = r * (int)MAX_WIDTH + c;
      if ((unsigned short)r < height && (unsigned short)c < width) {
        img1[r][c] = inp1_img[idx];
        img2[r][c] = inp2_img[idx];
      } else {
        img1[r][c] = 0;
        img2[r][c] = 0;
      }
      u0[r][c] = 0;
      v0[r][c] = 0;
    }
  }

  const int G_Q14[6] = {73, 1218, 5234, 5768, 1630, 118};

  VITIS_LOOP_GAUSS_H_0: for (int r = 0; r < MAX_HEIGHT; r++) {
    VITIS_LOOP_GAUSS_H_1: for (int c = 0; c < MAX_WIDTH; c++) {
#pragma HLS PIPELINE II = 1
      if ((unsigned short)r < height && (unsigned short)c < width) {
        long long s1 = 0;
        long long s2 = 0;
        for (int k = 0; k < 6; k++) {
          int cc = c + 3 - k;
          unsigned short p1 = get_px_u16_zero(img1, r, cc, height, width);
          unsigned short p2 = get_px_u16_zero(img2, r, cc, height, width);
          s1 += (long long)p1 * (long long)G_Q14[k];
          s2 += (long long)p2 * (long long)G_Q14[k];
        }
        tmp1_q10[r][c] = div_round_nearest_i64(s1, 1LL << (14 - FLOW_FRAC_BITS));
        tmp2_q10[r][c] = div_round_nearest_i64(s2, 1LL << (14 - FLOW_FRAC_BITS));
      } else {
        tmp1_q10[r][c] = 0;
        tmp2_q10[r][c] = 0;
      }
    }
  }

  VITIS_LOOP_GAUSS_V_0: for (int r = 0; r < MAX_HEIGHT; r++) {
    VITIS_LOOP_GAUSS_V_1: for (int c = 0; c < MAX_WIDTH; c++) {
#pragma HLS PIPELINE II = 1
      if ((unsigned short)r < height && (unsigned short)c < width) {
        long long s1 = 0;
        long long s2 = 0;
        for (int k = 0; k < 6; k++) {
          int rr = r + 3 - k;
          int p1 = get_q10_zero(tmp1_q10, rr, c, height, width);
          int p2 = get_q10_zero(tmp2_q10, rr, c, height, width);
          s1 += (long long)p1 * (long long)G_Q14[k];
          s2 += (long long)p2 * (long long)G_Q14[k];
        }
        img1_sm_q10[r][c] = div_round_nearest_i64(s1, 1LL << 14);
        img2_sm_q10[r][c] = div_round_nearest_i64(s2, 1LL << 14);
      } else {
        img1_sm_q10[r][c] = 0;
        img2_sm_q10[r][c] = 0;
      }
    }
  }

  VITIS_LOOP_DERIV_0: for (int r = 0; r < MAX_HEIGHT; r++) {
    VITIS_LOOP_DERIV_1: for (int c = 0; c < MAX_WIDTH; c++) {
#pragma HLS PIPELINE II = 1
      if ((unsigned short)r < height && (unsigned short)c < width) {
        int a1 = get_q10_zero(img1_sm_q10, r, c, height, width);
        int b1 = get_q10_zero(img1_sm_q10, r, c + 1, height, width);
        int c1 = get_q10_zero(img1_sm_q10, r + 1, c, height, width);
        int d1 = get_q10_zero(img1_sm_q10, r + 1, c + 1, height, width);

        int a2 = get_q10_zero(img2_sm_q10, r, c, height, width);
        int b2 = get_q10_zero(img2_sm_q10, r, c + 1, height, width);
        int c2 = get_q10_zero(img2_sm_q10, r + 1, c, height, width);
        int d2 = get_q10_zero(img2_sm_q10, r + 1, c + 1, height, width);

        int fx_num = (-a1 + b1 - c1 + d1) + (-a2 + b2 - c2 + d2);
        int fy_num = (-a1 - b1 + c1 + d1) + (-a2 - b2 + c2 + d2);
        int ft_num = (a1 + b1 + c1 + d1) - (a2 + b2 + c2 + d2);

        fx_q10[r][c] = round_div4(fx_num);
        fy_q10[r][c] = round_div4(fy_num);
        ft_q10[r][c] = round_div4(ft_num);
      } else {
        fx_q10[r][c] = 0;
        fy_q10[r][c] = 0;
        ft_q10[r][c] = 0;
      }
    }
  }

  const int alpha2 = HS_ALPHA * HS_ALPHA;

  VITIS_LOOP_ITER_0: for (int iter = 0; iter < HS_NITER; iter++) {
    VITIS_LOOP_ITER_1: for (int r = 0; r < MAX_HEIGHT; r++) {
      VITIS_LOOP_ITER_2: for (int c = 0; c < MAX_WIDTH; c++) {
#pragma HLS PIPELINE II = 1
        if ((unsigned short)r < height && (unsigned short)c < width) {
          int up = get_q10_zero(u0, r - 1, c, height, width);
          int dn = get_q10_zero(u0, r + 1, c, height, width);
          int lf = get_q10_zero(u0, r, c - 1, height, width);
          int rt = get_q10_zero(u0, r, c + 1, height, width);
          int ul = get_q10_zero(u0, r - 1, c - 1, height, width);
          int ur = get_q10_zero(u0, r - 1, c + 1, height, width);
          int dl = get_q10_zero(u0, r + 1, c - 1, height, width);
          int dr = get_q10_zero(u0, r + 1, c + 1, height, width);

          int vup = get_q10_zero(v0, r - 1, c, height, width);
          int vdn = get_q10_zero(v0, r + 1, c, height, width);
          int vlf = get_q10_zero(v0, r, c - 1, height, width);
          int vrt = get_q10_zero(v0, r, c + 1, height, width);
          int vul = get_q10_zero(v0, r - 1, c - 1, height, width);
          int vur = get_q10_zero(v0, r - 1, c + 1, height, width);
          int vdl = get_q10_zero(v0, r + 1, c - 1, height, width);
          int vdr = get_q10_zero(v0, r + 1, c + 1, height, width);

          long long sum_u = 2LL * ((long long)up + (long long)dn + (long long)lf + (long long)rt) +
                            ((long long)ul + (long long)ur + (long long)dl + (long long)dr);
          long long sum_v = 2LL * ((long long)vup + (long long)vdn + (long long)vlf + (long long)vrt) +
                            ((long long)vul + (long long)vur + (long long)vdl + (long long)vdr);
          int uAvg = div_round_nearest_i64(sum_u, 12);
          int vAvg = div_round_nearest_i64(sum_v, 12);

          long long fx = (long long)fx_q10[r][c];
          long long fy = (long long)fy_q10[r][c];
          long long ft = (long long)ft_q10[r][c];

          long long p_q20 = fx * (long long)uAvg + fy * (long long)vAvg + (ft << FLOW_FRAC_BITS);
          long long denom_q20 = ((long long)alpha2 << (2 * FLOW_FRAC_BITS)) + fx * fx + fy * fy;
          if (denom_q20 == 0) denom_q20 = 1;

          int du_q10 = (int)((fx * p_q20) / denom_q20);
          int dv_q10 = (int)((fy * p_q20) / denom_q20);

          u1[r][c] = uAvg - du_q10;
          v1[r][c] = vAvg - dv_q10;
        } else {
          u1[r][c] = 0;
          v1[r][c] = 0;
        }
      }
    }

    VITIS_LOOP_SWAP_0: for (int r = 0; r < MAX_HEIGHT; r++) {
      VITIS_LOOP_SWAP_1: for (int c = 0; c < MAX_WIDTH; c++) {
#pragma HLS PIPELINE II = 1
        u0[r][c] = u1[r][c];
        v0[r][c] = v1[r][c];
      }
    }
  }

  VITIS_LOOP_STORE_0: for (int r = 0; r < MAX_HEIGHT; r++) {
    VITIS_LOOP_STORE_1: for (int c = 0; c < MAX_WIDTH; c++) {
#pragma HLS PIPELINE II = 1
      int idx = r * (int)MAX_WIDTH + c;
      if ((unsigned short)r < height && (unsigned short)c < width) {
        int u = u0[r][c];
        int v = v0[r][c];
        if (u > 32767) u = 32767;
        if (u < -32768) u = -32768;
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        vx_img[idx] = (signed short)u;
        vy_img[idx] = (signed short)v;
      } else {
        vx_img[idx] = 0;
        vy_img[idx] = 0;
      }
    }
  }

  return 0;
}
