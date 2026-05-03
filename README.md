# Prompt-Set Methodology: A Phased Approach for LLM-Assisted HLS Design

[**English**](./README.md) | [**中文**](./README_CN.md)

> **"Turning Chaos into Silicon"**  
> A multi-stage, human-on-the-loop design methodology that disciplines Large Language Models (LLMs) into professional High-Level Synthesis (HLS) hardware engineers.

![Overview](Overview.drawio.png)

This repository contains the official implementation, prompts, and benchmark datasets for the paper: **Prompt-Set Methodology: Bridging Semantic, Optimization, and Deployment Gaps in LLM-Assisted HLS**.

---

## 1. Motivation: Why Prompt-Set?

Directly prompting an LLM (e.g., ChatGPT) with *"Write a Kalman Filter accelerator for FPGA"* often leads to the following engineering failures:
*   ❌ **Hallucinated Logic**: The generated C++ code uses dynamic memory (`malloc`) or `std::vector`, immediately causing HLS compiler synthesis errors.
*   ❌ **Performance Bottlenecks**: The code simulates correctly but exhibits extremely high latency, failing to exploit FPGA spatial parallelism.
*   ❌ **Integration Nightmares**: The exported IP core has incompatible AXI interfaces, causing Vivado Block Design to fail or the board to crash during deployment.

The **Prompt-Set Methodology** is designed to solve these exact issues. Instead of treating the LLM as a "one-click generator," it decomposes the workflow into three distinct, mutually constrained virtual experts (Agents).

---

## 2. The 3-Stage Workflow

The methodology adheres to the principle of *"Correctness First, Performance Second, Integration Last"*. Please execute the stages sequentially. 

### 🟢 Stage 1: The Translator (Semantics & Interfaces)
> **Goal**: Translate algorithmic specifications (Python/Matlab/Math) into **synthesizable** and **semantically equivalent** HLS C++ code.

*   **Action**: Use the [Stage1_Translator.md](./Stage1_Translator.md) template.
*   **LLM's Task**:
    1.  Cleanse code (remove OS calls like `printf`, file I/O).
    2.  Enforce strict top-level function signatures (e.g., `void func(float A[N], ...)`).
    3.  Generate a Testbench for C-Simulation (CSIM) self-verification.
*   **Deliverable**: `func_correct.cpp` — A "Golden Model" that is functionally correct but unoptimized.

### 🟡 Stage 2: The Architect (Micro-Architectural Optimization)
> **Goal**: Perform **deep structural refactoring** guided by hardware bottlenecks, moving beyond simple Pragma injection.

*   **Action**:
    1.  Run HLS synthesis to obtain the initial timing/resource report.
    2.  Use the [Stage2_Architect.md](./Stage2_Architect.md) template.
    3.  **Crucial Step**: Force the LLM to select an optimization strategy from the **[S-Lib (Structural Optimization Library)](./S_Lib_Catalog.md)** (e.g., S1 Tiling, S5 Systolic Array).
*   **LLM's Task**:
    1.  Analyze the synthesis report (Compute Bound vs. Memory Bound).
    2.  Propose structured refactoring plans (Plan A / Plan B).
    3.  Rewrite loops, introduce Line Buffers, or implement dataflow pipelines.
*   **Deliverable**: `hls_optimized.cpp` — A high-performance, hardware-aware kernel.

### 🔵 Stage 3: The Integrator (System-Level Deployment)
> **Goal**: Deploy the optimized IP core onto the target board (e.g., PYNQ-Z2) and execute the closed-loop driver.

*   **Action**: Use the [Stage3_Integrator.md](./Stage3_Integrator.md) template.
*   **LLM's Task**:
    1.  Modify `run_hls.tcl` to export the RTL IP.
    2.  Generate `vivado_bd.tcl` for automated AXI interconnects, clocks, and interrupts.
    3.  Write a Python driver (`driver.py`) to load the Bitstream and verify on-board.
