## Prompt3（阶段三）：HLS UKF IP → Vivado Block Design → PYNQ-Z2（PS 端 YOLO + PL 端 UKF 加速）

你是一名 FPGA SoC 集成工程师。在**不改 UKF 算法语义**（更新逻辑不改、数值类型仍为 float）的前提下，把现有 UKF HLS 工程做成可部署闭环：**PS 端 YOLOv3 输出观测，PL 端 UKF 滤波更新加速**。PS 端的软件流程/调用代码已给定，你只需要保证 PL 端导出的 overlay 与其接口完全一致（包括寄存器偏移与数据表示）。

冲突优先级：**硬约束与验收标准 > 集成目标与接口规范 > 工程上下文 > 默认常识**。

---

### 0) 工程上下文（以文件为准）

<project_context>
- HLS 工程根目录：`/home/whp/Desktop/XLab/UKF_Experiment/UKF2HLS`
- HLS 核心源码（需要你检查/修改端口 pragma / 顶层 wrapper）：`/home/whp/Desktop/XLab/UKF_Experiment/UKF2HLS/ukf_accel.cpp`
- 参数与常量：`/home/whp/Desktop/XLab/UKF_Experiment/UKF2HLS/ukf_config_params.h`
- HLS 脚本（CSIM/CSYNTH/COSIM/IP 导出）：`/home/whp/Desktop/XLab/UKF_Experiment/UKF2HLS/run_hls.tcl`
- 目标器件（HLS 以 run_hls.tcl 为准）：`xc7z020-clg484-1`
- UKF 维度常量以 `ukf_config_params.h` 为准；但最终必须与第 3.2 节寄存器接口一致（状态维度 4、观测维度 2、协方差 4×4）
</project_context>

板端：PYNQ-Z2（overlay 产物 `.bit + .hwh`；PS 端通过 AXI4-Lite MMIO 写/读寄存器驱动 UKF IP）。

---

### 1) 集成目标（你必须完成）

完成以下 4 件事（缺一不可）。所有脚本与复现命令放到：`/home/whp/Desktop/XLab/UKF_Experiment/UKF2HLS/vivado_bd`

1) **顶层端口规范化**：在 `ukf_accel.cpp` 新增硬件 wrapper 顶层，仅使用 AXI4-Lite（控制 + 数据均走寄存器），可综合。  
2) **run_hls.tcl 导出 IP**：用 `VIVADO_SYN/VIVADO_IMPL` 控制导出，补齐 `export_design -format ip_catalog`，不破坏 csim/csynth/cosim。  
3) **vivado_bd.tcl**：自动建工程/BD、导入 IP、连线/地址分配、生成 bitstream、导出 PYNQ 产物。  
4) **Vivado batch**：跑 `vivado_bd.tcl` 拿到 `.bit` 与 `.hwh`（含从导出包提取）。  

---

### 2) 硬约束（违反即失败）

#### 2.1 不可更改项
- **不得改变**UKF 核心计算语义：滤波更新逻辑保持一致；数值类型仍为 `float`（不引入定点）。
- **不得改变**PS↔PL 接口约定：全部参数通过 AXI4-Lite 寄存器传递；每个 `float` 按 IEEE754 的 32-bit bit pattern 通过 32-bit 寄存器写入/读出；寄存器偏移必须与第 3 节的地址映射一致。

#### 2.2 允许更改项（仅限为“可部署性 + 适配 YOLO 观测”服务）
- 允许：补全/调整顶层 `#pragma HLS INTERFACE ...`（`bundle/depth/offset/max_widen_bitwidth` 等）。
- 允许：增加“硬件 wrapper 顶层函数”以适配 PS 调用与实时性（同时保留原 `hls_SR_UKF` 作为软件回归/对照入口）。
- 允许：在 `run_hls.tcl` 新增/完善 IP 导出（ip_catalog），由 `VIVADO_SYN/VIVADO_IMPL` 控制且不破坏 csim/csynth/cosim。
- 允许：用 `vivado_bd.tcl` 生成 BD/bitstream/硬件导出。

---

### 3) PS（YOLO）↔ PL（UKF）数据流与接口规范（你必须落实到可用的 IP）

#### 3.1 观测模型（来自 YOLO 的 bbox → z）
PS 端 YOLOv3 推理输出目标 bbox（像素坐标）。对选定目标，生成 UKF 测量向量：
- `z[0] = bbox_cx`（中心 x）
- `z[1] = bbox_cy`（中心 y）

