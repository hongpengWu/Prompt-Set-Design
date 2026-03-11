#include "hs_config_params.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <png.h>

extern int hls_HS(unsigned short int inp1_img[MAX_HEIGHT * MAX_WIDTH],
                  unsigned short int inp2_img[MAX_HEIGHT * MAX_WIDTH],
                  signed short int vx_img[MAX_HEIGHT * MAX_WIDTH],
                  signed short int vy_img[MAX_HEIGHT * MAX_WIDTH],
                  unsigned short int height,
                  unsigned short int width);

static inline int idx_xy(unsigned short int width, unsigned short int x, unsigned short int y) {
  return (int)y * (int)width + (int)x;
}

static inline unsigned short get_u16_rep(const unsigned short *img,
                                         unsigned short width,
                                         unsigned short height,
                                         int x,
                                         int y) {
  int xx = x;
  int yy = y;
  if (xx < 0) xx = 0;
  if (yy < 0) yy = 0;
  if (xx >= (int)width) xx = (int)width - 1;
  if (yy >= (int)height) yy = (int)height - 1;
  return img[idx_xy(width, (unsigned short)xx, (unsigned short)yy)];
}

static void hs_float_reference(const unsigned short *i1,
                               const unsigned short *i2,
                               float *u_out,
                               float *v_out,
                               unsigned short height,
                               unsigned short width) {
  const int npix = (int)height * (int)width;
  static float Ix[MAX_PIXELS];
  static float Iy[MAX_PIXELS];
  static float It[MAX_PIXELS];
  static float u0[MAX_PIXELS];
  static float v0[MAX_PIXELS];
  static float u1[MAX_PIXELS];
  static float v1[MAX_PIXELS];

  for (int i = 0; i < npix; i++) {
    u0[i] = 0.0f;
    v0[i] = 0.0f;
    u1[i] = 0.0f;
    v1[i] = 0.0f;
  }

  for (unsigned short y = 0; y < height; y++) {
    for (unsigned short x = 0; x < width; x++) {
      unsigned short p00 = get_u16_rep(i1, width, height, (int)x - 1, (int)y - 1);
      unsigned short p01 = get_u16_rep(i1, width, height, (int)x + 0, (int)y - 1);
      unsigned short p02 = get_u16_rep(i1, width, height, (int)x + 1, (int)y - 1);
      unsigned short p10 = get_u16_rep(i1, width, height, (int)x - 1, (int)y + 0);
      unsigned short p12 = get_u16_rep(i1, width, height, (int)x + 1, (int)y + 0);
      unsigned short p20 = get_u16_rep(i1, width, height, (int)x - 1, (int)y + 1);
      unsigned short p21 = get_u16_rep(i1, width, height, (int)x + 0, (int)y + 1);
      unsigned short p22 = get_u16_rep(i1, width, height, (int)x + 1, (int)y + 1);

      int gx = -(int)p00 + (int)p02 - 2 * (int)p10 + 2 * (int)p12 - (int)p20 + (int)p22;
      int gy = -(int)p00 - 2 * (int)p01 - (int)p02 + (int)p20 + 2 * (int)p21 + (int)p22;

      const int ix_i = gx >> SOBEL_NORM_SHIFT;
      const int iy_i = gy >> SOBEL_NORM_SHIFT;
      const int it_i = (int)i2[idx_xy(width, x, y)] - (int)i1[idx_xy(width, x, y)];

      int idx = idx_xy(width, x, y);
      Ix[idx] = (float)ix_i;
      Iy[idx] = (float)iy_i;
      It[idx] = (float)it_i;
    }
  }

  const float alpha = (float)ALPHA_VAL;
  const float alpha2 = alpha * alpha;

  for (int iter = 0; iter < N_ITER; iter++) {
    const bool even = ((iter & 1) == 0);
    float *u_in = even ? u0 : u1;
    float *v_in = even ? v0 : v1;
    float *u_nx = even ? u1 : u0;
    float *v_nx = even ? v1 : v0;

    for (unsigned short y = 0; y < height; y++) {
      for (unsigned short x = 0; x < width; x++) {
        unsigned short xm1 = (x == 0) ? 0 : (unsigned short)(x - 1);
        unsigned short xp1 = (x + 1 >= width) ? (unsigned short)(width - 1) : (unsigned short)(x + 1);
        unsigned short ym1 = (y == 0) ? 0 : (unsigned short)(y - 1);
        unsigned short yp1 = (y + 1 >= height) ? (unsigned short)(height - 1) : (unsigned short)(y + 1);

        int idx = idx_xy(width, x, y);
        int idx_l = idx_xy(width, xm1, y);
        int idx_r = idx_xy(width, xp1, y);
        int idx_u = idx_xy(width, x, ym1);
        int idx_d = idx_xy(width, x, yp1);

        const float ubar = 0.25f * (u_in[idx_l] + u_in[idx_r] + u_in[idx_u] + u_in[idx_d]);
        const float vbar = 0.25f * (v_in[idx_l] + v_in[idx_r] + v_in[idx_u] + v_in[idx_d]);

        const float ix = Ix[idx];
        const float iy = Iy[idx];
        const float it = It[idx];

        const float p = ix * ubar + iy * vbar + it;
        const float denom = alpha2 + ix * ix + iy * iy;
        const float frac = p / denom;

        u_nx[idx] = ubar - ix * frac;
        v_nx[idx] = vbar - iy * frac;
      }
    }
  }

  const bool odd = ((N_ITER & 1) != 0);
  float *u_final = odd ? u1 : u0;
  float *v_final = odd ? v1 : v0;
  for (int i = 0; i < npix; i++) {
    u_out[i] = u_final[i];
    v_out[i] = v_final[i];
  }
}

