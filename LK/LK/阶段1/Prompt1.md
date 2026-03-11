## Prompt1（阶段一）：多源输入 → 功能正确且可综合的 HLS C/C++ 
 
 你是一名资深 FPGA / Vitis HLS 工程师与计算机视觉算法工程师。你的任务不是写论文，而是把“多源描述的 Lucas–Kanade（LK）光流”稳定地落到**功能正确、可综合、可复现**的 HLS C/C++ 实现上，并能被既有 Testbench 直接验证。 
 
 请严格按以下优先级处理冲突：**接口/约束与验收标准 > 本 Prompt 的算法规格 > 我提供的多源材料（Python/Matlab/伪代码）> 你的默认常识**。 
 
 --- 
 
 ### 1) 目标（你必须完成） 
 
 生成一个用于 **Vitis HLS 的 Horn–Schunck（HS）光流核**（single-scale、非金字塔、dense per-pixel 输出），实现接口： 
 
 ```cpp 
 int hls_HS( 
   unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH], 
   unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH], 
   signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH], 
   signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH], 
   unsigned short int height, 
   unsigned short int width 
 ); 
 ``` 
 
 并满足： 
 - **可综合**：能被 Vitis HLS C Synthesis 通过（Vivado flow_target）。 
 - **可仿真**：能被 csim/cosim 的 C++ 编译器通过并运行。 
 - **可验证**：在同一测试输入上输出稳定，满足验收标准（见第 5 节）。 
 
 --- 
 
 ### 2) 设计约束（硬约束，违反即失败） 
 
 #### 2.1 工具链与风格约束 
 - 目标工具：Vitis HLS（Vivado 后端）。 
 - 禁止使用：OpenCV、任何系统调用、文件 IO、线程/进程、网络、CUDA、Python 绑定。 
 - 禁止使用：`new/delete`、`malloc/free`、STL 容器（`std::vector`/`std::string` 等）、异常。 
 - 允许使用：`ap_int.h`（`ap_int/ap_uint`），必要时可用 `ap_fixed.h`。 
 - 循环必须静态可界定（HLS 能够推断 tripcount 或以 `height/width` 为界且有 MAX 上界）。 
 
 --- 
 
 ### 3) 算法规格 
 
 实现原始 Horn–Schunck 光流方法（single-scale、非金字塔），核心步骤如下： 
 3.0 输入输出与Matlab实现保持基本相同，具体是输入两张tif文件 `/root/autodl-tmp/whp/Experiment/Math_baseline/yos9.tif`，`/root/autodl-tmp/whp/Experiment/Math_baseline/yos10.tif` ，输出为png文件在 `\root\autodl-tmp\whp\Experiment\M2HLS` 下，并且参数设置就是tif的分辨率，其余的迭代等各种参数就与Matlab实现保持一致吧 
 3.1 梯度计算 
 
 空间梯度： 
 
 使用固定 Sobel 算子计算 Ix, Iy 
 
 时间梯度： 
 
 It = I2 - I1 
 
 所有梯度结果需使用定点或整型表示（避免 float） 
 
 3.2 初始化 
 
 初始光流场： 
 
 对所有像素：u = 0, v = 0 
 
 3.3 迭代更新（固定次数） 
 
 采用 固定迭代次数 N_ITER（例如 10 / 20 / 50），禁止 early stop 
 
 每次迭代： 
 
 计算当前 u, v 的邻域平均： 
 
 使用 4 邻域（上、下、左、右） 平均 
 
 边界像素采用复制或零填充（需确定性） 
 
 对每像素按以下公式更新： 
 \[ 
 u^{k+1}=\bar{u}^{k}-\frac{I_x\left(I_x\bar{u}^{k}+I_y\bar{v}^{k}+I_t\right)}{\alpha^{2}+I_x^{2}+I_y^{2}} 
 \] 
 \[ 
 v^{k+1}=\bar{v}^{k}-\frac{I_y\left(I_x\bar{u}^{k}+I_y\bar{v}^{k}+I_t\right)}{\alpha^{2}+I_x^{2}+I_y^{2}} 
 \] 
 其中 \overline{u}, \overline{v} 是邻域平均。 
 
 输出稠密场 vx_img/vy_img 为每像素运动向量。 
 
 注意： 
 
 迭代次数 Niter 由工程约束指定（如 10/20/50）。 
 
 3.4 正则化参数 
 alpha 必须定义为 编译期常量宏 
 alpha 使用定点表示（位宽需说明） 
 不允许在运行期动态修改 
 3.5 输出 
 输出 vx_img / vy_img 为 稠密光流场 
 输出光流效果图，箭头大小和采样间隔以及其他参数均与MATLAB算法保持一致，便于对照，输出文件名为hls_out_flow_arrows
 采用 signed short int 定点格式（需明确缩放因子） 
 所有输出必须确定、无未初始化值 
 --- 
 
 ### 4) 源 
 
 我将提供以下“多源材料”，你可以参考它们的数学原理但必须以第 2/5 节为准： 
 
 #### Source-A：Matlab基准实现 
 ```Matlab 
     /root/autodl-tmp/whp/Experiment/Math_baseline 
 ``` 
 --- 
 
 ### 5) 验收标准（你必须让输出“可比且可复现”） 
 
 输出必须满足： 
 
 - 构建与仿真：通过 Vitis HLS 的 CSIM 与 CSYNTH。 
 - 数值正确性：与 Matlab HS 基准输出在合理误差范围内一致。 
 - 可复现性：相同输入多次运行逐像素结果完全一致。 
 - 误差可解释性：偏差仅来源于定点化/舍入，不得出现随机或结构性错误。 
 
 --- 
 
 ### 6) 你需要交付的内容（输出格式必须严格遵守） 
 
 请按照以下 4 个文件的完整内容，不要输出任何额外解释文字： 
 
 hs_config_params.h —— Horn–Schunck 参数定义 
 hs_accel.cpp —— HS 核心算法（HLS 可综合） 
 hs_tb.cpp —— Testbench（调用 hls_HS 并输出结果） 
 run_hls.tcl——tcl脚本执行CSIM，CSYNTH，COSIM 
 具体范式按照Xilinx官网为准 `\root\autodl-tmp\whp\Vitis-Libraries-LK\vision\L1\tests\lknpyroflow\lknpyroflow_NPPC1_8UC1_32FC1\run_hls.tcl` ，注意opencv_env与编译链的路径， `\root\autodl-tmp\whp\Vitis-Libraries-LK\vision\L1\tests\lknpyroflow\lknpyroflow_NPPC1_8UC1_32FC1\run_hls.tcl` 是目前可执行的，可做参考。我的服务器是25v CPU，请你充分利用CPU算力 
 输出路径： 
 ''' 
 /root/autodl-tmp/whp/Experiment/M2HLS 
 ''' 
 额外要求： 
 - 保持函数名与签名不变 
 - 不要创建额外文件。 
 - 如果你需要新增辅助函数，必须放在 `lk_accel.cpp` 内部，并且可综合。 
 后续操作: 
 - 代码生成之后，请你用vitis_hls run_hls.tcl指令运行，并通过终端信息验证此次实验是否成功