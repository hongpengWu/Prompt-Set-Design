# Prompt-Set Methodology: 大模型辅助 HLS 设计的分阶段方法论

[**English**](./README.md) | [**中文**](./README_CN.md)

> **"Turning Chaos into Silicon"**  
> 一套将大模型（LLM）驯化为专业高级综合（HLS）硬件工程师的分阶段、人机协同（Human-on-the-Loop）设计范式。

![Overview](Overview.drawio.png)

本仓库包含了论文 **Prompt-Set Methodology: Bridging Semantic, Optimization, and Deployment Gaps in LLM-Assisted HLS** 的官方实现、Prompt 模板库以及基准测试数据集。

---

## 1. 研究动机：为什么需要 Prompt-Set？

如果你尝试过直接问 ChatGPT：“帮我写一个 FPGA 上的卡尔曼滤波加速器”，你大概率会遇到以下工程灾难：
*   ❌ **幻觉代码**：生成的 C++ 使用了动态内存（`malloc`）或 `std::vector`，HLS 编译器直接报错，无法综合。
*   ❌ **性能黑洞**：代码能通过仿真，但延迟极高，完全没有利用 FPGA 的空间并行优势。
*   ❌ **集成噩梦**：导出的 IP 核 AXI 接口不匹配，根本无法连入 Vivado Block Design，导致板级验证直接死机。

**Prompt-Set Methodology** 正是为了解决这些“系统工程差距”而生。它拒绝把 LLM 当作一个“端到端的一键生成器”，而是将其解耦为三个**分工明确、且带有硬性验收门控（Gates）**的虚拟专家（Agents）。

---

## 2. 三阶段工作流 (The 3-Stage Workflow)

本方法论遵循 *“正确性优先，性能其次，最后集成”* 的原则。请严格按顺序执行，每一步的输出都是下一步的基石。

### 🟢 Stage 1: The Translator (功能翻译官)
> **目标**：把算法规范（Python/Matlab/数学公式）翻译成**支持综合**且**语义等价**的 HLS C++ 代码。

*   **动作**：使用 [Stage1_Translator.md](./Stage1_Translator.md) 模板。
*   **LLM 的任务**：
    1.  清洗代码（去除 `printf`, file I/O 等系统调用）。
    2.  强制固定顶层函数接口（如 `void func(float A[N], ...)`）。
    3.  生成 Testbench 进行 C 仿真（CSIM）自我验证。
*   **交付物**：`func_correct.cpp` —— 这是一个跑得慢但绝对算得对的“黄金模型”。

### 🟡 Stage 2: The Architect (微架构架构师)
> **目标**：突破简单的 Pragma 插入，在硬件瓶颈引导下进行**深度的代码结构重构**。

*   **动作**：
    1.  运行 HLS 综合，获取初始的时序和资源报告。
    2.  使用 [Stage2_Architect.md](./Stage2_Architect.md) 模板。
    3.  **关键步骤**：强制 LLM 从 **[S-Lib (结构优化策略库)](./S_Lib_Catalog.md)** 中选择经过验证的重构模板（如 S1 Tiling 分块, S5 Systolic Array 脉动阵列）。
*   **LLM 的任务**：
    1.  分析综合报告（判断是计算受限还是访存受限）。
    2.  提出结构化重构方案（Plan A / Plan B）。
    3.  重写循环、引入行缓存（Line Buffer）或构建数据流（Dataflow）流水线。
*   **交付物**：`hls_optimized.cpp` —— 这是一个面向硬件的高性能内核。

### 🔵 Stage 3: The Integrator (系统集成者)
> **目标**：将优化后的 IP 核部署到目标板卡（如 PYNQ-Z2），并跑通软硬件闭环驱动。

*   **动作**：使用 [Stage3_Integrator.md](./Stage3_Integrator.md) 模板。
*   **LLM 的任务**：
    1.  修改 `run_hls.tcl` 导出 RTL IP。
    2.  生成 `vivado_bd.tcl` 自动处理 AXI 互联、时钟和中断。
    3.  编写 Python 驱动 (`driver.py`) 在板卡上加载 Bitstream 并验证结果。
*   **交付物**：`overlay.bit` + `driver.py` —— 这是一个可直接部署的完整硬件加速系统。