static bool read_png_u16_gray(const char *path, unsigned short *out, unsigned short &height, unsigned short &width) {
  FILE *fp = std::fopen(path, "rb");
  if (!fp) return false;

  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png) {
    std::fclose(fp);
    return false;
  }
  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_read_struct(&png, nullptr, nullptr);
    std::fclose(fp);
    return false;
  }

  if (setjmp(png_jmpbuf(png))) {
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(fp);
    return false;
  }

  png_init_io(png, fp);
  png_read_info(png, info);

  png_uint_32 w = png_get_image_width(png, info);
  png_uint_32 h = png_get_image_height(png, info);
  int color_type = png_get_color_type(png, info);
  int bit_depth = png_get_bit_depth(png, info);

  if (w == 0 || h == 0 || w > MAX_WIDTH || h > MAX_HEIGHT) {
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(fp);
    return false;
  }

  if (bit_depth == 16) png_set_strip_16(png);
  if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
  if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
  if (color_type & PNG_COLOR_MASK_ALPHA) png_set_strip_alpha(png);

  png_read_update_info(png, info);

  png_size_t rowbytes = png_get_rowbytes(png, info);
  if (rowbytes != 3 * w) {
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(fp);
    return false;
  }

  static png_byte rowbuf[3 * MAX_WIDTH];
  for (png_uint_32 y = 0; y < h; y++) {
    png_read_row(png, rowbuf, nullptr);
    for (png_uint_32 x = 0; x < w; x++) {
      int r = rowbuf[3 * x + 0];
      int g = rowbuf[3 * x + 1];
      int b = rowbuf[3 * x + 2];
      int gray8 = (77 * r + 150 * g + 29 * b + 128) >> 8;
      out[(int)y * (int)w + (int)x] = (unsigned short)gray8;
    }
  }

  png_read_end(png, info);
  png_destroy_read_struct(&png, &info, nullptr);
  std::fclose(fp);

  height = (unsigned short)h;
  width = (unsigned short)w;
  return true;
}

static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void crc32_init() {
  if (crc32_table_init) return;
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (int k = 0; k < 8; k++) {
      c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    }
    crc32_table[i] = c;
  }
  crc32_table_init = true;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *buf, size_t len) {
  uint32_t c = crc ^ 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    c = crc32_table[(c ^ buf[i]) & 0xFFu] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFFu;
}

static uint32_t adler32_update(uint32_t adler, const uint8_t *buf, size_t len) {
  uint32_t s1 = adler & 0xFFFFu;
  uint32_t s2 = (adler >> 16) & 0xFFFFu;
  for (size_t i = 0; i < len; i++) {
    s1 += buf[i];
    if (s1 >= 65521u) s1 -= 65521u;
    s2 += s1;
    if (s2 >= 65521u) s2 -= 65521u;
  }
  return (s2 << 16) | s1;
}

static void write_be32(FILE *f, uint32_t v) {
  uint8_t b[4];
  b[0] = (uint8_t)((v >> 24) & 0xFFu);
  b[1] = (uint8_t)((v >> 16) & 0xFFu);
  b[2] = (uint8_t)((v >> 8) & 0xFFu);
  b[3] = (uint8_t)(v & 0xFFu);
  fwrite(b, 1, 4, f);
}

static void write_chunk(FILE *f, const char type[4], const uint8_t *data, uint32_t len) {
  write_be32(f, len);
  fwrite(type, 1, 4, f);
  if (len) fwrite(data, 1, len, f);
  uint32_t crc = 0;
  crc = crc32_update(crc, (const uint8_t *)type, 4);
  if (len) crc = crc32_update(crc, data, len);
  write_be32(f, crc);
}

