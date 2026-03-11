#include "ukf_config_params.h"

#include <cmath>
#include <cstdint>

using real_t = double;

template <int N>
struct ReduceSum {
  static real_t run(const real_t in[N]) {
#pragma HLS inline
    real_t acc = 0.0;
    for (int i = 0; i < N; ++i) {
#pragma HLS unroll
      acc += in[i];
    }
    return acc;
  }
};

template <>
struct ReduceSum<7> {
  static real_t run(const real_t in[7]) {
#pragma HLS inline
    const real_t a0 = in[0] + in[1];
    const real_t a1 = in[2] + in[3];
    const real_t a2 = in[4] + in[5];
    const real_t a3 = in[6];
    const real_t b0 = a0 + a1;
    const real_t b1 = a2 + a3;
    return b0 + b1;
  }
};

template <>
struct ReduceSum<8> {
  static real_t run(const real_t in[8]) {
#pragma HLS inline
    const real_t a0 = in[0] + in[1];
    const real_t a1 = in[2] + in[3];
    const real_t a2 = in[4] + in[5];
    const real_t a3 = in[6] + in[7];
    const real_t b0 = a0 + a1;
    const real_t b1 = a2 + a3;
    return b0 + b1;
  }
};

template <>
struct ReduceSum<9> {
  static real_t run(const real_t in[9]) {
#pragma HLS inline
    const real_t a0 = in[0] + in[1];
    const real_t a1 = in[2] + in[3];
    const real_t a2 = in[4] + in[5];
    const real_t a3 = in[6] + in[7];
    const real_t a4 = in[8];
    const real_t b0 = a0 + a1;
    const real_t b1 = a2 + a3;
    const real_t b2 = a4;
    const real_t c0 = b0 + b1;
    return c0 + b2;
  }
};

template <int N>
static void cholupdate_upper(real_t R[N][N], const real_t x_in[N], bool add) {
  real_t x[N];
  for (int i = 0; i < N; ++i) {
    x[i] = x_in[i];
  }

  for (int k = 0; k < N; ++k) {
    const real_t Rkk = R[k][k];
    const real_t xk = x[k];

    real_t rad = Rkk * Rkk + (add ? (xk * xk) : -(xk * xk));
    if (rad < 0) {
      rad = 0;
    }
    const real_t r = std::sqrt(rad);
    const real_t c = (Rkk == 0) ? 1 : (r / Rkk);
    const real_t s = (Rkk == 0) ? 0 : (xk / Rkk);
    R[k][k] = r;

    for (int j = k + 1; j < N; ++j) {
      const real_t Rkj_old = R[k][j];
      const real_t xj_old = x[j];
      const real_t Rkj_new = (Rkj_old + (add ? (s * xj_old) : -(s * xj_old))) / c;
      R[k][j] = Rkj_new;
      x[j] = c * xj_old - s * Rkj_new;
    }
  }
}

static void cholesky_upper_3x3(const real_t P[UKF_N][UKF_N], real_t U[UKF_N][UKF_N]) {
#pragma HLS inline
  const real_t p00 = P[0][0];
  const real_t p01 = P[0][1];
  const real_t p02 = P[0][2];
  const real_t p11 = P[1][1];
  const real_t p12 = P[1][2];
  const real_t p22 = P[2][2];

  real_t r00 = p00;
  if (r00 < 0) {
    r00 = 0;
  }
  const real_t u00 = std::sqrt(r00);
  const real_t inv_u00 = (u00 == 0.0) ? 0.0 : (1.0 / u00);

  const real_t u01 = p01 * inv_u00;
  const real_t u02 = p02 * inv_u00;

  real_t r11 = p11 - u01 * u01;
  if (r11 < 0) {
    r11 = 0;
  }
  const real_t u11 = std::sqrt(r11);
  const real_t inv_u11 = (u11 == 0.0) ? 0.0 : (1.0 / u11);

  const real_t u12 = (p12 - u01 * u02) * inv_u11;

  real_t r22 = p22 - u02 * u02 - u12 * u12;
  if (r22 < 0) {
    r22 = 0;
  }
  const real_t u22 = std::sqrt(r22);

  U[0][0] = u00;
  U[0][1] = u01;
  U[0][2] = u02;
  U[1][0] = 0.0;
  U[1][1] = u11;
  U[1][2] = u12;
  U[2][0] = 0.0;
  U[2][1] = 0.0;
  U[2][2] = u22;
}

