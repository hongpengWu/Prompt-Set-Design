# Prompt-Set Methodology: A Phased Approach for LLM-Assisted HLS Design

> **"Turning Chaos into Silicon"**  
> 一套将大模型（LLM）驯化为专业硬件工程师的分阶段设计范式。

---

## 1. 为什么你需要这套方法论？

如果你尝试过直接问 ChatGPT：“帮我写一个 FPGA 上的卡尔曼滤波加速器”，你可能会遇到以下崩溃场景：
*   ❌ **幻觉代码**：生成的 C++ 用了 `malloc` 或 `std::vector`，HLS 编译器直接报错。
*   ❌ **性能黑洞**：代码能跑，但延迟极高，完全没有利用 FPGA 的并行优势。
*   ❌ **集成噩梦**：导出的 IP 核接口不对，根本无法连入 Vivado Block Design，板级验证直接死机。

**Prompt-Set Methodology** 就是为了解决这些问题而生的。它不再把 LLM 当作一个“一键生成器”，而是把它拆解为三个**分工明确、互相制约**的虚拟专家。

---

## 2. 三阶段工作流 (The 3-Stage Workflow)

请按照顺序执行以下三个阶段。不要跳步，每一步的输出都是下一步的基石。

### 🟢 Stage 1: The Translator (功能翻译官)
> **目标**：把你的算法（Python/Matlab/公式）翻译成**绝对正确**且**符合 HLS 语法**的 C++ 代码。

*   **你的动作**：使用 [Stage1_Translator.md](./Stage1_Translator.md) 模板。
*   **LLM 的任务**：
    1.  清洗代码，去除所有系统调用（printf, file I/O）。
    2.  固定顶层函数接口（Signature）。
    3.  生成 Testbench 进行自我验证。
*   **交付物**：`func_correct.cpp` —— 这是一个跑得慢但算得对的“黄金模型”。

### 🟡 Stage 2: The Architect (结构架构师)
> **目标**：针对性能瓶颈，对代码进行**结构重构**（不是简单的加 Pragma！）。

*   **你的动作**：
    1.  运行 HLS 综合，拿到报告（Report）。
    2.  使用 [Stage2_Architect.md](./Stage2_Architect.md) 模板。
    3.  **关键一步**：强迫 LLM 从 [S-Lib 策略库](./S_Lib_Catalog.md) 中选择优化策略（如 S1 Tiling, S5 Systolic Array）。
*   **LLM 的任务**：
    1.  分析报告，指出是算得慢（Compute Bound）还是读得慢（Memory Bound）。
    2.  提出 Plan A / Plan B 两个改造方案。
    3.  实施代码重构（如引入 Line Buffer，改变循环顺序）。
*   **交付物**：`hls_optimized.cpp` —— 这是一个高性能的硬件内核。

### 🔵 Stage 3: The Integrator (系统集成者)
> **目标**：把 IP 核部署到板卡（如 PYNQ-Z2）上，并跑通 Python 驱动。

*   **你的动作**：使用 [Stage3_Integrator.md](./Stage3_Integrator.md) 模板。
*   **LLM 的任务**：
    1.  修改 `run_hls.tcl` 导出 IP。
    2.  编写 `vivado_bd.tcl` 自动连线（处理 AXI 接口、时钟、中断）。
    3.  编写 `driver.py`，在板子上加载 Bitstream 并验证结果。
*   **交付物**：`overlay.bit` + `driver.py` —— 这是一个完整的硬件加速系统。

---

## 3. 新手入门指南 (Step-by-Step Guide)

假设你要实现一个 **矩阵乘法 (GEMM)** 加速器，请按以下步骤操作：

### Step 1: 准备原材料
准备好你的 Python 原型代码：
```python
def gemm(A, B):
    return A @ B
```

### Step 2: 启动 Stage 1
*   打开 `Stage1_Translator.md`。
*   将 `{INPUT_DESCRIPTION}` 替换为你的 Python 代码。
*   将 `{TOP_FUNCTION_SIGNATURE}` 设定为 `void gemm_accel(float A[N][N], float B[N][N], float C[N][N])`。
*   **发送给 LLM**。
*   **验证**：在本地运行生成的 `gemm_tb.cpp`，确保 `PASS`。

### Step 3: 启动 Stage 2
*   运行 Vitis HLS，发现 Latency 很大，报告显示瓶颈在内存读取。
*   打开 `Stage2_Architect.md`。
*   在 `{BOTTLENECK_DESCRIPTION}` 中填入：“内存带宽不足，计算单元处于饥饿状态”。
*   参考 `S_Lib_Catalog.md`，提示 LLM 考虑 **S1 (Tiling)** 和 **S5 (Systolic Array)**。
*   **发送给 LLM**。
*   **验证**：再次综合，确认 Latency 降低，且 C Simulation 依然通过。

### Step 4: 启动 Stage 3
*   打开 `Stage3_Integrator.md`。
*   填入你的板卡型号（如 PYNQ-Z2）。
*   **发送给 LLM**。
*   **执行**：运行生成的 Tcl 脚本生成 Bitstream，拷贝到板子上运行 Python 驱动。

---

## 4. 常见问题 (FAQ)

**Q: 为什么 LLM 总是喜欢用 `malloc`？**
A: 因为它默认是在写软件。Stage 1 的模板中包含了 **硬约束 (Hard Constraints)**，明确禁止了动态内存分配。请务必保留这部分约束。

**Q: Stage 2 优化后结果不对了怎么办？**
A: 这很常见。Stage 2 强调 **Invariant Constraints (不变约束)**，即必须通过原有的 Testbench。如果失败，请将错误日志反馈给 LLM，并要求它回滚到上一个版本重新尝试 Plan B。

**Q: 我可以直接从 Stage 1 跳到 Stage 3 吗？**
A: 可以，但这通常意味着你部署了一个性能很差的加速器（可能比 CPU 还慢）。Stage 2 是产生“硬件价值”的核心环节。

---

## 5. 核心资产导航

*   📚 **[S_Lib_Catalog.md](./S_Lib_Catalog.md)**: **优化策略库**。这里汇集了 12 种硬件设计模式（如脉动阵列、乒乓操作），是解决性能问题的锦囊。
*   📝 **[Templates](./)**: 三个阶段的 Prompt 模板文件。

---

*Designed for Engineers, by Engineers.*
