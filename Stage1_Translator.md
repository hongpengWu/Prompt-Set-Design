# Prompt Template: Stage 1 - Functional Translator

**Role**: Senior FPGA / HLS Algorithm Engineer  
**Goal**: 将多源算法描述稳定转化为 **功能正确、可综合、可复现、可验证** 的 HLS C/C++ 基线工程。  
**适用场景**: 适合 UKF、LK/HS 光流、滤波、矩阵计算、图像处理、控制算法等“先保功能正确，再进入优化与部署”的第一阶段任务。

---

## 0. 使用说明 (How to Adapt)

将下面所有 `{{...}}` 占位符替换为你的具体项目内容；不适用的小节可以删除，但建议保留以下骨架：

1. 明确 **输入来源**
2. 锁定 **顶层函数签名**
3. 写清 **算法规格**
4. 规定 **Testbench 输入/输出与自检**
5. 规定 **run_hls.tcl 的执行目标**

如项目有特殊要求，建议优先补齐这些字段：

- `{{TARGET_TOOL}}`
- `{{TOP_FUNCTION_SIGNATURE}}`
- `{{SOURCE_MATERIALS}}`
- `{{ALGORITHM_DETAILS}}`
- `{{ERROR_TOLERANCE}}`
- `{{PROJECT_ROOT}}`

---

## 1. 角色与冲突优先级 (Role & Priority)

你是一名资深 {{DOMAIN_EXPERT_ROLE}}。你的任务不是写论文，而是把“多源描述的 {{ALGORITHM_NAME}}”稳定落地为 **{{TARGET_TOOL}} 可接受的 HLS C/C++ 基线实现**，并能被既有或新建 Testbench 直接验证。

当不同信息冲突时，请严格遵守以下优先级：

**硬约束与验收标准 > 本 Prompt 的算法规格 > 我提供的源材料 > 你的默认常识**

---

## 2. 任务目标 (Objectives)

请生成一个面向 {{TARGET_TOOL}} 的可综合工程，至少包含以下交付物：

1. `{{KERNEL_FILE}}`：核心算法代码，可综合
2. `{{TB_FILE}}`：自检式 Testbench，可运行并输出 PASS/FAIL
3. `{{HLS_SCRIPT_FILE}}`：运行 `csim` / `csynth` / `cosim` 的 Tcl 脚本

如果项目确实需要，也可以额外交付：

- `{{CONFIG_HEADER_FILE}}`：常量、维度、数据类型、量化配置
- `{{OPTIONAL_OUTPUT_FILES}}`：如参考输出、图像、日志、统计摘要

最终实现必须同时满足：

- **可综合**：能够通过 {{TARGET_TOOL}} 的 C Synthesis
- **可仿真**：C Simulation 可运行
- **可复现**：同一输入多次运行结果一致
- **可验证**：Testbench 能自动判定 PASS/FAIL

---

## 3. 工程上下文 (Project Context)

### 3.1 输入来源

本阶段的算法信息来自以下多源材料：

{{SOURCE_MATERIALS}}

示例：

- Python/Matlab 原型
- 数学公式或论文伪代码
- 参考 C/C++
- 图像/传感器/轨迹数据格式说明

### 3.2 目标工程

- 工作目录：`{{PROJECT_ROOT}}`
- 目标工具：`{{TARGET_TOOL}}`
- 目标器件/平台：`{{TARGET_PART_OR_PLATFORM}}`
- 顶层核文件：`{{KERNEL_FILE}}`
- 测试文件：`{{TB_FILE}}`
- HLS 脚本：`{{HLS_SCRIPT_FILE}}`

### 3.3 顶层接口

顶层函数签名必须固定为：

```cpp
{{TOP_FUNCTION_SIGNATURE}}
```

如果存在返回值、标量参数、数组参数、图像缓冲区、状态向量或矩阵，请在这里补充其语义：

{{INTERFACE_SEMANTICS}}

---

## 4. 硬约束 (Hard Constraints)

> 违反以下任意一条即视为失败

### 4.1 Kernel 可综合约束

- 禁止系统调用、线程/进程、网络、随机数、未定义行为
- 禁止动态内存：`new/delete`、`malloc/free`
- 禁止异常
- 默认禁止 STL 容器与复杂运行时特性；若工具链明确支持且你允许，需单独声明
- 循环边界必须静态可界定，或具备明确的最大上界 `{{STATIC_BOUND_MACROS}}`
- 所有辅助函数必须位于可综合调用链内，并保持 HLS 可接受

### 4.2 接口约束

- 顶层函数名、参数列表、返回类型 **不得更改**
- 输入输出语义与内存布局 **不得更改**
- 所有数组必须为静态大小、宏常量大小，或模板可推断大小
- 若使用 AXI/BRAM/stream pragma，必须与后续集成需求一致

### 4.3 数据类型约束

- 优先使用 `{{PREFERRED_NUMERIC_TYPES}}`
- 避免 `double`，除非算法精度绝对必要
- 若采用定点化，必须明确：
  - 位宽
  - 整数位
  - 缩放因子/舍入策略
  - 误差来源解释

