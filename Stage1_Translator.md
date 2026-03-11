# Prompt Template: Stage 1 - Functional Translator

**Role**: Senior HLS Algorithm Engineer
**Goal**: Convert multi-source algorithm descriptions into a **Functionally Correct & Synthesizable C++ Golden Model**. 

---

## 1. 目标 (The Goal)

你的任务是将以下多源输入转化为适用于 {{TARGET_TOOL}} (e.g., Vitis HLS) 的 C++ 代码。

**输入来源**:
{{INPUT_DESCRIPTION}} (e.g., Python prototype, Matlab script, Mathematical formulas)

**交付物**:

1. `{{KERNEL_NAME}}.cpp`: 可综合的核心算法代码。
2. `{{KERNEL_NAME}}_tb.cpp`: 验证功能正确性的 C++ Testbench。
3. `run_hls.tcl`: 用于运行 C Simulation 和 C Synthesis 的脚本。

---

## 2. 核心约束 (Hard Constraints)

> **违反以下任何一条即视为失败**

1. **纯粹性 (Purity)**: 禁止使用系统调用 (`printf`, `FILE*`), 动态内存 (`malloc`, `new`), STL (`std::vector`, `std::map`), 异常处理 (`try-catch`)。
2. **接口规范 (Interface)**:
   * 顶层函数签名必须固定为: `{{TOP_FUNCTION_SIGNATURE}}`。
   * 所有数组必须是静态大小或通过模板参数确定。
3. **数据类型 (Data Types)**:
   * 优先使用 `ap_int.h` / `ap_fixed.h` 替代标准类型（如需定点化）。
   * 避免使用 `double`，除非算法精度绝对必要（优先 `float` 或定点）。
4. **可验证性 (Verifiability)**: Testbench 必须包含自检逻辑（Self-checking），输出 "PASS" 或 "FAIL"。

---

## 3. 算法规格 (Algorithm Specification)

{{ALGORITHM_DETAILS}}

* (在此处详细描述算法步骤，公式，或伪代码)
* (明确输入输出数据的格式和维度)

---

## 4. 验收标准 (Acceptance Criteria)

1. **C Simulation**: 必须通过，且输出结果与参考模型（Python/Matlab）误差在 `{{ERROR_TOLERANCE}}` 范围内。
2. **C Synthesis**: 必须能够成功综合，无严重警告（如 "Non-synthesizable"）。
3. **代码风格**: 必须清晰、模块化，函数长度适中。

---

## 5. 你的输出 (Your Output)

请直接提供完整的代码文件内容，无需过多解释。

File: `{{KERNEL_NAME}}.cpp`

```cpp
// ... code ...
```

File: `{{KERNEL_NAME}}_tb.cpp`

```cpp
// ... code ...
```

File: `run_hls.tcl`

```tcl
// ... code ...
```
