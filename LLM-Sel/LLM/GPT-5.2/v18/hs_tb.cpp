#include "hs_config_params.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <png.h>

int hls_HS(unsigned short int inp1_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int inp2_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vx_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vy_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int height,
           unsigned short int width);

static int read_png_gray8_to_u16(const char *path,
                                unsigned short *out_u16,
                                unsigned short &height,
                                unsigned short &width) {
  FILE *fp = std::fopen(path, "rb");
  if (!fp) return -1;

  png_structp png_ptr = png_create_read_struct("1.6.37", 0, 0, 0);
  if (!png_ptr) {
    std::fclose(fp);
    return -2;
  }
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_read_struct(&png_ptr, 0, 0);
    std::fclose(fp);
    return -3;
  }

  if (_setjmp((*png_set_longjmp_fn(png_ptr, longjmp, sizeof(jmp_buf))))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    std::fclose(fp);
    return -4;
  }

  png_init_io(png_ptr, fp);
  png_read_info(png_ptr, info_ptr);

  png_uint_32 w = 0;
  png_uint_32 h = 0;
  int bit_depth = 0;
  int color_type = 0;
  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type, 0, 0, 0);

  if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);
  if (bit_depth == 16) png_set_strip_16(png_ptr);
  if (bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
  if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_strip_alpha(png_ptr);

  png_read_update_info(png_ptr, info_ptr);

  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type, 0, 0, 0);
  int channels = png_get_channels(png_ptr, info_ptr);
  png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
  if (bit_depth != 8 || channels < 1 || channels > 4 || rowbytes > (png_size_t)(MAX_WIDTH * 4)) {
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    std::fclose(fp);
    return -5;
  }
  if (h > MAX_HEIGHT || w > MAX_WIDTH) {
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    std::fclose(fp);
    return -6;
  }

  static unsigned char rows_buf[MAX_HEIGHT][MAX_WIDTH * 4];
  static png_bytep rows[MAX_HEIGHT];
  for (unsigned int r = 0; r < h; r++) rows[r] = (png_bytep)rows_buf[r];

  png_read_image(png_ptr, rows);

  height = (unsigned short)h;
  width = (unsigned short)w;
  for (unsigned int r = 0; r < h; r++) {
    for (unsigned int c = 0; c < w; c++) {
      const unsigned char *row = rows_buf[r];
      unsigned char g = 0;
      if (channels == 1) {
        g = row[c];
      } else if (channels == 2) {
        g = row[c * 2];
      } else if (channels == 3) {
        unsigned int s = (unsigned int)row[c * 3] + (unsigned int)row[c * 3 + 1] + (unsigned int)row[c * 3 + 2];
        g = (unsigned char)((s + 1U) / 3U);
      } else {
        unsigned int s = (unsigned int)row[c * 4] + (unsigned int)row[c * 4 + 1] + (unsigned int)row[c * 4 + 2];
        g = (unsigned char)((s + 1U) / 3U);
      }
      out_u16[r * (unsigned int)MAX_WIDTH + c] = (unsigned short)g;
    }
  }

  png_destroy_read_struct(&png_ptr, &info_ptr, 0);
  std::fclose(fp);
  return 0;
}

static int write_png_gray8(const char *path, const unsigned char *gray, unsigned short height, unsigned short width) {
  FILE *fp = std::fopen(path, "wb");
  if (!fp) return -1;

  png_structp png_ptr = png_create_write_struct("1.6.37", 0, 0, 0);
  if (!png_ptr) {
    std::fclose(fp);
    return -2;
  }
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, 0);
    std::fclose(fp);
    return -3;
  }

  if (_setjmp((*png_set_longjmp_fn(png_ptr, longjmp, sizeof(jmp_buf))))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    std::fclose(fp);
    return -4;
  }

  png_init_io(png_ptr, fp);
  png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_GRAY, 0, 0, 0);
  png_write_info(png_ptr, info_ptr);

  static png_bytep rows[MAX_HEIGHT];
  for (int r = 0; r < (int)height; r++) rows[r] = (png_bytep)(gray + r * (int)width);
  png_write_image(png_ptr, rows);
  png_write_end(png_ptr, 0);

  png_destroy_write_struct(&png_ptr, &info_ptr);
  std::fclose(fp);
  return 0;
}