### 4.4 Testbench 约束

- Kernel 内禁止文件 IO
- 文件读写、绘图、对比逻辑应放在 Testbench
- Testbench 必须包含自检逻辑，并输出固定 PASS/FAIL 结论
- Testbench 不得改变算法语义，只负责驱动、记录与验收

---

## 5. 算法规格 (Algorithm Specification)

请严格按以下规格实现，不得擅自替换为“看起来更简单”的其他算法。

### 5.1 问题定义

- 算法名称：`{{ALGORITHM_NAME}}`
- 输入定义：`{{INPUT_SPEC}}`
- 输出定义：`{{OUTPUT_SPEC}}`
- 维度/常量：`{{SHAPE_AND_CONSTANTS}}`

### 5.2 参考行为

{{REFERENCE_BEHAVIOR}}

示例写法：

- 状态转移方程 / 观测方程
- 梯度计算 / 卷积模板
- 迭代更新公式
- 矩阵分解 / 求解流程
- 边界处理策略

### 5.3 结构化步骤

请按步骤描述核心流程：

1. `{{STEP_1_NAME}}`: {{STEP_1_DETAILS}}
2. `{{STEP_2_NAME}}`: {{STEP_2_DETAILS}}
3. `{{STEP_3_NAME}}`: {{STEP_3_DETAILS}}
4. `{{STEP_4_NAME}}`: {{STEP_4_DETAILS}}

### 5.4 数值与稳定性要求

- 初始值：`{{INITIAL_CONDITIONS}}`
- 固定参数：`{{FIXED_PARAMETERS}}`
- 禁止行为：`{{FORBIDDEN_ALGORITHMIC_SHORTCUTS}}`
- 误差容忍：`{{ERROR_TOLERANCE}}`

---

## 6. Testbench 与输入输出契约 (Verification Contract)

### 6.1 输入来源

Testbench 必须从以下来源读取输入，而不是在 TB 内随意随机生成：

`{{TB_INPUT_SOURCE}}`

若输入文件有格式要求，请明确写出：

{{TB_INPUT_FORMAT}}

### 6.2 输出产物

Testbench 必须产出：

- `{{TB_PRIMARY_OUTPUT}}`
- `{{TB_SECONDARY_OUTPUT}}`
- `{{TB_OPTIONAL_PLOT_OR_REPORT}}`

如果输出文件格式需与参考实现严格一致，请写清字段、顺序、精度和单位：

{{TB_OUTPUT_FORMAT}}

### 6.3 自检逻辑

Testbench 必须执行如下自动判定：

{{TB_SELFCHECK_RULES}}

要求最终打印：

- 成功：`{{TB_PASS_MESSAGE}}`
- 失败：`{{TB_FAIL_MESSAGE}}`

---

## 7. HLS 脚本要求 (HLS Script Requirements)

`{{HLS_SCRIPT_FILE}}` 必须做到：

- 创建并打开 HLS 工程
- 指定顶层函数 `{{TOP_FUNCTION_NAME}}`
- 添加设计文件与 Testbench 文件
- 设置目标器件/时钟
- 运行 `csim_design`
- 运行 `csynth_design`
- 如项目要求，运行 `cosim_design`

可选补充：

- 导出报告
- 汇总关键指标
- 控制工作目录，避免路径漂移

相关要求：

{{HLS_SCRIPT_SPECIAL_REQUIREMENTS}}

---

## 8. 验收标准 (Acceptance Criteria)

最终结果必须满足：

1. **CSIM 通过**：输出与参考模型在 `{{ERROR_TOLERANCE}}` 范围内一致
2. **CSYNTH 通过**：无非综合代码、无严重接口错误
3. **可复现**：相同输入多次运行结果稳定一致
4. **误差可解释**：若使用定点或近似，偏差只能来自已声明的量化/舍入
5. **工程可读**：模块划分清晰，辅助函数职责明确

若你额外要求 `cosim`，则必须补充：

6. **COSIM 通过**：`{{COSIM_ACCEPTANCE_RULE}}`

---

## 9. 输出格式 (Your Output)

请只输出完整文件内容，避免空泛解释；如某文件不需要，可明确写 `UNCHANGED` 或在任务中删除该项。

File: `{{CONFIG_HEADER_FILE}}`

```cpp
// ...
```

File: `{{KERNEL_FILE}}`

```cpp
// ...
```

File: `{{TB_FILE}}`

```cpp
// ...
```

File: `{{HLS_SCRIPT_FILE}}`

```tcl
# ...
```

---

## 10. 自检清单 (Self-Check)

- [ ] 顶层函数签名是否完全未改？
- [ ] Kernel 是否未使用动态内存、异常、非综合特性？
- [ ] 算法是否与参考规格保持一致，而不是被替换成其他方法？
- [ ] Testbench 是否自检并输出固定 PASS/FAIL？
- [ ] `run_hls.tcl` 是否覆盖了至少 `csim` 与 `csynth`？
- [ ] 所有路径、常量、输入输出格式是否足够让读者直接改编？
