## Prompt1（阶段一）：多源输入 → 功能正确且可综合的 HLS C/C++

你是一名资深 FPGA / Vitis HLS 与状态估计工程师。任务：将 Matlab 的 SR-UKF 稳定落地为**功能正确、可综合、可复现**的 HLS C/C++并由 Testbench 验证。
冲突优先级：**接口/约束与验收标准 > 本 Prompt 的算法规格 > 我提供的多源材料（Matlab）> 你的默认常识**。

---

### 1) 目标（你必须完成）

生成 **Vitis HLS 的 SR-UKF 核**（同构 Source-A：sigma 点、UT、QR、cholupdate 的 SR 更新）。Testbench 以 `ukf_input.txt` 为唯一数据源，生成 `ukf_output.txt` 与 `ukf_state_compare.png`。kernel 只做计算，禁止文件 IO。

```cpp
int hls_SR_UKF(
  const float x0[UKF_N],                 // initial filter estimate
  const float z_seq[UKF_STEPS * UKF_M],  // measurement sequence z_k (flattened)
  float x_seq[UKF_STEPS * UKF_N],        // posterior x_k (flattened)
  float S_upper_seq[UKF_STEPS * 6]       // upper-tri of S_k per step: S11 S12 S13 S22 S23 S33
);
```

并满足：
- **可综合**：能被 Vitis HLS C Synthesis 通过（Vivado flow_target）。
- **可仿真**：CSIM/COSIM 可编译并运行。
- **可验证**：输出确定、可复现，满足验收标准（见第 5 节）。

约束：`UKF_N/UKF_M/UKF_STEPS` 为编译期常量；顶层核等价 Matlab 循环内 `[x,S]=ukf(...); xV(:,k)=x;`，并输出每步 `S` 上三角。

---

### 2) 设计约束（硬约束，违反即失败）

#### 2.1 工具链与风格约束
- 工具：Vitis HLS（Vivado 后端）。
- 工作目录固定：`/home/whp/Desktop/XLab/UKF`；交付 4 文件、CSIM/CSYNTH/COSIM 执行、以及 `ukf_output.txt/png` 产出均在此目录。
- kernel（`ukf_accel.cpp`）禁止：系统调用、文件 IO、线程/进程、网络、CUDA、Python 绑定。
- 禁止：`new/delete`、`malloc/free`、STL 容器、异常。
- 允许：`ap_int.h`（必要时 `ap_fixed.h`）。
- 循环上界必须为常量（如 `UKF_N/UKF_M/UKF_STEPS/(2*UKF_N+1)`）。

#### 2.2 数值与矩阵约束
- 必须同构 Source-A：`S` 为协方差平方根；UT 用 `qr(...,0)` 与 `cholupdate`；`Qs/Rs` 为噪声标准差矩阵并参与 UT 的 QR 拼接。
- 不允许把 SR-UKF 偷换成普通 UKF / EKF / PF。
- 计算必须确定性：不得引入运行期随机数。

---

### 3) 算法规格（必须与 Source-A 一致）

本节是对 Source-A（`ukf.m/ukf_main.m`）的结构化拆解；实现必须一致（允许定点量化带来的可解释误差）。

#### 3.1 系统模型（来自 `ukf_main.m`）

状态维度与量测维度：
- `UKF_N = 3`
- `UKF_M = 2`

非线性状态转移（`fstate`）：
- `f(x) = [ x2; x3; 0.05 * x1 * (x2 + x3) ]`

量测方程（`hmeas`）：
- `h(x) = x(1:UKF_M)`（即取前两维）

噪声设置（来自 `ukf_main.m`）：
- `q=r=0.1` `Qs=q*I(UKF_N)`，`Rs=r*I(UKF_M)`

初始条件（来自 `ukf_main.m`）：
- `s0=[0;0;1]`，`S0=I(UKF_N)`；`x0` 必须来自 `ukf_input.txt` 并作为 kernel 入参

#### 3.2 SR-UKF 主流程（来自 `ukf.m`，必须同构）

- `sigmas(x,S,c)`：`lambda=alpha^2*(L+ki)-L`，`c=L+lambda`，`A=sqrt(c)*S^T`，`X=[x, x+A, x-A]`
- `ut(f, X, Wm, Wc, n_out, Rs)`（过程/量测各一次）：`Y(:,k)=f(X(:,k))`，`y=sum Wm*Y`，`Y1=Y-y`，`residual=Y1*diag(sqrt(abs(Wc)))`，`S=R(qr([residual(:,2:end) Rs]^T,0))`，再按 `Wc(1)` 符号 `cholupdate(S,residual(:,1),+/-)`
- `P12 = X2 * diag(Wc) * Z2^T`
- `K = P12 / S2 / S2^T`：必须用三角求解（禁止显式求逆）
- `x = x1 + K * (z - z1)`
- `S` 更新：`U = K * S2^T`，对 `i=1..UKF_M`：`S = cholupdate(S, U(:,i), '-')`

#### 3.3 UKF 权重与缩放参数（来自 `ukf.m`）

必须使用与 Source-A 相同的默认参数（且为编译期常量）：
- `alpha = 1e-3`
- `ki = 0`
- `beta = 2`

权重同构：`c = UKF_N + lambda`，`Wm=[lambda/c, 0.5/c,...]`（长度 `2*UKF_N+1`），`Wc=Wm` 且 `Wc(1)+=(1-alpha^2+beta)`。

---

### 4) 源（实现仅参考）

