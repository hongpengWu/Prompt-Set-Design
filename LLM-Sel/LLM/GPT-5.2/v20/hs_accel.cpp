#include "hs_config_params.h"

static void hs_update_pass(flow_t u_in[MAX_HEIGHT][MAX_WIDTH],
                           flow_t v_in[MAX_HEIGHT][MAX_WIDTH],
                           flow_t u_out[MAX_HEIGHT][MAX_WIDTH],
                           flow_t v_out[MAX_HEIGHT][MAX_WIDTH],
                           inv_t inv_den[MAX_HEIGHT][MAX_WIDTH],
                           grad_t Ix[MAX_HEIGHT][MAX_WIDTH],
                           grad_t Iy[MAX_HEIGHT][MAX_WIDTH],
                           grad_t It[MAX_HEIGHT][MAX_WIDTH]) {
#pragma HLS INLINE

update_y:
  for (int i = 0; i < MAX_HEIGHT; i++) {
#pragma HLS LOOP_TRIPCOUNT min = 128 max = 128
    int im1 = (i > 0) ? (i - 1) : 0;
    int ip1 = (i < (MAX_HEIGHT - 1)) ? (i + 1) : (MAX_HEIGHT - 1);

    {
      int j = 0;
      calc_t sum_u = (calc_t)u_in[im1][j] + (calc_t)u_in[ip1][j] + (calc_t)u_in[i][j] + (calc_t)u_in[i][j + 1];
      calc_t sum_v = (calc_t)v_in[im1][j] + (calc_t)v_in[ip1][j] + (calc_t)v_in[i][j] + (calc_t)v_in[i][j + 1];

      flow_t u_avg = (flow_t)(sum_u * (calc_t)0.25);
      flow_t v_avg = (flow_t)(sum_v * (calc_t)0.25);

      grad_t ix = Ix[i][j];
      grad_t iy = Iy[i][j];
      grad_t it = It[i][j];

      calc_t num_term = ix * u_avg + iy * v_avg + it;
      calc_t update_val = num_term * inv_den[i][j];

      u_out[i][j] = u_avg - ix * update_val;
      v_out[i][j] = v_avg - iy * update_val;
    }

  update_x_interior:
    for (int j = 1; j < (MAX_WIDTH - 1); j++) {
#pragma HLS LOOP_TRIPCOUNT min = 254 max = 254
#pragma HLS PIPELINE II = 1
      calc_t sum_u = (calc_t)u_in[im1][j] + (calc_t)u_in[ip1][j] + (calc_t)u_in[i][j - 1] + (calc_t)u_in[i][j + 1];
      calc_t sum_v = (calc_t)v_in[im1][j] + (calc_t)v_in[ip1][j] + (calc_t)v_in[i][j - 1] + (calc_t)v_in[i][j + 1];

      flow_t u_avg = (flow_t)(sum_u * (calc_t)0.25);
      flow_t v_avg = (flow_t)(sum_v * (calc_t)0.25);

      grad_t ix = Ix[i][j];
      grad_t iy = Iy[i][j];
      grad_t it = It[i][j];

      calc_t num_term = ix * u_avg + iy * v_avg + it;
      calc_t update_val = num_term * inv_den[i][j];

      u_out[i][j] = u_avg - ix * update_val;
      v_out[i][j] = v_avg - iy * update_val;
    }

    {
      int j = MAX_WIDTH - 1;
      calc_t sum_u = (calc_t)u_in[im1][j] + (calc_t)u_in[ip1][j] + (calc_t)u_in[i][j - 1] + (calc_t)u_in[i][j];
      calc_t sum_v = (calc_t)v_in[im1][j] + (calc_t)v_in[ip1][j] + (calc_t)v_in[i][j - 1] + (calc_t)v_in[i][j];

      flow_t u_avg = (flow_t)(sum_u * (calc_t)0.25);
      flow_t v_avg = (flow_t)(sum_v * (calc_t)0.25);

      grad_t ix = Ix[i][j];
      grad_t iy = Iy[i][j];
      grad_t it = It[i][j];

      calc_t num_term = ix * u_avg + iy * v_avg + it;
      calc_t update_val = num_term * inv_den[i][j];

      u_out[i][j] = u_avg - ix * update_val;
      v_out[i][j] = v_avg - iy * update_val;
    }
  }
}