static int write_png_gray16(const char *path, const unsigned short *gray16, unsigned short height, unsigned short width) {
  FILE *fp = std::fopen(path, "wb");
  if (!fp) return -1;

  png_structp png_ptr = png_create_write_struct("1.6.37", 0, 0, 0);
  if (!png_ptr) {
    std::fclose(fp);
    return -2;
  }
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, 0);
    std::fclose(fp);
    return -3;
  }

  if (_setjmp((*png_set_longjmp_fn(png_ptr, longjmp, sizeof(jmp_buf))))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    std::fclose(fp);
    return -4;
  }

  png_init_io(png_ptr, fp);
  png_set_IHDR(png_ptr, info_ptr, width, height, 16, PNG_COLOR_TYPE_GRAY, 0, 0, 0);
  png_write_info(png_ptr, info_ptr);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  png_set_swap(png_ptr);
#endif

  static png_bytep rows[MAX_HEIGHT];
  for (int r = 0; r < (int)height; r++) rows[r] = (png_bytep)(gray16 + r * (int)width);
  png_write_image(png_ptr, rows);
  png_write_end(png_ptr, 0);

  png_destroy_write_struct(&png_ptr, &info_ptr);
  std::fclose(fp);
  return 0;
}

static int write_png_rgb8(const char *path, const unsigned char *rgb, unsigned short height, unsigned short width) {
  FILE *fp = std::fopen(path, "wb");
  if (!fp) return -1;

  png_structp png_ptr = png_create_write_struct("1.6.37", 0, 0, 0);
  if (!png_ptr) {
    std::fclose(fp);
    return -2;
  }
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, 0);
    std::fclose(fp);
    return -3;
  }

  if (_setjmp((*png_set_longjmp_fn(png_ptr, longjmp, sizeof(jmp_buf))))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    std::fclose(fp);
    return -4;
  }

  png_init_io(png_ptr, fp);
  png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, 0, 0, 0);
  png_write_info(png_ptr, info_ptr);

  static png_bytep rows[MAX_HEIGHT];
  for (int r = 0; r < (int)height; r++) rows[r] = (png_bytep)(rgb + r * (int)width * 3);
  png_write_image(png_ptr, rows);
  png_write_end(png_ptr, 0);

  png_destroy_write_struct(&png_ptr, &info_ptr);
  std::fclose(fp);
  return 0;
}

static inline int clamp_i(int v, int lo, int hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }
static inline int abs_i(int v) { return (v < 0) ? -v : v; }

static void set_rgb(unsigned char *rgb, int width, int height, int x, int y, unsigned char r, unsigned char g, unsigned char b) {
  if (x < 0 || y < 0 || x >= width || y >= height) return;
  int idx = (y * width + x) * 3;
  rgb[idx + 0] = r;
  rgb[idx + 1] = g;
  rgb[idx + 2] = b;
}

static void draw_line(unsigned char *rgb,
                      int width,
                      int height,
                      int x0,
                      int y0,
                      int x1,
                      int y1,
                      unsigned char r,
                      unsigned char g,
                      unsigned char b) {
  int dx = abs_i(x1 - x0);
  int sx = (x0 < x1) ? 1 : -1;
  int dy = -abs_i(y1 - y0);
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx + dy;

  while (true) {
    set_rgb(rgb, width, height, x0, y0, r, g, b);
    if (x0 == x1 && y0 == y1) break;
    int e2 = err << 1;
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

static void draw_disk(unsigned char *rgb,
                      int width,
                      int height,
                      int cx,
                      int cy,
                      int radius,
                      unsigned char r,
                      unsigned char g,
                      unsigned char b) {
  int r2 = radius * radius;
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      if (dx * dx + dy * dy > r2) continue;
      set_rgb(rgb, width, height, cx + dx, cy + dy, r, g, b);
    }
  }
}

