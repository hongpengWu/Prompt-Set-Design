# Prompt Template: Stage 3 - System Integrator

**Role**: FPGA SoC Integration Engineer
**Goal**: Automate the export, integration, and deployment of the HLS IP on a physical board (e.g., PYNQ-Z2).

---

## 1. 工程上下文 (Project Context)

*   **HLS IP**: `{{KERNEL_NAME}}` (Stage 2 Output)
*   **Target Board**: {{BOARD_NAME}} (e.g., PYNQ-Z2, ZCU104)
*   **Target Part**: {{PART_NAME}} (e.g., xc7z020clg400-1)
*   **HLS Script**: `run_hls.tcl`

---

## 2. 任务目标 (Objectives)

一次性完成从 IP 导出到板级 Python 驱动的全流程：

1.  **IP Export**: 修改 `run_hls.tcl` 以导出 `ip_catalog` 格式的 IP。
2.  **Vivado Automation**: 编写 `vivado_bd.tcl` 脚本，自动创建工程、Block Design (BD)、连接 AXI 接口、生成 Bitstream。
3.  **Python Driver**: 编写 `driver.py`，在 PYNQ 环境下加载 Overlay，分配 contiguous memory，配置寄存器并运行。

---

## 3. 硬约束 (Hard Constraints)

> **违反以下任何一条即视为失败**

1.  **接口协议 (Protocols)**:
    *   控制面: 必须连接到 PS 的 `M_AXI_GP0` (Zynq) 或相应 Master 端口。
    *   数据面: 必须连接到 PS 的 `S_AXI_HP*` (Zynq) 或 MIG (DDR)。
    *   时钟/复位: 必须正确处理同步复位。
2.  **自动化 (Automation)**: 禁止依赖 GUI 操作。所有步骤必须通过 Tcl 脚本或 Python 脚本实现。
3.  **鲁棒性 (Robustness)**: 脚本必须检查 IP 是否存在，文件路径是否正确，避免硬编码绝对路径。

---

## 4. 接口规范 (Interface Specification)

*   **S_AXI_LITE (Control)**: 偏移地址 (Offset) 必须与 HLS 导出的 `x{kernel}_hw.h` 一致。
    *   `0x00`: Control signals (start, done, idle)
    *   `0x10`+: Scalar arguments & Pointers (low/high 32-bit)
*   **M_AXI (Data)**: 必须具备正确的 `depth`, `offset=slave`, `bundle` 属性以匹配 Vivado 互联。

---

## 5. 你的输出 (Your Output)

### A. HLS Export Script (`run_hls.tcl` 片段)
```tcl
# ... export_design command ...
```

### B. Vivado BD Script (`vivado_bd.tcl`)
```tcl
# ... create_project, create_bd_design, ...
# ... connect_bd_intf_net ...
# ... make_wrapper, write_bitstream ...
```

### C. PYNQ Python Driver (`driver.py`)
```python
from pynq import Overlay, allocate
import numpy as np

# ... load overlay ...
# ... allocate buffer ...
# ... write registers ...
# ... run and verify ...
```

### D. 排障清单 (Troubleshooting)
*   列出 3-5 个可能的板级集成失败原因（如 Cache Coherency, AXI Width Mismatch）。