static void cholesky_upper_2x2(const real_t P[UKF_M][UKF_M], real_t U[UKF_M][UKF_M]) {
#pragma HLS inline
  const real_t p00 = P[0][0];
  const real_t p01 = P[0][1];
  const real_t p11 = P[1][1];

  real_t r00 = p00;
  if (r00 < 0) {
    r00 = 0;
  }
  const real_t u00 = std::sqrt(r00);
  const real_t inv_u00 = (u00 == 0.0) ? 0.0 : (1.0 / u00);
  const real_t u01 = p01 * inv_u00;

  real_t r11 = p11 - u01 * u01;
  if (r11 < 0) {
    r11 = 0;
  }
  const real_t u11 = std::sqrt(r11);

  U[0][0] = u00;
  U[0][1] = u01;
  U[1][0] = 0.0;
  U[1][1] = u11;
}

template <int MROWS, int NCOLS>
static void qr_R_upper(const real_t A[MROWS][NCOLS], real_t R[NCOLS][NCOLS]) {
  real_t M[MROWS][NCOLS];
#pragma HLS array_partition variable = M complete dim = 0
  for (int r = 0; r < MROWS; ++r) {
    for (int c = 0; c < NCOLS; ++c) {
      M[r][c] = A[r][c];
    }
  }

  for (int k = 0; k < NCOLS; ++k) {
    real_t norm_terms[MROWS];
#pragma HLS array_partition variable = norm_terms complete
    for (int r = 0; r < MROWS; ++r) {
#pragma HLS unroll
      if (r < k) {
        norm_terms[r] = 0.0;
      } else {
        const real_t v = M[r][k];
        norm_terms[r] = v * v;
      }
    }
    const real_t norm = std::sqrt(ReduceSum<MROWS>::run(norm_terms));
    if (norm == 0.0) {
      continue;
    }

    const real_t x0 = M[k][k];
    const real_t alpha = -std::copysign(norm, x0);
    real_t v[MROWS];
#pragma HLS array_partition variable = v complete
    for (int r = 0; r < MROWS; ++r) {
      v[r] = 0.0;
    }
    v[k] = x0 - alpha;
    for (int r = k + 1; r < MROWS; ++r) {
      v[r] = M[r][k];
    }

    real_t vnorm_terms[MROWS];
#pragma HLS array_partition variable = vnorm_terms complete
    for (int r = 0; r < MROWS; ++r) {
#pragma HLS unroll
      if (r < k) {
        vnorm_terms[r] = 0.0;
      } else {
        const real_t vr = v[r];
        vnorm_terms[r] = vr * vr;
      }
    }
    const real_t vnorm2 = ReduceSum<MROWS>::run(vnorm_terms);
    if (vnorm2 == 0.0) {
      continue;
    }
    const real_t beta = 2.0 / vnorm2;

    for (int j = k; j < NCOLS; ++j) {
      real_t dot_terms[MROWS];
#pragma HLS array_partition variable = dot_terms complete
      for (int r = 0; r < MROWS; ++r) {
#pragma HLS unroll
        if (r < k) {
          dot_terms[r] = 0.0;
        } else {
          dot_terms[r] = v[r] * M[r][j];
        }
      }
      const real_t dot = ReduceSum<MROWS>::run(dot_terms);
      for (int r = k; r < MROWS; ++r) {
        M[r][j] -= beta * v[r] * dot;
      }
    }
    M[k][k] = alpha;
    for (int r = k + 1; r < MROWS; ++r) {
      M[r][k] = 0.0;
    }
  }

  for (int i = 0; i < NCOLS; ++i) {
    for (int j = 0; j < NCOLS; ++j) {
      R[i][j] = (j < i) ? 0 : M[i][j];
    }
  }
}