PL 侧 UKF 默认按“像素坐标系”工作，保持与现有 C 仿真一致。

#### 3.2 硬件顶层与寄存器接口（必须与既有 PS 调用一致）
PL 端导出的 HLS IP 顶层建议命名为 `ukf_accel_step`（用于在 `.hwh` 中形成稳定 VLNV/类型字段，便于 PS 端按 `overlay.ip_dict` 的 `type` 匹配到该 IP 实例）。顶层必须使用 AXI4-Lite（`s_axilite`，`ap_ctrl_hs`），且**所有输入/输出参数均映射为 32-bit 寄存器**（不使用 AXI Master，不走 DDR buffer 指针）。

数据表示：
- 所有 `float` 均通过 `uint32` 寄存器传递，位级等价于 `struct.pack("<f", x)` 的结果（小端）。
- `S` 矩阵采用 **row-major**（`S[i][j]` 紧密排列，地址增量按 `4*(i*4+j)` 计算）。

控制握手：
- 写 `AP_CTRL(0x00)` 的 bit0=1 启动一次更新。
- 轮询 `AP_CTRL(0x00)` 的 bit1（done）为 1 视为完成；PS 端不依赖中断。

寄存器地址映射（byte offset，32-bit word 对齐）：
- `0x00`：`AP_CTRL`（`ap_start/ap_done/ap_idle/ap_ready`）
- `0x10`：`z[0]`（float bits）
- `0x14`：`z[1]`（float bits）
- `0x18`：`q`（float bits）
- `0x20`：`r`（float bits）
- `0x30`：`x_in[0]`（float bits）
- `0x34`：`x_in[1]`（float bits）
- `0x38`：`x_in[2]`（float bits）
- `0x3C`：`x_in[3]`（float bits）
- `0x40..0x7C`：`S_in[4][4]`（16×float bits，row-major）
- `0x80`：`x_out[0]`（float bits）
- `0x84`：`x_out[1]`（float bits）
- `0x88`：`x_out[2]`（float bits）
- `0x8C`：`x_out[3]`（float bits）
- `0xC0..0xFC`：`S_out[4][4]`（16×float bits，row-major）

维度约定（由既有 PS 接口决定）：
- 状态维度为 4（`x_in/x_out` 均为 4 个 `float`）
- 观测维度为 2（`z` 为 2 个 `float`）
- 协方差为 4×4（`S_in/S_out` 为 16 个 `float`）

---

### 4) 你必须交付的内容（输出格式必须严格遵守）
按顺序输出（可直接复制使用）：

#### A) 端口与可导出性自检结论
- 用要点说明：IP 可导出、BD 可连接、PYNQ 可控（接口类型、bundle、depth/位宽/对齐假设）。
- 说明 PS 端 YOLO 输出 bbox 如何映射为 `z_in`。

#### B) `ukf_accel.cpp`
- 输出修改后的 `ukf_accel.cpp` 全文（不要输出 diff）。
- 要求：保留原 `hls_SR_UKF`（用于回归测试），新增 wrapper 顶层用于硬件部署，并通过 `set_top` 选择硬件顶层。

#### C) HLS 导出 IP 的可复现步骤
必须使用既有 `run_hls.tcl`（不得新建导出脚本），并且显式引用开关变量：
- `set VIVADO_SYN 1`
- `set VIVADO_IMPL 1`

在 `run_hls.tcl` 中新增/补齐导出逻辑（如 `export_design -format ip_catalog ...`），并满足：
- `VIVADO_IMPL` 控制实现级导出
- `VIVADO_SYN` 控制综合级导出
- 两者都为 0 时仍能运行 csim/csynth/cosim（并且不会误触发导出/实现）

输出必须给出：
- `run_hls.tcl` 的“最小改动片段”（只贴新增/修改行，含上下文定位）。
- 运行命令：`vitis_hls -f /home/whp/Desktop/XLab/UKF_Experiment/UKF2HLS/run_hls.tcl`
- 明确：导出格式（ip_catalog）、产物路径、Vivado 加入 IP repo 方法。

#### D) Vivado Block Design 集成与导出（Tcl）
你必须生成 `vivado_bd.tcl`（完整文件内容），并覆盖：
- 创建工程（器件/时钟）
- 加入 HLS IP（从第 C 节产物加入 IP Repo）
- BD 模块：Zynq PS、AXI 互联、proc_sys_reset、必要的时钟/复位
- 关键连线：控制面 GP0（AXI4-Lite），时钟复位
- Address Editor：控制寄存器映射解释清楚（基地址/范围/冲突规避）
- 生成 bitstream
- 导出“PYNQ 需要的产物”：`.bit` 与包含 `.hwh` 的硬件导出包（例如 `.xsa`）