---

## 3. 快速上手指南 (Step-by-Step Guide)

假设你要实现一个 **矩阵乘法 (GEMM)** 加速器，操作如下：

### Step 1: 准备算法规范
准备好你的 Python 原型代码：
```python
def gemm(A, B):
    return A @ B
```

### Step 2: 启动 Stage 1 (Translator)
*   打开 `Stage1_Translator.md`。
*   将 `{INPUT_DESCRIPTION}` 替换为你的 Python 代码。
*   将 `{TOP_FUNCTION_SIGNATURE}` 设定为 `void gemm_accel(float A[N][N], float B[N][N], float C[N][N])`。
*   **发送给 LLM**。
*   **验收门控 (Gate)**：在本地运行生成的 `gemm_tb.cpp` 进行 CSIM，必须 `PASS`。

### Step 3: 启动 Stage 2 (Architect)
*   运行 Vitis HLS，发现 Latency 很大，报告显示瓶颈在内存读取。
*   打开 `Stage2_Architect.md`。
*   在 `{BOTTLENECK_DESCRIPTION}` 中填入：“内存带宽不足，计算单元处于饥饿状态”。
*   参考 `S_Lib_Catalog.md`，提示 LLM 应用 **S1 (Tiling)** 或 **S5 (Systolic Array)** 策略。
*   **发送给 LLM**。
*   **验收门控 (Gate)**：再次进行 CSIM（必须依然通过），并确认综合报告中的 Latency 大幅降低。

### Step 4: 启动 Stage 3 (Integrator)
*   打开 `Stage3_Integrator.md`。
*   填入你的目标板卡型号（如 `PYNQ-Z2`）。
*   **发送给 LLM**。
*   **验收门控 (Gate)**：执行生成的 Tcl 脚本生成 Vivado 工程和 Bitstream，并使用 Python 驱动在真机上测试通过。

---

## 4. 仓库目录结构

本仓库包含论文中评估的完整方法论文档、Prompt 模板和基准实验案例：

*   **📂 [LK (Lucas-Kanade)](./LK)**: 稠密光流算法的硬件加速案例。
    *   展示了从 Python 原型到 FPGA 部署的全流程。
    *   包含 Stage 2 结构优化的完整迭代日志。
*   **📂 [UKF (Unscented Kalman Filter)](./UKF)**: 平方根无迹卡尔曼滤波案例。
    *   针对复杂控制算法的系统级实现。
    *   包含 6 次深度的微架构重构迭代记录。
*   **📂 [LLM-Sel (LLM Selection)](./LLM-Sel)**: LLM 基准测试对比。
    *   对比了 GPT-5.2 与 Gemini 3.0 Pro 在 HLS 代码生成稳定性上的表现。
*   **📚 [S_Lib_Catalog.md](./S_Lib_Catalog.md)**: **核心资产** —— 结构优化策略库（提供 JSON 和 Markdown 格式）。
*   **📝 Templates**: `Stage1_Translator.md`, `Stage2_Architect.md`, `Stage3_Integrator.md`。

---

## 5. 常见问题 (FAQ)

**Q: 为什么 LLM 在 Stage 1 总是喜欢生成 `malloc`？**
A: 因为 LLM 默认遵循软件工程思维。Stage 1 的 Prompt Card 中包含了**硬约束 (Hard Constraints)**，明确禁止了动态内存分配，请在对话中严格向 LLM 强调这些约束。

**Q: 如果 Stage 2 优化后，C 仿真结果不对了怎么办？**
A: 这是 LLM 产生“硬件逻辑幻觉”时的常见现象。Stage 2 依赖**不变约束 (Invariant Constraints)**（即必须通过原始 Testbench）。如果失败，请将 `csim.log` 错误日志反馈给 LLM，并强制它回滚代码，尝试 Plan B。

**Q: 我可以直接从 Stage 1 跳到 Stage 3 吗？**
A: 可以，但这通常意味着你部署了一个性能极差的加速器（大概率比 ARM CPU 还要慢）。Stage 2（Architect 结合 S-Lib）才是真正挖掘“硬件性能价值”的核心环节。

---

*Designed for Hardware Engineers, by Hardware Engineers.*