int hls_HS(unsigned short int inp1_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int inp2_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vx_img[MAX_HEIGHT * MAX_WIDTH],
           signed short int vy_img[MAX_HEIGHT * MAX_WIDTH],
           unsigned short int height,
           unsigned short int width) {
#pragma HLS INTERFACE m_axi port = inp1_img bundle = gmem0 offset = slave depth = 32768
#pragma HLS INTERFACE m_axi port = inp2_img bundle = gmem1 offset = slave depth = 32768
#pragma HLS INTERFACE m_axi port = vx_img bundle = gmem2 offset = slave depth = 32768
#pragma HLS INTERFACE m_axi port = vy_img bundle = gmem3 offset = slave depth = 32768
#pragma HLS INTERFACE s_axilite port = height
#pragma HLS INTERFACE s_axilite port = width
#pragma HLS INTERFACE s_axilite port = return

  static grad_t buff_Ix[MAX_HEIGHT][MAX_WIDTH];
  static grad_t buff_Iy[MAX_HEIGHT][MAX_WIDTH];
  static grad_t buff_It[MAX_HEIGHT][MAX_WIDTH];
  static inv_t buff_invden[MAX_HEIGHT][MAX_WIDTH];

  static flow_t buff_u[MAX_HEIGHT][MAX_WIDTH];
  static flow_t buff_v[MAX_HEIGHT][MAX_WIDTH];
  static flow_t buff_u_next[MAX_HEIGHT][MAX_WIDTH];
  static flow_t buff_v_next[MAX_HEIGHT][MAX_WIDTH];

#pragma HLS ARRAY_PARTITION variable = buff_u cyclic factor = 4 dim = 1
#pragma HLS ARRAY_PARTITION variable = buff_u cyclic factor = 4 dim = 2
#pragma HLS ARRAY_PARTITION variable = buff_v cyclic factor = 4 dim = 1
#pragma HLS ARRAY_PARTITION variable = buff_v cyclic factor = 4 dim = 2
#pragma HLS ARRAY_PARTITION variable = buff_u_next cyclic factor = 4 dim = 1
#pragma HLS ARRAY_PARTITION variable = buff_u_next cyclic factor = 4 dim = 2
#pragma HLS ARRAY_PARTITION variable = buff_v_next cyclic factor = 4 dim = 1
#pragma HLS ARRAY_PARTITION variable = buff_v_next cyclic factor = 4 dim = 2

  static pixel_t row_buf[3][MAX_WIDTH];
  int row_above = 0;
  int row_curr = 1;
  int row_below = 2;

  if (height != MAX_HEIGHT || width != MAX_WIDTH) {
    return 1;
  }

init_rows_y:
  for (int i = 0; i < MAX_HEIGHT; i++) {
#pragma HLS LOOP_TRIPCOUNT min = 128 max = 128
  init_rows_x:
    for (int j = 0; j < MAX_WIDTH; j++) {
#pragma HLS LOOP_TRIPCOUNT min = 256 max = 256
#pragma HLS PIPELINE II = 1
      buff_u[i][j] = 0;
      buff_v[i][j] = 0;
    }
  }

read_row0:
  for (int j = 0; j < MAX_WIDTH; j++) {
#pragma HLS LOOP_TRIPCOUNT min = 256 max = 256
#pragma HLS PIPELINE II = 1
    unsigned short int p1 = inp1_img[j];
    unsigned short int p2 = inp2_img[j];
    row_buf[row_curr][j] = (pixel_t)p1;
    buff_It[0][j] = (grad_t)((int)p2 - (int)p1);
  }

copy_row0:
  for (int j = 0; j < MAX_WIDTH; j++) {
#pragma HLS LOOP_TRIPCOUNT min = 256 max = 256
#pragma HLS PIPELINE II = 1
    row_buf[row_above][j] = row_buf[row_curr][j];
  }

read_row1:
  for (int j = 0; j < MAX_WIDTH; j++) {
#pragma HLS LOOP_TRIPCOUNT min = 256 max = 256
#pragma HLS PIPELINE II = 1
    unsigned short int p1 = inp1_img[MAX_WIDTH + j];
    unsigned short int p2 = inp2_img[MAX_WIDTH + j];
    row_buf[row_below][j] = (pixel_t)p1;
    buff_It[1][j] = (grad_t)((int)p2 - (int)p1);
  }

  int a_l = row_buf[row_above][0];
  int a_c = row_buf[row_above][0];
  int a_r = row_buf[row_above][1];
  int c_l = row_buf[row_curr][0];
  int c_c = row_buf[row_curr][0];
  int c_r = row_buf[row_curr][1];
  int b_l = row_buf[row_below][0];
  int b_c = row_buf[row_below][0];
  int b_r = row_buf[row_below][1];

grad_row0:
  for (int j = 0; j < MAX_WIDTH; j++) {
#pragma HLS LOOP_TRIPCOUNT min = 256 max = 256
#pragma HLS PIPELINE II = 1
    int val_m1_m1 = a_l;
    int val_m1_0 = a_c;
    int val_m1_p1 = a_r;

    int val_0_m1 = c_l;
    int val_0_p1 = c_r;

    int val_p1_m1 = b_l;
    int val_p1_0 = b_c;
    int val_p1_p1 = b_r;

    int gx = -val_m1_m1 + val_m1_p1 - 2 * val_0_m1 + 2 * val_0_p1 - val_p1_m1 + val_p1_p1;
    int gy = -val_m1_m1 - 2 * val_m1_0 - val_m1_p1 + val_p1_m1 + 2 * val_p1_0 + val_p1_p1;

    buff_Ix[0][j] = (grad_t)gx;
    buff_Iy[0][j] = (grad_t)gy;

    int idx = j + 2;
    if (idx >= MAX_WIDTH) idx = MAX_WIDTH - 1;

    a_l = a_c;
    a_c = a_r;
    a_r = row_buf[row_above][idx];

    c_l = c_c;
    c_c = c_r;
    c_r = row_buf[row_curr][idx];

    b_l = b_c;
    b_c = b_r;
    b_r = row_buf[row_below][idx];
  }

grad_rows_y:
  for (int i = 1; i < MAX_HEIGHT - 1; i++) {
#pragma HLS LOOP_TRIPCOUNT min = 126 max = 126
    int next_row = i + 1;
    int recycle = row_above;
    row_above = row_curr;
    row_curr = row_below;
    row_below = recycle;

  read_next_row:
    for (int j = 0; j < MAX_WIDTH; j++) {
#pragma HLS LOOP_TRIPCOUNT min = 256 max = 256
#pragma HLS PIPELINE II = 1
      unsigned short int p1 = inp1_img[next_row * MAX_WIDTH + j];
      unsigned short int p2 = inp2_img[next_row * MAX_WIDTH + j];
      row_buf[row_below][j] = (pixel_t)p1;
      buff_It[next_row][j] = (grad_t)((int)p2 - (int)p1);
    }

    int a_l = row_buf[row_above][0];
    int a_c = row_buf[row_above][0];
    int a_r = row_buf[row_above][1];
    int c_l = row_buf[row_curr][0];
    int c_c = row_buf[row_curr][0];
    int c_r = row_buf[row_curr][1];
    int b_l = row_buf[row_below][0];
    int b_c = row_buf[row_below][0];
    int b_r = row_buf[row_below][1];

  grad_row:
    for (int j = 0; j < MAX_WIDTH; j++) {
#pragma HLS LOOP_TRIPCOUNT min = 256 max = 256
#pragma HLS PIPELINE II = 1
      int val_m1_m1 = a_l;
      int val_m1_0 = a_c;
      int val_m1_p1 = a_r;

      int val_0_m1 = c_l;
      int val_0_p1 = c_r;

      int val_p1_m1 = b_l;
      int val_p1_0 = b_c;
      int val_p1_p1 = b_r;

      int gx = -val_m1_m1 + val_m1_p1 - 2 * val_0_m1 + 2 * val_0_p1 - val_p1_m1 + val_p1_p1;
      int gy = -val_m1_m1 - 2 * val_m1_0 - val_m1_p1 + val_p1_m1 + 2 * val_p1_0 + val_p1_p1;

      buff_Ix[i][j] = (grad_t)gx;
      buff_Iy[i][j] = (grad_t)gy;

      int idx = j + 2;
      if (idx >= MAX_WIDTH) idx = MAX_WIDTH - 1;

      a_l = a_c;
      a_c = a_r;
      a_r = row_buf[row_above][idx];

      c_l = c_c;
      c_c = c_r;
      c_r = row_buf[row_curr][idx];

      b_l = b_c;
      b_c = b_r;
      b_r = row_buf[row_below][idx];
    }
  }

  int tmp_above = row_above;
  row_above = row_curr;
  row_curr = row_below;
  row_below = tmp_above;

copy_last_row:
  for (int j = 0; j < MAX_WIDTH; j++) {
#pragma HLS LOOP_TRIPCOUNT min = 256 max = 256
#pragma HLS PIPELINE II = 1
    row_buf[row_below][j] = row_buf[row_curr][j];
  }

  int la_l = row_buf[row_above][0];
  int la_c = row_buf[row_above][0];
  int la_r = row_buf[row_above][1];
  int lc_l = row_buf[row_curr][0];
  int lc_c = row_buf[row_curr][0];
  int lc_r = row_buf[row_curr][1];
  int lb_l = row_buf[row_below][0];
  int lb_c = row_buf[row_below][0];
  int lb_r = row_buf[row_below][1];

grad_last_row:
  for (int j = 0; j < MAX_WIDTH; j++) {
#pragma HLS LOOP_TRIPCOUNT min = 256 max = 256
#pragma HLS PIPELINE II = 1
    int val_m1_m1 = la_l;
    int val_m1_0 = la_c;
    int val_m1_p1 = la_r;

    int val_0_m1 = lc_l;
    int val_0_p1 = lc_r;

    int val_p1_m1 = lb_l;
    int val_p1_0 = lb_c;
    int val_p1_p1 = lb_r;

    int gx = -val_m1_m1 + val_m1_p1 - 2 * val_0_m1 + 2 * val_0_p1 - val_p1_m1 + val_p1_p1;
    int gy = -val_m1_m1 - 2 * val_m1_0 - val_m1_p1 + val_p1_m1 + 2 * val_p1_0 + val_p1_p1;

    buff_Ix[MAX_HEIGHT - 1][j] = (grad_t)gx;
    buff_Iy[MAX_HEIGHT - 1][j] = (grad_t)gy;

    int idx = j + 2;
    if (idx >= MAX_WIDTH) idx = MAX_WIDTH - 1;

    la_l = la_c;
    la_c = la_r;
    la_r = row_buf[row_above][idx];

    lc_l = lc_c;
    lc_c = lc_r;
    lc_r = row_buf[row_curr][idx];

    lb_l = lb_c;
    lb_c = lb_r;
    lb_r = row_buf[row_below][idx];
  }

invden_y:
  for (int i = 0; i < MAX_HEIGHT; i++) {
#pragma HLS LOOP_TRIPCOUNT min = 128 max = 128
  invden_x:
    for (int j = 0; j < MAX_WIDTH; j++) {
#pragma HLS LOOP_TRIPCOUNT min = 256 max = 256
#pragma HLS PIPELINE II = 1
      grad_t ix = buff_Ix[i][j];
      grad_t iy = buff_Iy[i][j];
      den_t den = ALPHA_VAL * ALPHA_VAL + ix * ix + iy * iy;
      den_fx_t den_fx = (den_fx_t)den;
      ap_ufixed<32, 2, AP_RND, AP_SAT> one = 1;
      ap_ufixed<32, 2, AP_RND, AP_SAT> inv_den = one / den_fx;
      buff_invden[i][j] = (inv_t)inv_den;
    }
  }

iter_loop:
  for (int k = 0; k < N_ITER; k++) {
#pragma HLS LOOP_TRIPCOUNT min = 20 max = 20
#pragma HLS LOOP_FLATTEN off
    if ((k & 1) == 0) {
      hs_update_pass(buff_u, buff_v, buff_u_next, buff_v_next, buff_invden, buff_Ix, buff_Iy, buff_It);
    } else {
      hs_update_pass(buff_u_next, buff_v_next, buff_u, buff_v, buff_invden, buff_Ix, buff_Iy, buff_It);
    }
  }

write_loop_y:
  for (int i = 0; i < MAX_HEIGHT; i++) {
#pragma HLS LOOP_TRIPCOUNT min = 128 max = 128
  write_loop_x:
    for (int j = 0; j < MAX_WIDTH; j++) {
#pragma HLS LOOP_TRIPCOUNT min = 256 max = 256
#pragma HLS PIPELINE II = 1
      flow_t u_val = ((N_ITER & 1) == 0) ? buff_u[i][j] : buff_u_next[i][j];
      flow_t v_val = ((N_ITER & 1) == 0) ? buff_v[i][j] : buff_v_next[i][j];
      vx_img[i * MAX_WIDTH + j] = (short)u_val.range(15, 0);
      vy_img[i * MAX_WIDTH + j] = (short)v_val.range(15, 0);
    }
  }

  return 0;
}
