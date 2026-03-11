#include "hs_config_params.h"

#include <ap_int.h>

static inline unsigned short int hs_get_u16_rep(const unsigned short int *img,
                                                int y,
                                                int x,
                                                int height,
                                                int width) {
    if (y < 0) y = 0;
    if (x < 0) x = 0;
    if (y >= height) y = height - 1;
    if (x >= width) x = width - 1;
    return img[y * width + x];
}

static inline int hs_smooth3x3_u16(const unsigned short int *img,
                                  int y,
                                  int x,
                                  int height,
                                  int width) {
    const int p00 = (int)hs_get_u16_rep(img, y - 1, x - 1, height, width);
    const int p01 = (int)hs_get_u16_rep(img, y - 1, x + 0, height, width);
    const int p02 = (int)hs_get_u16_rep(img, y - 1, x + 1, height, width);
    const int p10 = (int)hs_get_u16_rep(img, y + 0, x - 1, height, width);
    const int p11 = (int)hs_get_u16_rep(img, y + 0, x + 0, height, width);
    const int p12 = (int)hs_get_u16_rep(img, y + 0, x + 1, height, width);
    const int p20 = (int)hs_get_u16_rep(img, y + 1, x - 1, height, width);
    const int p21 = (int)hs_get_u16_rep(img, y + 1, x + 0, height, width);
    const int p22 = (int)hs_get_u16_rep(img, y + 1, x + 1, height, width);

    const int sum = p00 + (p01 << 1) + p02 + (p10 << 1) + (p11 << 2) + (p12 << 1) + p20 +
                    (p21 << 1) + p22;
    return (sum + 8) >> 4;
}

static inline int hs_sobel_x_from_smooth(const int *sm,
                                        int y,
                                        int x,
                                        int height,
                                        int width) {
    const int y0 = (y <= 0) ? 0 : (y - 1);
    const int y1 = y;
    const int y2 = (y >= height - 1) ? (height - 1) : (y + 1);
    const int x0 = (x <= 0) ? 0 : (x - 1);
    const int x1 = x;
    const int x2 = (x >= width - 1) ? (width - 1) : (x + 1);

    const int p00 = sm[y0 * width + x0];
    const int p10 = sm[y1 * width + x0];
    const int p20 = sm[y2 * width + x0];
    const int p02 = sm[y0 * width + x2];
    const int p12 = sm[y1 * width + x2];
    const int p22 = sm[y2 * width + x2];

    const int g = (p02 + (p12 << 1) + p22) - (p00 + (p10 << 1) + p20);
    const int r = (g >= 0) ? (g + (1 << (HS_SOBEL_SHIFT - 1))) : (g - (1 << (HS_SOBEL_SHIFT - 1)));
    return r >> HS_SOBEL_SHIFT;
}

static inline int hs_sobel_y_from_smooth(const int *sm,
                                        int y,
                                        int x,
                                        int height,
                                        int width) {
    const int y0 = (y <= 0) ? 0 : (y - 1);
    const int y2 = (y >= height - 1) ? (height - 1) : (y + 1);
    const int x0 = (x <= 0) ? 0 : (x - 1);
    const int x1 = x;
    const int x2 = (x >= width - 1) ? (width - 1) : (x + 1);

    const int p00 = sm[y0 * width + x0];
    const int p01 = sm[y0 * width + x1];
    const int p02 = sm[y0 * width + x2];
    const int p20 = sm[y2 * width + x0];
    const int p21 = sm[y2 * width + x1];
    const int p22 = sm[y2 * width + x2];

    const int g = (p20 + (p21 << 1) + p22) - (p00 + (p01 << 1) + p02);
    const int r = (g >= 0) ? (g + (1 << (HS_SOBEL_SHIFT - 1))) : (g - (1 << (HS_SOBEL_SHIFT - 1)));
    return r >> HS_SOBEL_SHIFT;
}

static inline signed short int hs_sat_s16_from_s32(int v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (signed short int)v;
}

