# Prompt Template: Stage 3 - System Integrator

**Role**: FPGA SoC Integration Engineer  
**Goal**: 将 Stage 2 产出的 HLS 核自动化打通到 **IP 导出 → Vivado Block Design → Bitstream → 板端驱动/验证** 的完整部署闭环。  
**适用场景**: 适用于 PYNQ-Z2、ZCU104、Zynq / Zynq MPSoC、或其他需要 PS-PL 协同的板级集成任务。

---

## 0. 使用说明 (How to Adapt)

本模板可覆盖两类常见集成模式：

1. **AXI4-Lite 寄存器型 IP**：控制与数据都走寄存器
2. **AXI4-Lite + AXI Master 型 IP**：控制走寄存器，数据走 DDR / CMA buffer

改编时优先替换这些字段：

- `{{BOARD_NAME}}`
- `{{BOARD_PART}}`
- `{{HLS_TOP_NAME}}`
- `{{INTERFACE_PROFILE}}`
- `{{REGISTER_MAP_OR_POINTER_RULES}}`
- `{{DEPLOYMENT_ARTIFACTS}}`

---

## 1. 角色与冲突优先级 (Role & Priority)

你是一名资深 {{INTEGRATION_ROLE}}。你的任务不是泛泛描述“怎么做”，而是把现有 HLS 工程做成 **可批处理复现、可连接、可部署、可由 PS 端驱动** 的板级交付物。

请严格按以下优先级处理冲突：

**硬约束与验收标准 > 本 Prompt 的集成目标与接口规范 > 我提供的工程上下文与已有脚本 > 你的默认常识**

---

## 2. 工程上下文 (Project Context)

### 2.1 HLS 工程

- HLS 工程根目录：`{{PROJECT_ROOT}}`
- 顶层源码：`{{KERNEL_CPP}}`
- 参数/类型定义：`{{CONFIG_HEADER}}`
- HLS 脚本：`{{RUN_HLS_TCL}}`
- 当前 HLS 顶层：`{{HLS_TOP_NAME}}`
- 当前目标器件：`{{HLS_TARGET_PART}}`

### 2.2 板级目标

- 目标板卡：`{{BOARD_NAME}}`
- Vivado 工程器件：`{{BOARD_PART}}`
- 运行环境：`{{RUNTIME_ENV}}`
- 输出 Overlay 产物：`{{DEPLOYMENT_ARTIFACTS}}`

### 2.3 PS 侧上下文

{{PS_SIDE_CONTEXT}}

示例：

- 已有 Python 驱动，要求 PL 对齐其寄存器偏移
- 已有检测/跟踪软件，只需提供 Overlay
- 需要新写最小驱动脚本做 sanity check

---

## 3. 阶段目标 (Objectives)

请一次性完成以下任务：

1. **顶层端口规范化**：检查并修正 `{{KERNEL_CPP}}` 中的接口 pragma，使其可导出、可在 BD 中连接
2. **HLS IP 导出**：修改或补齐 `{{RUN_HLS_TCL}}`，导出 `ip_catalog` 格式 IP
3. **Vivado 自动化**：生成 `vivado_bd.tcl`，自动创建工程、BD、连线、地址分配、生成 bitstream
4. **板端交付**：给出 `.bit`、`.hwh`、必要时 `.xsa` 的生成与提取方式
5. **PS 端验证**：根据项目需要，生成最小 Python 驱动，或输出一份可直接对接的接口技术说明

---

## 4. 硬约束 (Hard Constraints)

> 违反以下任意一条即视为失败

### 4.1 算法与语义

- 不得改变核心算法语义
- 不得随意更改数值类型，除非项目明确允许
- 不得改变输入输出语义与内存布局

### 4.2 自动化

- 禁止依赖 GUI 手工操作
- 所有关键步骤必须通过 Tcl 或 Python 自动化完成
- 路径处理必须鲁棒，避免硬编码不可移植绝对路径

### 4.3 接口一致性

- 控制面必须通过 `S_AXI_LITE` 或项目指定控制接口暴露
- 若存在大数组/图像/向量，数据面必须按项目选择：
  - `{{INTERFACE_PROFILE}} = register_only`
  - `{{INTERFACE_PROFILE}} = axi_master`
- 偏移地址、指针语义、位宽、编码方式必须与 HLS 导出或 PS 端约定一致

### 4.4 时钟与复位

- 必须正确连接 PS 时钟、互联时钟、IP 时钟
- 必须使用统一且合法的复位同步方案
- 不允许留下未连接或时钟域不一致的 AXI 端口

---

## 5. 接口规范模板 (Interface Specification)

请根据项目选择并填写以下一种或两种模式。

### 5.1 控制面：AXI4-Lite

默认要求：

- `0x00`：控制寄存器（`ap_start/ap_done/ap_idle/ap_ready`）
- `0x10+`：标量参数、指针低高位、或寄存器化数据

若偏移必须与既有驱动一致，请显式列出：

{{REGISTER_MAP_OR_POINTER_RULES}}

### 5.2 数据面：寄存器模式

适用于小规模状态量、参数量、单步计算：

- 所有输入输出通过 32-bit 或 64-bit 寄存器传递
- 若传 `float`，需说明按 IEEE754 bit pattern 编码
- 若传矩阵，需说明 row-major / col-major 与地址步进

### 5.3 数据面：AXI Master 模式

适用于图像、长向量、大数组：