static void sigmas(const real_t x[UKF_N], const real_t S[UKF_N][UKF_N], real_t coeff,
                   real_t X[UKF_N][UKF_SIGMA_POINTS]) {
  real_t A[UKF_N][UKF_N];
  for (int j = 0; j < UKF_N; ++j) {
    for (int i = 0; i < UKF_N; ++i) {
      A[i][j] = coeff * S[j][i];
    }
  }

  for (int i = 0; i < UKF_N; ++i) {
    X[i][0] = x[i];
  }
  for (int k = 0; k < UKF_N; ++k) {
    for (int i = 0; i < UKF_N; ++i) {
      X[i][1 + k] = x[i] + A[i][k];
      X[i][1 + UKF_N + k] = x[i] - A[i][k];
    }
  }
}

static void fstate(const real_t x[UKF_N], real_t y[UKF_N]) {
  y[0] = x[1];
  y[1] = x[2];
  y[2] = 0.05 * x[0] * (x[1] + x[2]);
}

static void hmeas(const real_t x[UKF_N], real_t z[UKF_M]) {
  z[0] = x[0];
  z[1] = x[1];
}

static void ut_process_meas_fused(const real_t x[UKF_N], const real_t S[UKF_N][UKF_N], real_t coeff,
                                  const real_t Wm[UKF_SIGMA_POINTS], const real_t Wc[UKF_SIGMA_POINTS], real_t x1[UKF_N],
                                  real_t z1[UKF_M], real_t S1[UKF_N][UKF_N], real_t S2[UKF_M][UKF_M],
                                  real_t P12[UKF_N][UKF_M]) {
#pragma HLS array_partition variable = Wm complete
#pragma HLS array_partition variable = Wc complete
  real_t Y[UKF_N][UKF_SIGMA_POINTS];
  real_t Z[UKF_M][UKF_SIGMA_POINTS];
#pragma HLS array_partition variable = Y complete dim = 2
#pragma HLS array_partition variable = Z complete dim = 2

  for (int i = 0; i < UKF_N; ++i) {
    x1[i] = 0.0;
  }
  for (int i = 0; i < UKF_M; ++i) {
    z1[i] = 0.0;
  }

  const real_t x0 = x[0];
  const real_t x1_in = x[1];
  const real_t x2 = x[2];

  const real_t s00 = S[0][0];
  const real_t s01 = S[0][1];
  const real_t s02 = S[0][2];
  const real_t s11 = S[1][1];
  const real_t s12 = S[1][2];
  const real_t s22 = S[2][2];

  const real_t d0_0 = coeff * s00;
  const real_t d0_1 = coeff * s01;
  const real_t d0_2 = coeff * s02;
  const real_t d1_1 = coeff * s11;
  const real_t d1_2 = coeff * s12;
  const real_t d2_2 = coeff * s22;

  for (int k = 0; k < UKF_SIGMA_POINTS; ++k) {
#pragma HLS pipeline II = 1
    real_t sx0 = x0;
    real_t sx1 = x1_in;
    real_t sx2 = x2;
    if (k == 1) {
      sx0 = x0 + d0_0;
      sx1 = x1_in + d0_1;
      sx2 = x2 + d0_2;
    } else if (k == 2) {
      sx1 = x1_in + d1_1;
      sx2 = x2 + d1_2;
    } else if (k == 3) {
      sx2 = x2 + d2_2;
    } else if (k == 4) {
      sx0 = x0 - d0_0;
      sx1 = x1_in - d0_1;
      sx2 = x2 - d0_2;
    } else if (k == 5) {
      sx1 = x1_in - d1_1;
      sx2 = x2 - d1_2;
    } else if (k == 6) {
      sx2 = x2 - d2_2;
    }

    const real_t y0 = sx1;
    const real_t y1 = sx2;
    const real_t y2 = 0.05 * sx0 * (sx1 + sx2);

    const real_t z0 = y0;
    const real_t z1k = y1;

    Y[0][k] = y0;
    Y[1][k] = y1;
    Y[2][k] = y2;
    Z[0][k] = z0;
    Z[1][k] = z1k;

    const real_t wm = Wm[k];
    x1[0] += wm * y0;
    x1[1] += wm * y1;
    x1[2] += wm * y2;
    z1[0] += wm * z0;
    z1[1] += wm * z1k;
  }

  real_t Pxx[UKF_N][UKF_N];
  real_t Pzz[UKF_M][UKF_M];
#pragma HLS array_partition variable = Pxx complete dim = 0
#pragma HLS array_partition variable = Pzz complete dim = 0
#pragma HLS array_partition variable = P12 complete dim = 0

  const real_t q2 = static_cast<real_t>(UKF_Q_STD) * static_cast<real_t>(UKF_Q_STD);
  Pxx[0][0] = q2;
  Pxx[0][1] = 0.0;
  Pxx[0][2] = 0.0;
  Pxx[1][0] = 0.0;
  Pxx[1][1] = q2;
  Pxx[1][2] = 0.0;
  Pxx[2][0] = 0.0;
  Pxx[2][1] = 0.0;
  Pxx[2][2] = q2;

  const real_t r2 = static_cast<real_t>(UKF_R_STD) * static_cast<real_t>(UKF_R_STD);
  Pzz[0][0] = r2;
  Pzz[0][1] = 0.0;
  Pzz[1][0] = 0.0;
  Pzz[1][1] = r2;

  for (int i = 0; i < UKF_N; ++i) {
    for (int j = 0; j < UKF_M; ++j) {
      P12[i][j] = 0.0;
    }
  }

  for (int k = 0; k < UKF_SIGMA_POINTS; ++k) {
#pragma HLS pipeline II = 1
    const real_t dx0 = Y[0][k] - x1[0];
    const real_t dx1 = Y[1][k] - x1[1];
    const real_t dx2 = Y[2][k] - x1[2];
    const real_t dz0 = Z[0][k] - z1[0];
    const real_t dz1 = Z[1][k] - z1[1];
    const real_t w = Wc[k];

    P12[0][0] += w * dx0 * dz0;
    P12[0][1] += w * dx0 * dz1;
    P12[1][0] += w * dx1 * dz0;
    P12[1][1] += w * dx1 * dz1;
    P12[2][0] += w * dx2 * dz0;
    P12[2][1] += w * dx2 * dz1;

    Pxx[0][0] += w * dx0 * dx0;
    Pxx[0][1] += w * dx0 * dx1;
    Pxx[0][2] += w * dx0 * dx2;
    Pxx[1][1] += w * dx1 * dx1;
    Pxx[1][2] += w * dx1 * dx2;
    Pxx[2][2] += w * dx2 * dx2;

    Pzz[0][0] += w * dz0 * dz0;
    Pzz[0][1] += w * dz0 * dz1;
    Pzz[1][1] += w * dz1 * dz1;
  }

  Pxx[1][0] = Pxx[0][1];
  Pxx[2][0] = Pxx[0][2];
  Pxx[2][1] = Pxx[1][2];
  Pzz[1][0] = Pzz[0][1];

  cholesky_upper_3x3(Pxx, S1);
  cholesky_upper_2x2(Pzz, S2);
}