```Matlab
/home/whp/Desktop/XLab/UKF_Experiment/UKF_Math/ukf.m
/home/whp/Desktop/XLab/UKF_Experiment/UKF_Math/ukf_main.m
```

---

### 5) 验收标准（你必须让输出“可比且可复现”）

输出必须满足：
- 构建与仿真：通过 Vitis HLS 的 CSIM 与 CSYNTH。
- 可复现性：相同 `ukf_input.txt` 多次运行，`ukf_output.txt` 与 `ukf_state_compare.png` 完全一致。
- 结构一致性：sigma 点数、UT/QR/cholupdate 更新路径同构 Source-A。
- 数值可解释性：若使用定点，偏差仅来源于量化/舍入；不得出现 NaN/Inf、未初始化值、维度错误或三角求解错误。
- 接口一致性：`ukf_output.txt` 的字段含义必须与 Matlab baseline 一致（见第 6 节）。

Testbench 必须执行 **errorcheck** 作为阶段一自动判定（见第 6.4 节）。

---

### 6) 你需要交付的内容（输出格式必须严格遵守）

- 这 4 个文件必须位于：`/home/whp/Desktop/XLab/UKF/`（工程根目录，文件名必须一致）。
- `ukf_config_params.h`：维度与常量参数（UKF_N/UKF_M/UKF_STEPS、alpha/beta/ki、q/r 等）
- `ukf_accel.cpp`：SR-UKF kernel（可综合，含全部辅助函数）
- `ukf_tb.cpp`：Testbench（文件 IO + 绘图 + errorcheck）
- `run_hls.tcl`：跑 CSIM/CSYNTH/COSIM；tcl的格式按 Xilinx 范式， T_exec也要迁移进来（Xilinx范式在：`/home/whp/Desktop/XLab/LucasKanade/build_linux/run_hls.tcl`）
#### 6.1 Testbench 输入接口（与 ukf_input.txt 严格一致）

Testbench 必须读取：`/home/whp/Desktop/XLab/UKF_Experiment/UKF_Math/ukf_input.txt`（不得在 TB 内随机生成）。解析：
- 忽略 `#` 行；解析 `N <int>` 且检查 `N==UKF_STEPS`
- 解析 `x0 <x0_1> <x0_2> <x0_3>` 作为 kernel 输入
- 读取 N 行：`k s1 s2 s3 z1 z2`，检查 k 连续；`s` 仅绘图；`z` 填 `z_seq`（按 k 再 m 展平）

#### 6.2 Testbench 输出接口（与 Matlab baseline 严格一致）

Testbench 必须在工作目录写出 `ukf_output.txt` 与 `ukf_state_compare.png`。`ukf_output.txt`：
- 允许 `#` 表头；必须包含 `N <int>` 与字段行：`k z1 z2 x1 x2 x3 S11 S12 S13 S22 S23 S33`
- 输出 N 行、每行 12 列同上；`z` 原样拷贝输入，`x/S` 来自 kernel
- 数字格式必须 `%.17g`。

#### 6.3 绘图逻辑（与 Matlab baseline 等价）

生成 `ukf_state_compare.png`：3 个 subplot，每幅画 `sV(i,:)` 实线与 `xV(i,:)` 虚线（k=1..N）。必须在无 GUI 的 CSIM 环境生成；允许 `popen` 调 gnuplot 输出 PNG（禁中间文件）。

#### 6.4 errorcheck（必须实现，且输出/阈值固定）

Testbench 在成功运行 kernel、写出 `ukf_output.txt/png` 后执行 errorcheck，并据此给出最终 PASS/FAIL。判定目标：输出数值健康，且在量测方程 `h(x)=x(1:2)` 下对量测拟合误差合理。

实现要求：
- 仅使用 `z_seq/x_seq/S_upper_seq` 计算；不得读其他文件。
- `is_finite_f(float v)` 使用 `std::isfinite`。
- 对 k=0..N-1：取 `z1/z2` 与 `x1/x2/x3`，任一非有限数→`101`；取 `S11/S22/S33`，任一非有限数或 `<=0`→`102`；令 `e1=x1-z1`,`e2=x2-z2`，累积 `se_z1+=e1^2`,`se_z2+=e2^2`，并更新 `maxae_z1/maxae_z2`
- 循环后：`rmse_z1=sqrt(se_z1/N)`, `rmse_z2=sqrt(se_z2/N)`
- 阈值固定：`RMSE_Z_THRESH=0.2`，`MAXAE_Z_THRESH=0.6`
- 打印固定（`std::setprecision(17)`）：
  - `errorcheck: rmse(x1-z1)=<rmse_z1> rmse(x2-z2)=<rmse_z2>`
  - `errorcheck: maxae(x1-z1)=<maxae_z1> maxae(x2-z2)=<maxae_z2>`
- 若任一 RMSE 或 MaxAE 超阈：返回 `103`；main 输出 `TEST FAIL: errorcheck rc=<rc>` 并返回非 0（用 `return 5`）
- 否则 main 输出 `TEST PASS` 并 `return 0`

额外要求：
- 保持顶层函数名与签名不变（除非你在第 1 节明确给出“最终签名”，且在所有文件中一致）。
- 不要创建额外文件。
- 如果需要新增辅助函数，必须放在 `ukf_accel.cpp` 内部，并且可综合。

后续操作：
- 代码生成之后，在 `/home/whp/Desktop/XLab/UKF` 下执行：`vitis_hls -f run_hls.tcl`，并通过终端信息验证此次实验是否成功