- 必须声明 `depth`
- 必须指定 `offset=slave`
- 建议给出 `bundle` 规划，如 `gmem0/gmem1/...`
- 必须说明数据由 PS DDR/CMA 提供

### 5.4 PS ↔ PL 调用顺序

请明确：

1. 先写哪些寄存器或指针
2. 何时启动
3. 如何轮询 done
4. 何时读回结果
5. 如何验证一次调用成功

---

## 6. HLS 导出要求 (HLS Export Requirements)

`{{RUN_HLS_TCL}}` 必须满足：

- 复用既有 HLS 脚本，不额外新建平行导出脚本
- 显式控制 `csim / csynth / cosim / export`
- 导出格式为 `ip_catalog`
- 支持通过变量控制导出级别

推荐变量：

```tcl
set VIVADO_SYN 1
set VIVADO_IMPL 1
```

导出逻辑需要说明：

- 使用何种 `export_design` 选项
- 导出产物位置
- Vivado 如何加入 IP repo
- 当导出关闭时，脚本仍应能完成基础验证流程

特殊要求：

{{RUN_HLS_EXPORT_RULES}}

---

## 7. Vivado BD 自动化要求 (Vivado Automation Requirements)

你必须提供完整 `vivado_bd.tcl`，至少覆盖：

1. 创建工程
2. 设置器件/板卡
3. 注册 HLS IP repo
4. 创建并打开 Block Design
5. 实例化 PS、互联、复位模块、HLS IP
6. 连接控制面 AXI
7. 连接数据面 AXI 或寄存器型接口
8. 连接时钟与复位
9. 自动分配地址
10. 生成 wrapper
11. 运行 synthesis / implementation / bitstream
12. 导出 `.bit`、`.hwh`、必要时 `.xsa`

脚本必须具备鲁棒性：

- 使用 `file normalize` / `file join`
- `ip_repo_paths -> update_ip_catalog -> get_ipdefs` 顺序固定
- 找不到 IP 时打印候选 VLNV 并退出
- 对 `*_aclk` / `*_aresetn` 做存在性检查后连接
- `validate_bd_design` 能通过

---

## 8. 板端驱动要求 (Board-Side Driver Requirements)

根据项目选择以下一种交付方式：

### 8.1 生成 Python 驱动

适用于尚无 PS 端驱动的项目。驱动需完成：

- 加载 Overlay
- 定位 IP 实例
- 分配 CMA buffer 或准备寄存器输入
- 写参数 / 指针 / 控制位
- 启动并轮询完成
- 读回输出并做最小 sanity check

### 8.2 仅输出接口技术说明

适用于 PS 端代码已经存在的项目。必须写清：

- 如何识别 IP
- 完整寄存器映射
- 一次调用的写读顺序
- 数据打包与字节序规则

---

## 9. 输出格式 (Your Output)

请按以下顺序输出，可直接复制使用。

### A. 端口与可导出性自检结论

- 说明当前 HLS 顶层是否可导出
- 说明接口类型是否与板级连接匹配
- 说明控制面、数据面、时钟复位是否完整

### B. `{{KERNEL_CPP}}`

```cpp
// 若需要修改顶层 pragma 或 wrapper，在这里给出完整文件
```

### C. `{{RUN_HLS_TCL}}` 最小改动片段

```tcl
# 只贴新增或修改片段，含上下文定位
```

并补充：

- 运行命令：`{{HLS_RUN_COMMAND}}`
- 导出格式：`ip_catalog`
- 导出产物路径：`{{IP_EXPORT_PATH}}`
- Vivado 加入 IP repo 方法：`{{IP_REPO_IMPORT_METHOD}}`

### D. `vivado_bd.tcl`

```tcl
# 完整 Tcl 文件
```

并补充：

- Vivado batch 命令：`{{VIVADO_BATCH_COMMAND}}`
- `.hwh` 提取命令：`{{HWH_EXTRACTION_COMMAND}}`

### E. 板端驱动或接口技术说明

若生成驱动：

```python
# driver.py
```

若不生成驱动，则输出：

- IP 识别方式
- 寄存器映射表
- 一次调用流程
- 数据编码规则

### F. 排障清单 (Troubleshooting)

列出 6–12 条最可能的失败原因，至少覆盖：

- AXI 时钟/复位不匹配
- 地址分配或寄存器偏移不一致
- IP repo 未正确注册
- `.bit/.hwh` 与驱动不匹配
- AXI Master 未连通 DDR / HP 口
- 数据打包、宽度、字节序错误

---

## 10. 验证方式 (Validation)

请面向以下验收闭环交付：

1. HLS 脚本可运行并导出 IP
2. Vivado Tcl 可批处理生成 bitstream
3. `.bit` 与 `.hwh` 可被板端识别
4. PS 端最小驱动或既有驱动能成功启动 IP
5. 一次最小样例运行可完成并得到合理输出

验证命令模板：

```bash
{{HLS_RUN_COMMAND}}
{{VIVADO_BATCH_COMMAND}}
{{BOARD_SIDE_RUN_COMMAND}}
```

---

## 11. 自检清单 (Self-Check)

- [ ] 是否未改变核心算法语义？
- [ ] 是否所有关键步骤都可通过脚本自动化执行？
- [ ] 是否明确了寄存器映射或 AXI Master 指针语义？
- [ ] 是否明确了时钟、复位、控制面、数据面的连接关系？
- [ ] 是否给出了 `.bit/.hwh/.xsa` 的稳定产物路径？
- [ ] 是否给出了板端驱动或足够完整的接口技术说明？
