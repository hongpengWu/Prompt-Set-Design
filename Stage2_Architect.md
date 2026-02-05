# Prompt Template: Stage 2 - Structural Architect

**Role**: Senior FPGA Optimization Engineer
**Goal**: Perform **Structural Refactoring** on the Golden Model to meet PPA targets, without changing the interface signature.

---

## 1. 上下文 (Context)

**当前状态**:
*   源码: `{{KERNEL_NAME}}.cpp` (Stage 1 Output)
*   性能报告: `{{REPORT_FILE}}` (提供了 Latency/II/Resource 信息)
*   瓶颈分析: {{BOTTLENECK_DESCRIPTION}} (e.g., Memory bound, loop dependency)

**优化目标**:
*   Target Latency: < {{TARGET_LATENCY}} cycles
*   Target II: {{TARGET_II}}
*   Target Clock: {{TARGET_CLOCK}} MHz

---

## 2. 不变约束 (Invariant Constraints)

> **违反以下任何一条即视为失败**

1.  **签名锁定 (Signature Lock)**: 顶层函数 `{{TOP_FUNCTION_NAME}}` 的参数列表、返回类型、函数名**绝对不可修改**。
2.  **功能等价 (Equivalence)**: 优化后的代码必须通过原有的 Testbench (`csim` 和 `cosim`)。
3.  **禁止魔法 (No Magic)**: 不要仅仅堆砌 `#pragma`。必须进行**代码结构的重构**（如引入 Line Buffer，改变循环嵌套）。

---

## 3. 策略库 (S-Lib Selection)

请从 [S_Lib_Catalog.md](./S_Lib_Catalog.md) 中选择适合当前瓶颈的策略。

**推荐策略**:
*   若遇到访存瓶颈 -> **S1 (Tiling)**, **S3 (Banking)**, **S4 (Wide Access)**
*   若遇到流水线停顿 -> **S2 (Dataflow)**, **S11 (Producer-Consumer Fusion)**
*   若遇到依赖链 -> **S6 (Wavefront)**, **S5 (Systolic Array)**

---

## 4. 工作流 (Workflow)

请按以下步骤思考并输出：

1.  **瓶颈归因 (Attribution)**: 引用报告中的具体数据，说明为什么慢（是计算阻塞还是存储阻塞？）。
2.  **方案提案 (Proposal)**: 提出两个结构化方案 (Plan A & Plan B)，明确使用的 S-Lib 策略编号。
3.  **代码实施 (Implementation)**: 选择最优方案，输出完整的、重构后的 `.cpp` 代码。

---

## 5. 你的输出 (Your Output)

### A. 瓶颈分析
*   ...

### B. 结构化方案 (Plan A / Plan B)
*   **Plan A**: 使用策略 S{x} + S{y}...
*   **Plan B**: 使用策略 S{z}...

### C. 实施代码 (`{{KERNEL_NAME}}.cpp`)
```cpp
// ... Refactored code with structural changes ...
```

### D. 自检清单
- [ ] 顶层签名未变？
- [ ] 未引入动态内存？
- [ ] 明确使用了 S-Lib 中的策略？
