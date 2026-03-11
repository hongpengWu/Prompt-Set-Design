#pragma once

#include <cmath>

using ukf_float = float;

constexpr int UKF_N = 3;
constexpr int UKF_M = 2;
constexpr int UKF_STEPS = 20;

constexpr int UKF_SIGMA_POINTS = 2 * UKF_N + 1;
constexpr int UKF_S_UPPER_ELEMS = 6;

constexpr ukf_float UKF_ALPHA = static_cast<ukf_float>(1e-3);
constexpr ukf_float UKF_KI = static_cast<ukf_float>(0.0);
constexpr ukf_float UKF_BETA = static_cast<ukf_float>(2.0);

constexpr ukf_float UKF_Q_STD = static_cast<ukf_float>(0.1);
constexpr ukf_float UKF_R_STD = static_cast<ukf_float>(0.1);
