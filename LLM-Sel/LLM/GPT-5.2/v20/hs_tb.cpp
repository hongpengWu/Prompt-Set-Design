#include "hs_config_params.h"

#include <png.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int hls_HS(unsigned short int inp1_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int inp2_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vx_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vy_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int height,
           unsigned short int width);

static inline unsigned char clamp_u8_int(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (unsigned char)v;
}

static int read_png_gray8_to_u16(const char *path,
                                unsigned short *out_u16,
                                unsigned short &height,
                                unsigned short &width) {
  FILE *fp = std::fopen(path, "rb");
  if (!fp) return -1;

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
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

  if (setjmp(png_jmpbuf(png_ptr))) {
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

  if (w == 0 || h == 0 || w > (png_uint_32)MAX_WIDTH || h > (png_uint_32)MAX_HEIGHT) {
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    std::fclose(fp);
    return -5;
  }

  if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);
  if (bit_depth == 16) png_set_strip_16(png_ptr);
  if (bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
  if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_strip_alpha(png_ptr);

  png_read_update_info(png_ptr, info_ptr);

  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type, 0, 0, 0);
  const int channels = png_get_channels(png_ptr, info_ptr);
  const png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
  if (bit_depth != 8 || channels < 1 || channels > 4 || rowbytes > (png_size_t)(MAX_WIDTH * 4)) {
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
        const unsigned int s = (unsigned int)row[c * 3] + (unsigned int)row[c * 3 + 1] + (unsigned int)row[c * 3 + 2];
        g = (unsigned char)((s + 1U) / 3U);
      } else {
        const unsigned int s = (unsigned int)row[c * 4] + (unsigned int)row[c * 4 + 1] + (unsigned int)row[c * 4 + 2];
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

  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
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

  if (setjmp(png_jmpbuf(png_ptr))) {
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

static int write_png_rgb8(const char *path, const unsigned char *rgb, unsigned short height, unsigned short width) {
  FILE *fp = std::fopen(path, "wb");
  if (!fp) return -1;

  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
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

  if (setjmp(png_jmpbuf(png_ptr))) {
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

static inline void set_rgb(unsigned char *rgb, int height, int width, int y, int x, unsigned char b, unsigned char g, unsigned char r) {
  if ((unsigned)x >= (unsigned)width || (unsigned)y >= (unsigned)height) return;
  const int idx = (y * width + x) * 3;
  rgb[idx + 0] = b;
  rgb[idx + 1] = g;
  rgb[idx + 2] = r;
}

static void draw_line_thick(unsigned char *rgb,
                            int height,
                            int width,
                            int x0,
                            int y0,
                            int x1,
                            int y1,
                            int thickness,
                            unsigned char b,
                            unsigned char g,
                            unsigned char r) {
  int dx = std::abs(x1 - x0);
  int sx = (x0 < x1) ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx + dy;

  const int rad = (thickness <= 1) ? 0 : (thickness / 2);
  while (true) {
    for (int oy = -rad; oy <= rad; oy++) {
      for (int ox = -rad; ox <= rad; ox++) {
        set_rgb(rgb, height, width, y0 + oy, x0 + ox, b, g, r);
      }
    }
    if (x0 == x1 && y0 == y1) break;
    const int e2 = err << 1;
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
                       int height,
                       int width,
                       float x0f,
                       float y0f,
                       float x1f,
                       float y1f,
                       int thickness,
                       float tipLength,
                       unsigned char b,
                       unsigned char g,
                       unsigned char r) {
  const float dx = x1f - x0f;
  const float dy = y1f - y0f;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1.0f) return;

  const int x0 = (int)std::lround(x0f);
  const int y0 = (int)std::lround(y0f);
  const int x1 = (int)std::lround(x1f);
  const int y1 = (int)std::lround(y1f);

  draw_line_thick(rgb, height, width, x0, y0, x1, y1, thickness, b, g, r);

  const float ux = dx / len;
  const float uy = dy / len;
  const float hx = -uy;
  const float hy = ux;

  const float head_len = len * tipLength;
  const float head_w = head_len * 0.5f;

  const float bx = x1f - ux * head_len;
  const float by = y1f - uy * head_len;

  const float lx = bx + hx * head_w;
  const float ly = by + hy * head_w;
  const float rx = bx - hx * head_w;
  const float ry = by - hy * head_w;

  draw_line_thick(rgb, height, width, x1, y1, (int)std::lround(lx), (int)std::lround(ly), thickness, b, g, r);
  draw_line_thick(rgb, height, width, x1, y1, (int)std::lround(rx), (int)std::lround(ry), thickness, b, g, r);
}

int main(int argc, char **argv) {
  const char *img0_path = "/home/whp/Desktop/XLab/M2HLS/lkof_im0_256x128.png";
  const char *img1_path = "/home/whp/Desktop/XLab/M2HLS/lkof_im1_256x128.png";
  if (argc >= 3) {
    img0_path = argv[1];
    img1_path = argv[2];
  }

  static unsigned short inp1_u16[MAX_HEIGHT * MAX_WIDTH];
  static unsigned short inp2_u16[MAX_HEIGHT * MAX_WIDTH];
  static signed short vx_s16[MAX_HEIGHT * MAX_WIDTH];
  static signed short vy_s16[MAX_HEIGHT * MAX_WIDTH];

  unsigned short height = 0;
  unsigned short width = 0;
  unsigned short h1 = 0;
  unsigned short w1 = 0;
  if (read_png_gray8_to_u16(img0_path, inp1_u16, height, width) != 0) return 1;
  if (read_png_gray8_to_u16(img1_path, inp2_u16, h1, w1) != 0) return 2;
  if (h1 != height || w1 != width) return 3;

  const int rc = hls_HS(inp1_u16, inp2_u16, vx_s16, vy_s16, height, width);
  if (rc != 0) return 4;

  static unsigned char vx_vis[MAX_HEIGHT * MAX_WIDTH];
  static unsigned char vy_vis[MAX_HEIGHT * MAX_WIDTH];
  static unsigned char arrows_rgb[MAX_HEIGHT * MAX_WIDTH * 3];

  for (int y = 0; y < (int)height; y++) {
    for (int x = 0; x < (int)width; x++) {
      const int idx = y * (int)width + x;
      const unsigned char g = (unsigned char)(inp1_u16[idx] & 0xFF);
      const int o = idx * 3;
      arrows_rgb[o + 0] = g;
      arrows_rgb[o + 1] = g;
      arrows_rgb[o + 2] = g;
    }
  }

  for (int y = 0; y < (int)height; y++) {
    for (int x = 0; x < (int)width; x++) {
      const int idx = y * (int)width + x;
      const float u = (float)vx_s16[idx] / (float)(1 << FLOW_FRAC_BITS);
      const float v = (float)vy_s16[idx] / (float)(1 << FLOW_FRAC_BITS);

      const int dvx = (int)std::lround(u * 10.0f + 128.0f);
      const int dvy = (int)std::lround(v * 10.0f + 128.0f);
      vx_vis[idx] = clamp_u8_int(dvx);
      vy_vis[idx] = clamp_u8_int(dvy);
    }
  }

  const int step = 5;
  const float arrow_scale = 5.0f;
  const float thr = 0.1f;
  for (int y = 0; y < (int)height; y += step) {
    for (int x = 0; x < (int)width; x += step) {
      const int idx = y * (int)width + x;
      const float u = (float)vx_s16[idx] / (float)(1 << FLOW_FRAC_BITS);
      const float v = (float)vy_s16[idx] / (float)(1 << FLOW_FRAC_BITS);
      if (std::fabs(u) < thr && std::fabs(v) < thr) continue;
      const float x2 = (float)x + u * arrow_scale;
      const float y2 = (float)y + v * arrow_scale;
      draw_arrow(arrows_rgb, (int)height, (int)width, (float)x, (float)y, x2, y2, 2, 0.3f, 0, 0, 255);
    }
  }

  const char *out_vx = "/home/whp/Desktop/XLab/M2HLS/hls_out_vx.png";
  const char *out_vy = "/home/whp/Desktop/XLab/M2HLS/hls_out_vy.png";
  const char *out_ar = "/home/whp/Desktop/XLab/M2HLS/hls_out_flow_arrows.png";

  if (write_png_gray8(out_vx, vx_vis, height, width) != 0) return 5;
  if (write_png_gray8(out_vy, vy_vis, height, width) != 0) return 6;
  if (write_png_rgb8(out_ar, arrows_rgb, height, width) != 0) return 7;

  return 0;
}