static void ut_process(const real_t X[UKF_N][UKF_SIGMA_POINTS], const real_t Wm[UKF_SIGMA_POINTS],
                       const real_t Wc[UKF_SIGMA_POINTS], const real_t sqrtAbsWc[UKF_SIGMA_POINTS], real_t x1[UKF_N],
                       real_t X1[UKF_N][UKF_SIGMA_POINTS], real_t S1[UKF_N][UKF_N], real_t X2[UKF_N][UKF_SIGMA_POINTS]) {
#pragma HLS array_partition variable = Wm complete
#pragma HLS array_partition variable = X1 complete dim = 2
  for (int i = 0; i < UKF_N; ++i) {
    x1[i] = 0;
  }

  for (int k = 0; k < UKF_SIGMA_POINTS; ++k) {
    real_t yk[UKF_N];
    real_t xk[UKF_N];
    for (int i = 0; i < UKF_N; ++i) {
      xk[i] = X[i][k];
    }
    fstate(xk, yk);
    for (int i = 0; i < UKF_N; ++i) {
      X1[i][k] = yk[i];
    }
  }

  for (int i = 0; i < UKF_N; ++i) {
    real_t terms[UKF_SIGMA_POINTS];
#pragma HLS array_partition variable = terms complete
    for (int k = 0; k < UKF_SIGMA_POINTS; ++k) {
#pragma HLS unroll
      terms[k] = Wm[k] * X1[i][k];
    }
    x1[i] = ReduceSum<UKF_SIGMA_POINTS>::run(terms);
  }

  real_t P[UKF_N][UKF_N];
#pragma HLS array_partition variable = P complete dim = 0
  const real_t q2 = static_cast<real_t>(UKF_Q_STD) * static_cast<real_t>(UKF_Q_STD);
  for (int i = 0; i < UKF_N; ++i) {
    for (int j = 0; j < UKF_N; ++j) {
      P[i][j] = (i == j) ? q2 : 0.0;
    }
  }

  for (int k = 0; k < UKF_SIGMA_POINTS; ++k) {
    real_t d[UKF_N];
#pragma HLS array_partition variable = d complete
    for (int i = 0; i < UKF_N; ++i) {
      const real_t di = X1[i][k] - x1[i];
      d[i] = di;
      X2[i][k] = di;
    }
    const real_t w = Wc[k];
    P[0][0] += w * d[0] * d[0];
    P[0][1] += w * d[0] * d[1];
    P[0][2] += w * d[0] * d[2];
    P[1][1] += w * d[1] * d[1];
    P[1][2] += w * d[1] * d[2];
    P[2][2] += w * d[2] * d[2];
  }
  P[1][0] = P[0][1];
  P[2][0] = P[0][2];
  P[2][1] = P[1][2];

  cholesky_upper_3x3(P, S1);
}

