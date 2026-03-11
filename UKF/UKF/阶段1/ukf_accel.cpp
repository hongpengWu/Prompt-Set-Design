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

  real_t residual[UKF_N][UKF_SIGMA_POINTS];
  for (int k = 0; k < UKF_SIGMA_POINTS; ++k) {
    const real_t sw = sqrtAbsWc[k];
    for (int i = 0; i < UKF_N; ++i) {
      const real_t d = X1[i][k] - x1[i];
      X2[i][k] = d;
      residual[i][k] = d * sw;
    }
  }

  constexpr int MROWS = (UKF_SIGMA_POINTS - 1) + UKF_N;
  real_t At[MROWS][UKF_N];
  for (int c = 0; c < UKF_SIGMA_POINTS - 1; ++c) {
    for (int i = 0; i < UKF_N; ++i) {
      At[c][i] = residual[i][c + 1];
    }
  }
  for (int col = 0; col < UKF_N; ++col) {
    for (int i = 0; i < UKF_N; ++i) {
      At[(UKF_SIGMA_POINTS - 1) + col][i] = (i == col) ? static_cast<real_t>(UKF_Q_STD) : 0.0;
    }
  }

  qr_R_upper<MROWS, UKF_N>(At, S1);

  real_t r0[UKF_N];
  for (int i = 0; i < UKF_N; ++i) {
    r0[i] = residual[i][0];
  }
  cholupdate_upper<UKF_N>(S1, r0, (Wc[0] >= 0));
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

  real_t residual[UKF_M][UKF_SIGMA_POINTS];
  for (int k = 0; k < UKF_SIGMA_POINTS; ++k) {
    const real_t sw = sqrtAbsWc[k];
    for (int i = 0; i < UKF_M; ++i) {
      const real_t d = Z1[i][k] - z1[i];
      Z2[i][k] = d;
      residual[i][k] = d * sw;
    }
  }

  constexpr int MROWS = (UKF_SIGMA_POINTS - 1) + UKF_M;
  real_t At[MROWS][UKF_M];
  for (int c = 0; c < UKF_SIGMA_POINTS - 1; ++c) {
    for (int i = 0; i < UKF_M; ++i) {
      At[c][i] = residual[i][c + 1];
    }
  }
  for (int col = 0; col < UKF_M; ++col) {
    for (int i = 0; i < UKF_M; ++i) {
      At[(UKF_SIGMA_POINTS - 1) + col][i] = (i == col) ? static_cast<real_t>(UKF_R_STD) : 0.0;
    }
  }

  qr_R_upper<MROWS, UKF_M>(At, S2);

  real_t r0[UKF_M];
  for (int i = 0; i < UKF_M; ++i) {
    r0[i] = residual[i][0];
  }
  cholupdate_upper<UKF_M>(S2, r0, (Wc[0] >= 0));
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

    real_t X[UKF_N][UKF_SIGMA_POINTS];
    sigmas(x, S, coeff, X);

    real_t x1[UKF_N];
    real_t X1[UKF_N][UKF_SIGMA_POINTS];
    real_t S1[UKF_N][UKF_N];
    real_t X2[UKF_N][UKF_SIGMA_POINTS];
    ut_process(X, Wm, Wc, sqrtAbsWc, x1, X1, S1, X2);

    real_t z1[UKF_M];
    real_t Z1[UKF_M][UKF_SIGMA_POINTS];
    real_t S2[UKF_M][UKF_M];
    real_t Z2[UKF_M][UKF_SIGMA_POINTS];
    ut_meas(X1, Wm, Wc, sqrtAbsWc, z1, Z1, S2, Z2);

    real_t P12[UKF_N][UKF_M];
    for (int i = 0; i < UKF_N; ++i) {
      for (int j = 0; j < UKF_M; ++j) {
        real_t sum = 0.0;
        for (int k = 0; k < UKF_SIGMA_POINTS; ++k) {
          sum += X2[i][k] * Wc[k] * Z2[j][k];
        }
        P12[i][j] = sum;
      }
    }

    real_t K[UKF_N][UKF_M];
    for (int i = 0; i < UKF_N; ++i) {
      for (int j = 0; j < UKF_M; ++j) {
        K[i][j] = P12[i][j];
      }
    }

    right_solve_upper_2(P12, S2, K);

    real_t S2t[UKF_M][UKF_M];
    for (int i = 0; i < UKF_M; ++i) {
      for (int j = 0; j < UKF_M; ++j) {
        S2t[i][j] = S2[j][i];
      }
    }
    real_t K2[UKF_N][UKF_M];
    right_solve_lower_2(K, S2t, K2);
    for (int i = 0; i < UKF_N; ++i) {
      for (int j = 0; j < UKF_M; ++j) {
        K[i][j] = K2[i][j];
      }
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
    for (int i = 0; i < UKF_N; ++i) {
      for (int j = 0; j < UKF_M; ++j) {
        real_t sum = 0.0;
        for (int k = 0; k < UKF_M; ++k) {
          sum += K[i][k] * S2[j][k];
        }
        U[i][j] = sum;
      }
    }

    for (int i = 0; i < UKF_N; ++i) {
      for (int j = 0; j < UKF_N; ++j) {
        S[i][j] = S1[i][j];
      }
    }
    for (int j = 0; j < UKF_M; ++j) {
      real_t uj[UKF_N];
      for (int i = 0; i < UKF_N; ++i) {
        uj[i] = U[i][j];
      }
      cholupdate_upper<UKF_N>(S, uj, false);
    }

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