*   **Deliverable**: `overlay.bit` + `driver.py` — A fully deployable hardware acceleration system.

---

## 3. Quick Start: Step-by-Step Guide

Assume you are building a **Matrix Multiplication (GEMM)** accelerator:

### Step 1: Prepare the Specification
Prepare your algorithmic prototype (e.g., Python):
```python
def gemm(A, B):
    return A @ B
```

### Step 2: Launch Stage 1 (Translator)
*   Open `Stage1_Translator.md`.
*   Replace `{INPUT_DESCRIPTION}` with your Python code.
*   Set `{TOP_FUNCTION_SIGNATURE}` to `void gemm_accel(float A[N][N], float B[N][N], float C[N][N])`.
*   **Prompt the LLM**.
*   **Gate**: Run CSIM with the generated `gemm_tb.cpp`. It must `PASS`.

### Step 3: Launch Stage 2 (Architect)
*   Run Vitis HLS synthesis. You may find high latency due to memory bottlenecks.
*   Open `Stage2_Architect.md`.
*   In `{BOTTLENECK_DESCRIPTION}`, describe the issue (e.g., "Memory bandwidth insufficient, PE starvation").
*   Consult `S_Lib_Catalog.md` and prompt the LLM to apply **S1 (Tiling)** or **S5 (Systolic Array)**.
*   **Prompt the LLM**.
*   **Gate**: Re-run CSIM (must still pass) and check the synthesis report for latency reduction.

### Step 4: Launch Stage 3 (Integrator)
*   Open `Stage3_Integrator.md`.
*   Specify your target board (e.g., `PYNQ-Z2`).
*   **Prompt the LLM**.
*   **Gate**: Execute the generated Tcl scripts to create the Vivado block design and generate the Bitstream. Test it on the board using the Python driver.

---

## 4. Repository Structure

This repository includes the methodology documentation, Prompt templates, and comprehensive benchmark experiments evaluated in the paper:

*   **📂 [LK (Lucas-Kanade)](./LK)**: Hardware acceleration for Optical Flow.
    *   Demonstrates the end-to-end flow from Python prototype to FPGA deployment.
    *   Includes complete iteration logs for Stage 2 structural optimization.
*   **📂 [UKF (Unscented Kalman Filter)](./UKF)**: Square-Root UKF implementation.
    *   Targeting complex control algorithms.
    *   Includes 6 detailed iterations of micro-architectural refactoring.
*   **📂 [LLM-Sel (LLM Selection)](./LLM-Sel)**: LLM baseline comparisons.
    *   Benchmarks GPT-5.2 vs. Gemini 3.0 Pro on HLS generation stability.
*   **📚 [S_Lib_Catalog.md](./S_Lib_Catalog.md)**: **The core asset** — The Structural Optimization Library (JSON/Markdown format).
*   **📝 Templates**: `Stage1_Translator.md`, `Stage2_Architect.md`, `Stage3_Integrator.md`.

---

## 5. FAQ

**Q: Why does the LLM keep generating `malloc` in Stage 1?**
A: Because LLMs are biased towards software engineering. The Stage 1 Prompt Card contains **Hard Constraints** explicitly forbidding dynamic memory allocation. Ensure you enforce these constraints in your prompt.

**Q: What if the code fails C-Simulation after Stage 2 optimization?**
A: This is common when LLMs hallucinate complex hardware logic. Stage 2 relies on **Invariant Constraints** (must pass the original testbench). If it fails, feed the `csim.log` back to the LLM and command it to rollback and try Plan B.

**Q: Can I skip Stage 2 and go directly from Stage 1 to Stage 3?**
A: Yes, but you will deploy an unoptimized accelerator that might be slower than an ARM CPU. Stage 2 (Architect + S-Lib) is where the true "hardware performance" is generated.

---

*Designed for Hardware Engineers, by Hardware Engineers.*