你必须给出 Vivado batch 命令与从导出包提取 `.hwh` 的解压命令（`unzip -j ... "*.hwh" ...`）。

并在 `vivado_bd.tcl` 中满足鲁棒性要求：
- 路径：`file normalize/join`，工程目录固定在脚本目录下
- IP repo：`ip_repo_paths` → `update_ip_catalog` → `get_ipdefs`；找不到就 `exit 1` 并打印可用 VLNV
- PS：启用 `M_AXI_GP0`；`FCLK_CLK0` 覆盖 GP0/互联/HLS；`proc_sys_reset` 统一复位
- 控制面：`ps7/M_AXI_GP0 -> interconnect -> ukf_ip/s_axi_control`，`assign_bd_address` 自动分配
- 时钟复位：对互联/转换器的 `*_aclk/*_aresetn` 做“存在性检查后连接”，确保 `validate_bd_design` 通过

器件说明必须写清：
- HLS 目标器件以 `run_hls.tcl` 为准（`xc7z020-clg484-1`）
- Vivado BD 工程用 PYNQ-Z2 器件 `xc7z020clg400-1` 生成 bitstream；若报 part 不匹配，再回推重跑 HLS 以对齐器件

#### E) PS 端对接用的“接口技术说明”
不得编写或要求编写新的 PS 端 Python 脚本。你必须用文字/表格把以下内容写清楚，以便 PS 端既有代码可直接驱动：
- UKF IP 的识别方式：不得假设实例名；必须通过 `overlay.ip_dict` 的 `type` 字段按 VLNV/关键字匹配到包含 `ukf_accel_step` 的 IP。
- 第 3.2 节寄存器映射的完整复述（含偏移、含义、数据类型/编码方式）。
- 一次调用的寄存器写/读顺序（先写 q/r/z/x_in/S_in，再写 `AP_CTRL` 启动，再轮询 done，最后读回 x_out/S_out）。

#### F) 失败模式与排障清单（6–12 条）
必须包含以下类别：
- AXI 时钟/复位不匹配
- 寄存器偏移不一致（PS 端按固定 offset 写/读；PL 端必须提供同一映射；可用 `.hwh` 校验）
- 浮点编码/字节序不一致（float↔u32 打包解包错误）
- AP_CTRL 握手错误（start/done 语义、重复启动、未清 done）
- IP 实例定位错误（未按 VLNV/关键字遍历 `overlay.ip_dict`）
- YOLO bbox 坐标系/缩放不一致导致滤波发散

---

### 5) 规则与踩坑（必须遵守）
- 不得编造寄存器偏移/IP 名称：本 Prompt 给出的寄存器偏移是 PS 端既有接口的一部分；PL 端必须对齐；同时用 `.hwh`/overlay 元数据做交叉校验  
- BD 不得泛泛而谈：必须给出端口级连接关系  
- export_design 选项不得编造：Vitis HLS 2024.2 用 `-flow {none|syn|impl}`；禁止输出 `-vivado_syn/-vivado_impl`（必要时引用 `help export_design` 自证）  
- HLS Tcl 必须 `cd $SCRIPT_DIR`，避免产物路径漂移  
- AXI 时钟/复位必须全覆盖：互联/转换器的 `*_aclk/*_aresetn` 做存在性检查后连接  
- IP repo 注册顺序固定：`ip_repo_paths` → `update_ip_catalog` → `get_ipdefs`；失败要打印候选 VLNV 并退出  
- PS 端不得假设 IP 实例名：按 VLNV/关键字（如 `*ukf_accel_step*`）遍历 `overlay.ip_dict` 匹配  
- `.hwh` 提取必须可复现：给出从 `.xsa` 解压提取命令并校验文件存在  
- Notebook 兼容：不要依赖 `__file__`；`argparse` 用 `parse_known_args()`；Notebook 跑 shell 用 `!cmd`  
- 资源路径：`.bit/.hwh` 与脚本同目录；scp 目标写全到 `/home/xilinx/jupyter_notebooks/...`  
- 寄存器访问：`ip.register_map` 不可靠时必须提供 MMIO 偏移写的备选；偏移来源仍必须可自动解析（`.hwh`）