static void draw_line_thick(unsigned char *rgb,
                            int width,
                            int height,
                            int x0,
                            int y0,
                            int x1,
                            int y1,
                            int thickness,
                            unsigned char r,
                            unsigned char g,
                            unsigned char b) {
  int radius = (thickness <= 1) ? 0 : (thickness / 2);

  int dx = abs_i(x1 - x0);
  int sx = (x0 < x1) ? 1 : -1;
  int dy = -abs_i(y1 - y0);
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx + dy;

  while (true) {
    if (radius == 0) {
      set_rgb(rgb, width, height, x0, y0, r, g, b);
    } else {
      draw_disk(rgb, width, height, x0, y0, radius, r, g, b);
    }

    if (x0 == x1 && y0 == y1) break;
    int e2 = err << 1;
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

static void draw_arrow(unsigned char *rgb,
                       int width,
                       int height,
                       int x0,
                       int y0,
                       int x1,
                       int y1,
                       unsigned char r,
                       unsigned char g,
                       unsigned char b,
                       float tipLength) {
  draw_line_thick(rgb, width, height, x0, y0, x1, y1, 2, r, g, b);

  float dx = (float)(x1 - x0);
  float dy = (float)(y1 - y0);
  float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1.0f) return;

  float head_len = len * tipLength;
  if (head_len < 2.0f) head_len = 2.0f;
  if (head_len > 20.0f) head_len = 20.0f;

  float angle = std::atan2((float)(y0 - y1), (float)(x0 - x1));
  const float pi = 3.14159265358979323846f;
  float a1 = angle + (pi / 6.0f);
  float a2 = angle - (pi / 6.0f);

  int hx1 = x1 + (int)std::lround(head_len * std::cos(a1));
  int hy1 = y1 + (int)std::lround(head_len * std::sin(a1));
  int hx2 = x1 + (int)std::lround(head_len * std::cos(a2));
  int hy2 = y1 + (int)std::lround(head_len * std::sin(a2));

  draw_line_thick(rgb, width, height, x1, y1, hx1, hy1, 2, r, g, b);
  draw_line_thick(rgb, width, height, x1, y1, hx2, hy2, 2, r, g, b);
}

int main() {
  static unsigned short int inp1[MAX_HEIGHT * MAX_WIDTH];
  static unsigned short int inp2[MAX_HEIGHT * MAX_WIDTH];
  static signed short int vx[MAX_HEIGHT * MAX_WIDTH];
  static signed short int vy[MAX_HEIGHT * MAX_WIDTH];

  unsigned short h1 = 0, w1 = 0;
  unsigned short h2 = 0, w2 = 0;

  const char *in0 = "/home/whp/Desktop/XLab/M2HLS/lkof_im0_256x128.png";
  const char *in1 = "/home/whp/Desktop/XLab/M2HLS/lkof_im1_256x128.png";

  const char *out_vx_path = "/home/whp/Desktop/XLab/M2HLS/hls_out_vx.png";
  const char *out_vy_path = "/home/whp/Desktop/XLab/M2HLS/hls_out_vy.png";
  const char *out_vx16_path = "/home/whp/Desktop/XLab/M2HLS/hls_out_vx16.png";
  const char *out_vy16_path = "/home/whp/Desktop/XLab/M2HLS/hls_out_vy16.png";
  const char *out_arrows_path = "/home/whp/Desktop/XLab/M2HLS/hls_out_flow_arrows.png";
  const char *out_flow18_path = "/home/whp/Desktop/XLab/M2HLS/hls_out_flow18.png";

  int rc0 = read_png_gray8_to_u16(in0, inp1, h1, w1);
  int rc1 = read_png_gray8_to_u16(in1, inp2, h2, w2);
  if (rc0 != 0 || rc1 != 0 || h1 != h2 || w1 != w2) {
    std::printf("input read failed: rc0=%d rc1=%d h1=%u w1=%u h2=%u w2=%u\n", rc0, rc1, h1, w1, h2, w2);
    return 1;
  }

  int hs_rc = hls_HS(inp1, inp2, vx, vy, h1, w1);
  if (hs_rc != 0) {
    std::printf("hls_HS failed: %d\n", hs_rc);
    return 2;
  }

  static unsigned short out_vx_u16[MAX_HEIGHT * MAX_WIDTH];
  static unsigned short out_vy_u16[MAX_HEIGHT * MAX_WIDTH];
  for (int r = 0; r < (int)h1; r++) {
    for (int c = 0; c < (int)w1; c++) {
      int idx = r * (int)MAX_WIDTH + c;
      out_vx_u16[r * (int)w1 + c] = (unsigned short)(-vx[idx]);
      out_vy_u16[r * (int)w1 + c] = (unsigned short)vy[idx];
    }
  }

  int wrc_vx16 = write_png_gray16(out_vx16_path, out_vx_u16, h1, w1);
  int wrc_vy16 = write_png_gray16(out_vy16_path, out_vy_u16, h1, w1);
  if (wrc_vx16 != 0 || wrc_vy16 != 0) {
    std::printf("write vx16/vy16 failed: vx=%d vy=%d\n", wrc_vx16, wrc_vy16);
    return 3;
  }

  static unsigned char out_vx_gray[MAX_HEIGHT * MAX_WIDTH];
  static unsigned char out_vy_gray[MAX_HEIGHT * MAX_WIDTH];
  for (int r = 0; r < (int)h1; r++) {
    for (int c = 0; c < (int)w1; c++) {
      int idx = r * (int)MAX_WIDTH + c;
      float u = -(float)vx[idx] / (float)(1 << FLOW_FRAC_BITS);
      float v = (float)vy[idx] / (float)(1 << FLOW_FRAC_BITS);
      int vx_disp = (int)std::lround(u * 10.0f + 128.0f);
      int vy_disp = (int)std::lround(v * 10.0f + 128.0f);
      out_vx_gray[r * (int)w1 + c] = (unsigned char)clamp_i(vx_disp, 0, 255);
      out_vy_gray[r * (int)w1 + c] = (unsigned char)clamp_i(vy_disp, 0, 255);
    }
  }

  int wrc_vx = write_png_gray8(out_vx_path, out_vx_gray, h1, w1);
  int wrc_vy = write_png_gray8(out_vy_path, out_vy_gray, h1, w1);
  if (wrc_vx != 0 || wrc_vy != 0) {
    std::printf("write vx/vy failed: vx=%d vy=%d\n", wrc_vx, wrc_vy);
    return 4;
  }

  static unsigned char out_rgb[MAX_HEIGHT * MAX_WIDTH * 3];
  for (int r = 0; r < (int)h1; r++) {
    for (int c = 0; c < (int)w1; c++) {
      unsigned char g = (unsigned char)inp1[r * (int)MAX_WIDTH + c];
      int idx = (r * (int)w1 + c) * 3;
      out_rgb[idx + 0] = g;
      out_rgb[idx + 1] = g;
      out_rgb[idx + 2] = g;
    }
  }

  const int step = 10;
  const float arrow_scale = 1.0f;
  const float tipLength = 0.3f;
  for (int y = 0; y < (int)h1; y += step) {
    for (int x = 0; x < (int)w1; x += step) {
      int idx = y * (int)MAX_WIDTH + x;
      float u = -(float)vx[idx] / (float)(1 << FLOW_FRAC_BITS);
      float v = (float)vy[idx] / (float)(1 << FLOW_FRAC_BITS);

      int x2 = (int)std::lround((float)x + u * arrow_scale);
      int y2 = (int)std::lround((float)y + v * arrow_scale);
      x2 = clamp_i(x2, 0, (int)w1 - 1);
      y2 = clamp_i(y2, 0, (int)h1 - 1);

      draw_arrow(out_rgb, (int)w1, (int)h1, x, y, x2, y2, 255, 0, 0, tipLength);
    }
  }

  int wrc = write_png_rgb8(out_arrows_path, out_rgb, h1, w1);
  if (wrc != 0) {
    std::printf("write arrows failed: %d\n", wrc);
    return 5;
  }

  int wrc2 = write_png_rgb8(out_flow18_path, out_rgb, h1, w1);
  if (wrc2 != 0) {
    std::printf("write flow18 failed: %d\n", wrc2);
    return 6;
  }

  std::printf("OK: wrote %s %s %s %s %s %s\n",
              out_vx16_path,
              out_vy16_path,
              out_vx_path,
              out_vy_path,
              out_arrows_path,
              out_flow18_path);
  return 0;
}