static bool write_png_rgb(const char *path, int w, int h, const uint8_t *rgb) {
  crc32_init();
  FILE *f = std::fopen(path, "wb");
  if (!f) return false;

  const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  fwrite(sig, 1, 8, f);

  uint8_t ihdr[13];
  ihdr[0] = (uint8_t)((w >> 24) & 0xFF);
  ihdr[1] = (uint8_t)((w >> 16) & 0xFF);
  ihdr[2] = (uint8_t)((w >> 8) & 0xFF);
  ihdr[3] = (uint8_t)(w & 0xFF);
  ihdr[4] = (uint8_t)((h >> 24) & 0xFF);
  ihdr[5] = (uint8_t)((h >> 16) & 0xFF);
  ihdr[6] = (uint8_t)((h >> 8) & 0xFF);
  ihdr[7] = (uint8_t)(h & 0xFF);
  ihdr[8] = 8;
  ihdr[9] = 2;
  ihdr[10] = 0;
  ihdr[11] = 0;
  ihdr[12] = 0;
  write_chunk(f, "IHDR", ihdr, 13);

  static uint8_t raw[(1 + 3 * MAX_WIDTH) * MAX_HEIGHT];
  const int stride = 3 * w;
  const int raw_stride = 1 + stride;
  for (int y = 0; y < h; y++) {
    raw[y * raw_stride + 0] = 0;
    for (int x = 0; x < stride; x++) {
      raw[y * raw_stride + 1 + x] = rgb[y * stride + x];
    }
  }
  const uint32_t raw_len = (uint32_t)(raw_stride * h);

  static uint8_t zbuf[2 + 5 * 2 + (1 + 3 * MAX_WIDTH) * MAX_HEIGHT + 4];
  uint32_t zpos = 0;
  zbuf[zpos++] = 0x78;
  zbuf[zpos++] = 0x01;

  uint32_t adler = 1u;
  uint32_t remaining = raw_len;
  uint32_t offset = 0;
  while (remaining) {
    uint16_t block_len = (remaining > 65535u) ? 65535u : (uint16_t)remaining;
    const bool last = (remaining <= 65535u);
    zbuf[zpos++] = last ? 0x01 : 0x00;
    zbuf[zpos++] = (uint8_t)(block_len & 0xFFu);
    zbuf[zpos++] = (uint8_t)((block_len >> 8) & 0xFFu);
    uint16_t nlen = (uint16_t)~block_len;
    zbuf[zpos++] = (uint8_t)(nlen & 0xFFu);
    zbuf[zpos++] = (uint8_t)((nlen >> 8) & 0xFFu);
    for (uint16_t i = 0; i < block_len; i++) {
      uint8_t b = raw[offset + i];
      zbuf[zpos++] = b;
    }
    adler = adler32_update(adler, raw + offset, block_len);
    offset += block_len;
    remaining -= block_len;
  }

  zbuf[zpos++] = (uint8_t)((adler >> 24) & 0xFFu);
  zbuf[zpos++] = (uint8_t)((adler >> 16) & 0xFFu);
  zbuf[zpos++] = (uint8_t)((adler >> 8) & 0xFFu);
  zbuf[zpos++] = (uint8_t)(adler & 0xFFu);

  write_chunk(f, "IDAT", zbuf, zpos);
  write_chunk(f, "IEND", nullptr, 0);

  std::fclose(f);
  return true;
}

static inline void set_rgb(uint8_t *rgb, int w, int h, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h) return;
  int idx = (y * w + x) * 3;
  rgb[idx + 0] = r;
  rgb[idx + 1] = g;
  rgb[idx + 2] = b;
}