static void ut_meas(const real_t X[UKF_N][UKF_SIGMA_POINTS], const real_t Wm[UKF_SIGMA_POINTS], const real_t Wc[UKF_SIGMA_POINTS],
                    const real_t sqrtAbsWc[UKF_SIGMA_POINTS], real_t z1[UKF_M], real_t Z1[UKF_M][UKF_SIGMA_POINTS],
                    real_t S2[UKF_M][UKF_M], real_t Z2[UKF_M][UKF_SIGMA_POINTS]) {
#pragma HLS array_partition variable = Wm complete
#pragma HLS array_partition variable = Z1 complete dim = 2
  for (int i = 0; i < UKF_M; ++i) {
    z1[i] = 0;
  }

  for (int k = 0; k < UKF_SIGMA_POINTS; ++k) {
    real_t zk[UKF_M];
    real_t xk[UKF_N];
    for (int i = 0; i < UKF_N; ++i) {
      xk[i] = X[i][k];
    }
    hmeas(xk, zk);
    for (int i = 0; i < UKF_M; ++i) {
      Z1[i][k] = zk[i];
    }
  }

  for (int i = 0; i < UKF_M; ++i) {
    real_t terms[UKF_SIGMA_POINTS];
#pragma HLS array_partition variable = terms complete
    for (int k = 0; k < UKF_SIGMA_POINTS; ++k) {
#pragma HLS unroll
      terms[k] = Wm[k] * Z1[i][k];
    }
    z1[i] = ReduceSum<UKF_SIGMA_POINTS>::run(terms);
  }

  real_t P[UKF_M][UKF_M];
#pragma HLS array_partition variable = P complete dim = 0
  const real_t r2 = static_cast<real_t>(UKF_R_STD) * static_cast<real_t>(UKF_R_STD);
  for (int i = 0; i < UKF_M; ++i) {
    for (int j = 0; j < UKF_M; ++j) {
      P[i][j] = (i == j) ? r2 : 0.0;
    }
  }

  for (int k = 0; k < UKF_SIGMA_POINTS; ++k) {
    real_t d[UKF_M];
#pragma HLS array_partition variable = d complete
    for (int i = 0; i < UKF_M; ++i) {
      const real_t di = Z1[i][k] - z1[i];
      d[i] = di;
      Z2[i][k] = di;
    }
    const real_t w = Wc[k];
    P[0][0] += w * d[0] * d[0];
    P[0][1] += w * d[0] * d[1];
    P[1][1] += w * d[1] * d[1];
  }
  P[1][0] = P[0][1];

  cholesky_upper_2x2(P, S2);
}