int hls_HS(unsigned short int inp1_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int inp2_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vx_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vy_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int height,
           unsigned short int width) {
#pragma HLS INLINE off

    if (height == 0 || width == 0) return -1;
    if (height > MAX_HEIGHT || width > MAX_WIDTH) return -2;

    static int sm1[MAX_PIXELS];
    static int sm2[MAX_PIXELS];
    static int ix[MAX_PIXELS];
    static int iy[MAX_PIXELS];
    static int it[MAX_PIXELS];

    static int u0[MAX_PIXELS];
    static int v0[MAX_PIXELS];
    static int u1[MAX_PIXELS];
    static int v1[MAX_PIXELS];

    const int h = (int)height;
    const int w = (int)width;

    for (int y = 0; y < h; y++) {
#pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_HEIGHT
        for (int x = 0; x < w; x++) {
#pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_WIDTH
#pragma HLS PIPELINE II = 1
            const int idx = y * w + x;
            sm1[idx] = hs_smooth3x3_u16(inp1_img, y, x, h, w);
            sm2[idx] = hs_smooth3x3_u16(inp2_img, y, x, h, w);
            u0[idx] = 0;
            v0[idx] = 0;
        }
    }

    for (int y = 0; y < h; y++) {
#pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_HEIGHT
        for (int x = 0; x < w; x++) {
#pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_WIDTH
#pragma HLS PIPELINE II = 1
            const int idx = y * w + x;
            const int avg = (sm1[idx] + sm2[idx] + 1) >> 1;
            (void)avg;
            ix[idx] = hs_sobel_x_from_smooth(sm1, y, x, h, w);
            iy[idx] = hs_sobel_y_from_smooth(sm1, y, x, h, w);
            it[idx] = sm2[idx] - sm1[idx];
        }
    }

    const int alpha2 = HS_ALPHA_INT * HS_ALPHA_INT;

    for (int iter = 0; iter < HS_N_ITER; iter++) {
#pragma HLS LOOP_TRIPCOUNT min = HS_N_ITER max = HS_N_ITER
        for (int y = 0; y < h; y++) {
#pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_HEIGHT
            for (int x = 0; x < w; x++) {
#pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_WIDTH
#pragma HLS PIPELINE II = 1
                const int idx = y * w + x;

                const int idx_u = (y == 0) ? idx : (idx - w);
                const int idx_d = (y == h - 1) ? idx : (idx + w);
                const int idx_l = (x == 0) ? idx : (idx - 1);
                const int idx_r = (x == w - 1) ? idx : (idx + 1);

                const int ubar = (u0[idx_u] + u0[idx_d] + u0[idx_l] + u0[idx_r]) >> 2;
                const int vbar = (v0[idx_u] + v0[idx_d] + v0[idx_l] + v0[idx_r]) >> 2;

                const int fx = ix[idx];
                const int fy = iy[idx];
                const int ft = it[idx];

                const long long term =
                    (long long)fx * (long long)ubar + (long long)fy * (long long)vbar +
                    ((long long)ft * (long long)HS_FLOW_SCALE);
                const int denom = alpha2 + fx * fx + fy * fy;

                const long long du_ll = (long long)fx * term;
                const long long dv_ll = (long long)fy * term;

                const int du = (int)(du_ll / (long long)denom);
                const int dv = (int)(dv_ll / (long long)denom);

                u1[idx] = ubar - du;
                v1[idx] = vbar - dv;
            }
        }

        for (int y = 0; y < h; y++) {
#pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_HEIGHT
            for (int x = 0; x < w; x++) {
#pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_WIDTH
#pragma HLS PIPELINE II = 1
                const int idx = y * w + x;
                u0[idx] = u1[idx];
                v0[idx] = v1[idx];
            }
        }
    }

    for (int y = 0; y < h; y++) {
#pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_HEIGHT
        for (int x = 0; x < w; x++) {
#pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_WIDTH
#pragma HLS PIPELINE II = 1
            const int idx = y * w + x;
            vx_img[idx] = hs_sat_s16_from_s32(u0[idx]);
            vy_img[idx] = hs_sat_s16_from_s32(v0[idx]);
        }
    }

    return 0;
}

