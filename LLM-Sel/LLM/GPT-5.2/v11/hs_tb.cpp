#include "hs_config_params.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include <png.h>

int hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT * MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT * MAX_WIDTH],
    signed short int vx_img[MAX_HEIGHT * MAX_WIDTH],
    signed short int vy_img[MAX_HEIGHT * MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
);

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int round_div_int(int num, int den) {
    if (den == 0) return 0;
    if (num >= 0) return (num + den / 2) / den;
    return (num - den / 2) / den;
}

static int abs_int(int v) {
    return (v < 0) ? -v : v;
}

static void set_pixel_rgb(uint8_t *rgb, int width, int height, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if ((unsigned)x >= (unsigned)width || (unsigned)y >= (unsigned)height) return;
    int idx = (y * width + x) * 3;
    rgb[idx + 0] = r;
    rgb[idx + 1] = g;
    rgb[idx + 2] = b;
}

static void draw_line(uint8_t *rgb, int width, int height, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b, int thickness) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? -(y1 - y0) : -(y0 - y1);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        for (int ty = -thickness; ty <= thickness; ty++) {
            for (int tx = -thickness; tx <= thickness; tx++) {
                set_pixel_rgb(rgb, width, height, x0 + tx, y0 + ty, r, g, b);
            }
        }

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

static void draw_arrow(uint8_t *rgb, int width, int height, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    if (dx == 0 && dy == 0) return;

    draw_line(rgb, width, height, x0, y0, x1, y1, r, g, b, 0);

    int len = abs_int(dx);
    if (abs_int(dy) > len) len = abs_int(dy);
    if (len <= 0) return;

    int head_len = (len * 3 + 5) / 10;
    if (head_len < 1) head_len = 1;

    int back_x = x1 - round_div_int(dx * head_len, len);
    int back_y = y1 - round_div_int(dy * head_len, len);

    int wing = head_len / 2;
    if (wing < 1) wing = 1;

    int off_x = round_div_int((-dy) * wing, len);
    int off_y = round_div_int((dx) * wing, len);

    int wing1_x = back_x + off_x;
    int wing1_y = back_y + off_y;
    int wing2_x = back_x - off_x;
    int wing2_y = back_y - off_y;

    draw_line(rgb, width, height, x1, y1, wing1_x, wing1_y, r, g, b, 0);
    draw_line(rgb, width, height, x1, y1, wing2_x, wing2_y, r, g, b, 0);
}

static int read_png_gray8(const char *path, uint8_t *out_gray, int max_h, int max_w, int *out_h, int *out_w) {
    FILE *fp = std::fopen(path, "rb");
    if (!fp) return -1;

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        std::fclose(fp);
        return -2;
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        std::fclose(fp);
        return -3;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        std::fclose(fp);
        return -4;
    }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);

    png_uint_32 w = 0;
    png_uint_32 h = 0;
    int bit_depth = 0;
    int color_type = 0;
    png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type, NULL, NULL, NULL);

    if ((int)w > max_w || (int)h > max_h) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        std::fclose(fp);
        return -5;
    }

    if (bit_depth == 16) png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);

    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGB_ALPHA || color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_rgb_to_gray_fixed(png_ptr, 1, 29900, 58700);
    }
    if (color_type & PNG_COLOR_MASK_ALPHA) png_set_strip_alpha(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    if (rowbytes < w) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        std::fclose(fp);
        return -6;
    }

    static uint8_t rowbuf[MAX_WIDTH * 4];
    for (png_uint_32 y = 0; y < h; y++) {
        png_read_row(png_ptr, rowbuf, NULL);
        for (png_uint_32 x = 0; x < w; x++) {
            out_gray[y * (int)w + (int)x] = rowbuf[x];
        }
    }

    png_read_end(png_ptr, NULL);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    std::fclose(fp);

    *out_h = (int)h;
    *out_w = (int)w;
    return 0;
}

static int write_png_rgb8(const char *path, const uint8_t *rgb, int height, int width) {
    FILE *fp = std::fopen(path, "wb");
    if (!fp) return -1;

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        std::fclose(fp);
        return -2;
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        std::fclose(fp);
        return -3;
    }
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        std::fclose(fp);
        return -4;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(
        png_ptr,
        info_ptr,
        (png_uint_32)width,
        (png_uint_32)height,
        8,
        PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE,
        PNG_FILTER_TYPE_BASE
    );
    png_write_info(png_ptr, info_ptr);

    for (int y = 0; y < height; y++) {
        png_bytep row = (png_bytep)&rgb[y * width * 3];
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    std::fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    const char *img0_path = (argc >= 2) ? argv[1] : "/home/whp/Desktop/XLab/M2HLS/lkof_im0_256x128.png";
    const char *img1_path = (argc >= 3) ? argv[2] : "/home/whp/Desktop/XLab/M2HLS/lkof_im1_256x128.png";

    static uint8_t img0_gray[MAX_HEIGHT * MAX_WIDTH];
    static uint8_t img1_gray[MAX_HEIGHT * MAX_WIDTH];
    int height = 0;
    int width = 0;
    int h1 = 0;
    int w1 = 0;

    if (read_png_gray8(img0_path, img0_gray, MAX_HEIGHT, MAX_WIDTH, &height, &width) != 0) return 1;
    if (read_png_gray8(img1_path, img1_gray, MAX_HEIGHT, MAX_WIDTH, &h1, &w1) != 0) return 1;
    if (h1 != height || w1 != width) return 1;

    static unsigned short inp0[MAX_HEIGHT * MAX_WIDTH];
    static unsigned short inp1[MAX_HEIGHT * MAX_WIDTH];
    static signed short vx_raw[MAX_HEIGHT * MAX_WIDTH];
    static signed short vy_raw[MAX_HEIGHT * MAX_WIDTH];

    for (int i = 0; i < height * width; i++) {
        inp0[i] = (unsigned short)img0_gray[i];
        inp1[i] = (unsigned short)img1_gray[i];
        vx_raw[i] = 0;
        vy_raw[i] = 0;
    }

    hls_HS(inp0, inp1, vx_raw, vy_raw, (unsigned short)height, (unsigned short)width);

    static uint8_t vis_rgb[MAX_HEIGHT * MAX_WIDTH * 3];
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t g = img0_gray[y * width + x];
            int idx = (y * width + x) * 3;
            vis_rgb[idx + 0] = g;
            vis_rgb[idx + 1] = g;
            vis_rgb[idx + 2] = g;
        }
    }

    const int step = 5;
    const int arrow_scale = 5;
    const int flow_skip_th = FLOW_SCALE / 10;

    for (int y = 0; y < height; y += step) {
        for (int x = 0; x < width; x += step) {
            int idx = y * width + x;
            int u_q = (int)vx_raw[idx];
            int v_q = (int)vy_raw[idx];

            if (abs_int(u_q) < flow_skip_th && abs_int(v_q) < flow_skip_th) continue;

            int dx = round_div_int(u_q * arrow_scale, FLOW_SCALE);
            int dy = round_div_int(v_q * arrow_scale, FLOW_SCALE);

            int x2 = clamp_int(x + dx, 0, width - 1);
            int y2 = clamp_int(y + dy, 0, height - 1);

            draw_arrow(vis_rgb, width, height, x, y, x2, y2, 0, 0, 255);
        }
    }

    const char *out_path = "/home/whp/Desktop/XLab/M2HLS/hls_out_flow11.png";
    if (write_png_rgb8(out_path, vis_rgb, height, width) != 0) return 1;

    std::printf("Saved: %s\n", out_path);
    return 0;
}