static void draw_line(uint8_t *rgb, int w, int h, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
  int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    if ((unsigned)x0 < (unsigned)w && (unsigned)y0 < (unsigned)h) set_rgb(rgb, w, h, x0, y0, r, g, b);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

static void draw_flow_quiver(uint8_t *rgb,
                             const signed short *vx,
                             const signed short *vy,
                             unsigned short height,
                             unsigned short width) {
  const int step = 32;
  const float scale = 4.0f;
  for (int y = step / 2; y < (int)height; y += step) {
    for (int x = step / 2; x < (int)width; x += step) {
      int idx = idx_xy(width, (unsigned short)x, (unsigned short)y);
      float u = (float)vx[idx] / (float)(1 << FLOW_FRAC_BITS);
      float v = (float)vy[idx] / (float)(1 << FLOW_FRAC_BITS);
      int x1 = (int)std::lround((float)x + scale * u);
      int y1 = (int)std::lround((float)y + scale * v);
      draw_line(rgb, width, height, x, y, x1, y1, 255, 0, 0);
      int hx0 = x1;
      int hy0 = y1;
      int hx1 = (int)std::lround((float)x1 - 0.3f * (float)(x1 - x) + 0.3f * (float)(y1 - y));
      int hy1 = (int)std::lround((float)y1 - 0.3f * (float)(y1 - y) - 0.3f * (float)(x1 - x));
      int hx2 = (int)std::lround((float)x1 - 0.3f * (float)(x1 - x) - 0.3f * (float)(y1 - y));
      int hy2 = (int)std::lround((float)y1 - 0.3f * (float)(y1 - y) + 0.3f * (float)(x1 - x));
      draw_line(rgb, width, height, hx0, hy0, hx1, hy1, 255, 0, 0);
      draw_line(rgb, width, height, hx0, hy0, hx2, hy2, 255, 0, 0);
    }
  }
}

int main(int argc, char **argv) {
  const char *img0 = (argc >= 3) ? argv[1] : "/home/whp/Desktop/XLab/M2HLS/lkof_im0_256x128.png";
  const char *img1 = (argc >= 3) ? argv[2] : "/home/whp/Desktop/XLab/M2HLS/lkof_im1_256x128.png";

  unsigned short height0 = 0, width0 = 0;
  unsigned short height1 = 0, width1 = 0;

  static unsigned short inp1[MAX_PIXELS];
  static unsigned short inp2[MAX_PIXELS];
  static signed short vx[MAX_PIXELS];
  static signed short vy[MAX_PIXELS];
  static float u_ref[MAX_PIXELS];
  static float v_ref[MAX_PIXELS];

  if (!read_png_u16_gray(img0, inp1, height0, width0)) {
    std::printf("Failed to read image: %s\n", img0);
    return 1;
  }
  if (!read_png_u16_gray(img1, inp2, height1, width1)) {
    std::printf("Failed to read image: %s\n", img1);
    return 1;
  }
  if (height0 != height1 || width0 != width1) {
    std::printf("Input size mismatch.\n");
    return 1;
  }
  if (height0 != MAX_HEIGHT || width0 != MAX_WIDTH) {
    std::printf("Expected %ux%u, got %ux%u.\n", (unsigned)MAX_WIDTH, (unsigned)MAX_HEIGHT, (unsigned)width0, (unsigned)height0);
    return 1;
  }

  const unsigned short height = height0;
  const unsigned short width = width0;
  const int npix = (int)height * (int)width;

  std::printf("Image Size: %ux%u\n", (unsigned)width, (unsigned)height);
  std::printf("Calling HLS function...\n");
  int ret = hls_HS(inp1, inp2, vx, vy, height, width);
  std::printf("HLS function returned: %d\n", ret);
  if (ret != 0) return 1;

  std::printf("Running float reference...\n");
  hs_float_reference(inp1, inp2, u_ref, v_ref, height, width);

  double max_err_u = 0.0, max_err_v = 0.0;
  double sum_err_u = 0.0, sum_err_v = 0.0;
  for (int i = 0; i < npix; i++) {
    double u = (double)vx[i] / (double)(1 << FLOW_FRAC_BITS);
    double v = (double)vy[i] / (double)(1 << FLOW_FRAC_BITS);
    double eu = std::fabs(u - (double)u_ref[i]);
    double ev = std::fabs(v - (double)v_ref[i]);
    if (eu > max_err_u) max_err_u = eu;
    if (ev > max_err_v) max_err_v = ev;
    sum_err_u += eu;
    sum_err_v += ev;
  }
  double mean_err_u = sum_err_u / (double)npix;
  double mean_err_v = sum_err_v / (double)npix;

  std::printf("Max Error U: %.6f\n", max_err_u);
  std::printf("Max Error V: %.6f\n", max_err_v);
  std::printf("Mean Error U: %.6f\n", mean_err_u);
  std::printf("Mean Error V: %.6f\n", mean_err_v);

  static uint8_t rgb[MAX_WIDTH * MAX_HEIGHT * 3];
  for (int y = 0; y < (int)height; y++) {
    for (int x = 0; x < (int)width; x++) {
      unsigned short p = inp1[idx_xy(width, (unsigned short)x, (unsigned short)y)];
      uint8_t g = (uint8_t)p;
      int idx = (y * (int)width + x) * 3;
      rgb[idx + 0] = g;
      rgb[idx + 1] = g;
      rgb[idx + 2] = g;
    }
  }
  draw_flow_quiver(rgb, vx, vy, height, width);
  const char *out_path = "/home/whp/Desktop/XLab/M2HLS/hls_out_flow16.png";
  bool ok = write_png_rgb(out_path, (int)width, (int)height, rgb);
  if (!ok) {
    std::printf("Failed to write output image.\n");
    return 1;
  }
  std::printf("Wrote: %s\n", out_path);

  if (mean_err_u < 0.1 && mean_err_v < 0.1) {
    std::printf("Test PASSED with warnings (mean error is low).\n");
    return 0;
  }
  std::printf("Test FAILED.\n");
  return 1;
}