static void right_solve_upper_2(const real_t A[UKF_N][UKF_M], const real_t U[UKF_M][UKF_M], real_t X[UKF_N][UKF_M]) {
  for (int i = 0; i < UKF_N; ++i) {
    for (int j = UKF_M - 1; j >= 0; --j) {
      real_t sum = 0.0;
      for (int k = j + 1; k < UKF_M; ++k) {
        sum += X[i][k] * U[j][k];
      }
      X[i][j] = (A[i][j] - sum) / U[j][j];
    }
  }
}

static void right_solve_lower_2(const real_t A[UKF_N][UKF_M], const real_t U[UKF_M][UKF_M], real_t X[UKF_N][UKF_M]) {
  for (int i = 0; i < UKF_N; ++i) {
    for (int j = 0; j < UKF_M; ++j) {
      real_t sum = 0.0;
      for (int k = 0; k < j; ++k) {
        sum += X[i][k] * U[j][k];
      }
      X[i][j] = (A[i][j] - sum) / U[j][j];
    }
  }
}

int hls_SR_UKF(const float x0[UKF_N], const float z_seq[UKF_STEPS * UKF_M], float x_seq[UKF_STEPS * UKF_N],
               float S_upper_seq[UKF_STEPS * 6]) {
  const real_t alpha = static_cast<real_t>(UKF_ALPHA);
  const real_t beta = static_cast<real_t>(UKF_BETA);
  const real_t ki = static_cast<real_t>(UKF_KI);

  const real_t lambda = alpha * alpha * (static_cast<real_t>(UKF_N) + ki) - static_cast<real_t>(UKF_N);
  const real_t c = static_cast<real_t>(UKF_N) + lambda;

  real_t Wm[UKF_SIGMA_POINTS];
  real_t Wc[UKF_SIGMA_POINTS];
  Wm[0] = lambda / c;
  for (int i = 1; i < UKF_SIGMA_POINTS; ++i) {
    Wm[i] = 0.5 / c;
  }
  for (int i = 0; i < UKF_SIGMA_POINTS; ++i) {
    Wc[i] = Wm[i];
  }
  Wc[0] = Wc[0] + (1.0 - alpha * alpha + beta);

  real_t sqrtAbsWc[UKF_SIGMA_POINTS];
#pragma HLS array_partition variable = sqrtAbsWc complete
  for (int i = 0; i < UKF_SIGMA_POINTS; ++i) {
    sqrtAbsWc[i] = std::sqrt(std::fabs(Wc[i]));
  }

  const real_t coeff = std::sqrt(c);

  real_t x[UKF_N];
  for (int i = 0; i < UKF_N; ++i) {
    x[i] = static_cast<real_t>(x0[i]);
  }

  real_t S[UKF_N][UKF_N];
  for (int i = 0; i < UKF_N; ++i) {
    for (int j = 0; j < UKF_N; ++j) {
      S[i][j] = 0;
    }
    S[i][i] = 1;
  }

  for (int step = 0; step < UKF_STEPS; ++step) {
    real_t z[UKF_M];
    for (int i = 0; i < UKF_M; ++i) {
      z[i] = static_cast<real_t>(z_seq[step * UKF_M + i]);
    }

    real_t x1[UKF_N];
    real_t S1[UKF_N][UKF_N];
    real_t z1[UKF_M];
    real_t S2[UKF_M][UKF_M];
    real_t P12[UKF_N][UKF_M];
    ut_process_meas_fused(x, S, coeff, Wm, Wc, x1, z1, S1, S2, P12);

    real_t Ktmp[UKF_N][UKF_M];
    real_t K[UKF_N][UKF_M];
#pragma HLS array_partition variable = Ktmp complete dim = 0
#pragma HLS array_partition variable = K complete dim = 0
    const real_t s2_00 = S2[0][0];
    const real_t s2_01 = S2[0][1];
    const real_t s2_11 = S2[1][1];
    const real_t inv_s2_00 = (s2_00 == 0.0) ? 0.0 : (1.0 / s2_00);
    const real_t inv_s2_11 = (s2_11 == 0.0) ? 0.0 : (1.0 / s2_11);
    for (int i = 0; i < UKF_N; ++i) {
#pragma HLS unroll
      const real_t a0 = P12[i][0];
      const real_t a1 = P12[i][1];
      const real_t x_1 = a1 * inv_s2_11;
      const real_t x_0 = (a0 - x_1 * s2_01) * inv_s2_00;
      Ktmp[i][0] = x_0;
      Ktmp[i][1] = x_1;
    }
    for (int i = 0; i < UKF_N; ++i) {
#pragma HLS unroll
      const real_t a0 = Ktmp[i][0];
      const real_t a1 = Ktmp[i][1];
      const real_t x_0 = a0 * inv_s2_00;
      const real_t x_1 = (a1 - x_0 * s2_01) * inv_s2_11;
      K[i][0] = x_0;
      K[i][1] = x_1;
    }

    real_t dz[UKF_M];
    for (int i = 0; i < UKF_M; ++i) {
      dz[i] = z[i] - z1[i];
    }

    for (int i = 0; i < UKF_N; ++i) {
      real_t inc = 0.0;
      for (int j = 0; j < UKF_M; ++j) {
        inc += K[i][j] * dz[j];
      }
      x[i] = x1[i] + inc;
    }

    real_t U[UKF_N][UKF_M];
#pragma HLS array_partition variable = U complete dim = 0
    for (int i = 0; i < UKF_N; ++i) {
#pragma HLS unroll
      const real_t k0 = K[i][0];
      const real_t k1 = K[i][1];
      U[i][0] = k0 * s2_00 + k1 * s2_01;
      U[i][1] = k1 * s2_11;
    }

    real_t P[UKF_N][UKF_N];
#pragma HLS array_partition variable = P complete dim = 0
    const real_t a = S1[0][0];
    const real_t b = S1[0][1];
    const real_t c_ = S1[0][2];
    const real_t d = S1[1][1];
    const real_t e = S1[1][2];
    const real_t f = S1[2][2];

    P[0][0] = a * a;
    P[0][1] = a * b;
    P[0][2] = a * c_;
    P[1][0] = P[0][1];
    P[1][1] = b * b + d * d;
    P[1][2] = b * c_ + d * e;
    P[2][0] = P[0][2];
    P[2][1] = P[1][2];
    P[2][2] = c_ * c_ + e * e + f * f;

    for (int r = 0; r < UKF_N; ++r) {
      for (int cc = r; cc < UKF_N; ++cc) {
        const real_t uu = U[r][0] * U[cc][0] + U[r][1] * U[cc][1];
        const real_t v = P[r][cc] - uu;
        P[r][cc] = v;
        P[cc][r] = v;
      }
    }

    cholesky_upper_3x3(P, S);

    for (int i = 0; i < UKF_N; ++i) {
      x_seq[step * UKF_N + i] = static_cast<float>(x[i]);
    }
    S_upper_seq[step * 6 + 0] = static_cast<float>(S[0][0]);
    S_upper_seq[step * 6 + 1] = static_cast<float>(S[0][1]);
    S_upper_seq[step * 6 + 2] = static_cast<float>(S[0][2]);
    S_upper_seq[step * 6 + 3] = static_cast<float>(S[1][1]);
    S_upper_seq[step * 6 + 4] = static_cast<float>(S[1][2]);
    S_upper_seq[step * 6 + 5] = static_cast<float>(S[2][2]);
  }

  return 0;
}
