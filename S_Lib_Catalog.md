# S-Lib: HLS Structural Optimization Strategy Catalog (High-Performance Edition)

本目录定义了通用的 HLS 结构优化策略库（Strategy Library）。这是 Prompt-Set 流程中的**核心资产**。在 **Stage 2 (Architect)** 中，LLM 必须从本库中选择策略来解决具体的性能瓶颈。

> **Design Philosophy**:  
> Pragma 只是结果，结构才是原因。不要试图用 `#pragma HLS PIPELINE` 掩盖糟糕的存储结构。
> 真正的优化来自：**Data Reuse (数据复用)**, **Parallelism (并行度)**, **Contention Avoidance (冲突避免)**.

---

## 一、 核心计算模式优化 (Core Computation Patterns)

### **S1: Tiling + Local Buffer (分块与局部缓存)**
*   **适用场景**: 全局内存（DDR/HBM）带宽受限，且存在数据复用（如卷积、矩阵乘、图像滤波）。
*   **原理**: 将大块数据分批搬运到片上 BRAM，在片上完成多次计算后，再写回。减少 `Access/Compute` 比率。
*   **Code Pattern**:
    ```cpp
    // Global Memory: in[N], out[N]
    // Local Buffer:  buf[TILE_SIZE]
    for (int i = 0; i < N; i += TILE_SIZE) {
        // 1. Load Tile
        for (int j = 0; j < TILE_SIZE; j++) buf[j] = in[i+j];
        
        // 2. Compute on Tile (High Reuse)
        process_tile(buf);
        
        // 3. Store Tile
        for (int j = 0; j < TILE_SIZE; j++) out[i+j] = buf[j];
    }
    ```
*   **关键 Pragma**: 
    *   `#pragma HLS ARRAY_PARTITION variable=buf cyclic/block factor=...` (匹配计算并行度)

### **S5: Systolic Array (脉动阵列)**
*   **适用场景**: 矩阵乘法 (GEMM)、2D 卷积、规则的 MAC 密集型运算。
*   **原理**: 构建 2D PE（Processing Element）阵列，数据在 PE 间仅做局部流动（Shift），消除全局广播网络，实现极高的频率和计算密度。
*   **Code Pattern**:
    ```cpp
    // 2D Array of PEs
    // A_local, B_local flowing through registers
    for (int k = 0; k < K; k++) {
        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                #pragma HLS PIPELINE II=1
                // Shift data: A flows East, B flows South
                val_A = (j == 0) ? in_A[i][k] : fifo_A[i][j-1].read();
                val_B = (i == 0) ? in_B[k][j] : fifo_B[i-1][j].read();
                
                // MAC
                C[i][j] += val_A * val_B;
                
                // Forward
                if (j < SIZE-1) fifo_A[i][j].write(val_A);
                if (i < SIZE-1) fifo_B[i][j].write(val_B);
            }
        }
    }
    ```
*   **Master Tip**: 使用 `hls::stream` 或 `ap_shift_reg` 实现 PE 间连接，避免长线路由。

### **S6: Wavefront / Hyperplane Scheduling (波前/超平面调度)**
*   **适用场景**: 动态规划 (DP)、Smith-Waterman、Seidel/Gauss-Seidel 迭代、存在数据依赖的网格计算。
*   **原理**: 当 `A[i][j]` 依赖于 `A[i-1][j]` 和 `A[i][j-1]` 时，普通的行列遍历无法流水。沿着“反对角线”（$i+j = const$）方向遍历，同对角线上的点无依赖，可并行。
*   **Code Pattern (Loop Skewing)**:
    ```cpp
    // Original: for i, for j ... dep on [i-1][j], [i][j-1]
    // Skewed:
    for (int sum = 0; sum < N + M; sum++) { // Wavefront index
        for (int i = 0; i < N; i++) {
            #pragma HLS PIPELINE II=1
            int j = sum - i; // Derive j
            if (j >= 0 && j < M) {
                // Now [i, j] and [i+1, j-1] are parallel safe
                update(buffer[i], buffer[i-1]); 
            }
        }
    }
    ```

### **S13: LUT / ROM Approximation (查表近似)**
*   **适用场景**: 复杂的超越函数（`exp`, `log`, `sigmoid`, `sin/cos`）位于内层循环，导致 DSP 消耗过高且 II 无法收敛。
*   **原理**: 对于精度要求不苛刻的场景，预先计算函数值存入 `static const` 数组（ROM），将复杂数学运算转换为一次内存读取。
*   **Code Pattern**:
    ```cpp
    // Precomputed LUT for sigmoid(x) where x in [-8, 8]
    static const ap_fixed<16,10> SIGMOID_LUT[1024] = { ... };

    ap_fixed<16,10> fast_sigmoid(ap_fixed<16,10> x) {
        // Map x to index
        int idx = (x + 8) * (1024 / 16);
        if (idx < 0) return 0;
        if (idx >= 1024) return 1;
        return SIGMOID_LUT[idx]; // Single cycle read
    }
    ```

### **S7: Custom Bitwidth & Fixed Point (自定义位宽与定点化)**
*   **适用场景**: 资源（BRAM/LUT/DSP）受限，或算法对精度不敏感（如神经网络推理）。
*   **原理**: 放弃标准 `float` 或 `int`，使用 Xilinx 任意精度数据类型 (`ap_fixed`, `ap_int`)。这能显著减少数据路径位宽，降低 BRAM 使用量，并允许在一个 DSP48 中打包执行多个乘法。
*   **Code Pattern**:
    ```cpp
    #include "ap_fixed.h"
    // Use 16-bit fixed point: 6 bits integer, 10 bits fractional
    typedef ap_fixed<16, 6> data_t; 
    
    void kernel(data_t* in, data_t* out) {
        // Operations are now resource-efficient
        data_t val = in[0] * in[1]; 
    }
    ```

---

## 二、 访存与带宽优化 (Memory & Bandwidth)

### **S3: Banking & Partitioning (存储体划分)**
*   **适用场景**: 循环展开（Unroll）导致端口冲突（II > 1）。
*   **原理**: 增加 BRAM 端口数量以匹配计算吞吐。
*   **Code Pattern**:
    ```cpp
    int buffer[1024];
    #pragma HLS ARRAY_PARTITION variable=buffer cyclic factor=4
    // Now we can access buffer[i], buffer[i+1], buffer[i+2], buffer[i+3] simultaneously
    for (int i = 0; i < 1024; i += 4) {
        #pragma HLS PIPELINE II=1
        // Vectorized compute
        process(buffer[i], buffer[i+1], buffer[i+2], buffer[i+3]);
    }
    ```

### **S4: Wide Load/Store (宽端口矢量化)**
*   **适用场景**: 算法吞吐受限于 DDR 带宽（Memory Bound），且数据访问连续。
*   **原理**: 将 `int` 或 `float` 指针重塑为 `ap_uint<512>`，单周期读写 512-bit 数据。
*   **Code Pattern**:
    ```cpp
    typedef ap_uint<512> vec_t;
    void kernel(vec_t* in, vec_t* out, int size) {
        #pragma HLS INTERFACE m_axi port=in bundle=gmem0 max_widen_bitwidth=512
        for (int i = 0; i < size / 16; i++) { // 512 bits = 16 * 32-bit ints
            vec_t chunk = in[i];
            // Manually unpack or cast
            for (int k = 0; k < 16; k++) {
                int val = chunk.range(k*32+31, k*32);
                // compute...
            }
        }
    }
    ```

### **S10: Temporal Blocking (时间复用分块)**
*   **适用场景**: 迭代型 Stencil (如热传导方程、流体模拟)，需要多步时间迭代 ($t, t+1, t+2...$)。
*   **原理**: 将多次时间步的计算“包”进同一片上工作集。不仅仅分块空间，还分块时间。
*   **Code Pattern**: 
    *   构建一个“金字塔”或“菱形”形状的片上 Buffer。
    *   在 Buffer 内连续执行 $t \to t+1 \to t+2$ 的更新，然后再去 DDR 读新数据。
    *   大幅提升 Arithmetic Intensity (Flops/Byte)。

---

## 三、 流水线与互连优化 (Pipeline & Interconnect)

### **S2: Dataflow Canonical Form (数据流标准型)**
*   **适用场景**: 复杂的端到端处理流程，包含多个异构阶段（如 Load -> FFT -> Filter -> Store）。
*   **原理**: 强制解耦。利用 Ping-Pong Buffer 或 FIFO 实现任务级流水（Task-Level Pipeline）。
*   **Code Pattern**:
    ```cpp
    void top_function(...) {
        #pragma HLS DATAFLOW
        hls::stream<data_t> s1, s2;
        
        load_module(in, s1);      // Producer
        compute_module(s1, s2);   // Consumer & Producer
        store_module(s2, out);    // Consumer
    }
    ```
*   **Risk**: 避免“非标准”的数据流（如 Bypassing, Feedback loop），这会破坏 Dataflow 的握手逻辑。

### **S11: Line Buffer & Sliding Window (行缓存与滑窗)**
*   **适用场景**: 图像处理卷积、Sobel 边缘检测（3x3, 5x5 窗口）。
*   **原理**: 利用行缓存（Line Buffer）保存前几行数据，避免重复从 DDR 读取像素。
*   **Code Pattern**:
    ```cpp
    hls::LineBuffer<2, WIDTH, pixel_t> line_buf;
    hls::Window<3, 3, pixel_t> window;
    
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            #pragma HLS PIPELINE II=1
            pixel_t in_pix = in_stream.read();
            
            // Shift Line Buffer & Window
            line_buf.shift_pixels_up(x);
            line_buf.insert_bottom_row(in_pix, x);
            window.shift_pixels_left();
            window.insert(in_pix, 0, 0); // Update window from line buffer
            
            // Compute on Window
            if (valid_region) out_stream.write( apply_filter(window) );
        }
    }
    ```

### **S12: Control-Flow Regularization (控制流规整化)**
*   **适用场景**: 稀疏数据处理、条件分支多的逻辑。
*   **原理**: 分支预测失败会清空流水线。将 `if-else` 转换为数据选择（Select/Mux）或掩码（Masking）。
*   **Code Pattern**:
    ```cpp
    // Bad (Control divergence)
    if (cond) res = a * b; else res = c + d;
    
    // Good (Predication)
    int val_true = a * b;
    int val_false = c + d;
    res = cond ? val_true : val_false; // HLS infers a Mux
    ```

### **S9: Loop Fusion (循环融合)**
*   **适用场景**: 多个循环顺序处理相同大小的数据，中间产生大量临时 BRAM 占用。
*   **原理**: 将相邻的 Producer 循环和 Consumer 循环合并，使数据在寄存器中直接传递（Locality），减少 BRAM 读写次数和循环控制开销。
*   **Code Pattern**:
    ```cpp
    // Before: 2 Loops, 1 Temp Buffer
    for (i=0..N) temp[i] = A[i] * 2;
    for (i=0..N) C[i] = temp[i] + B[i];
    
    // After: 1 Loop, Register Forwarding
    for (i=0..N) {
        int t = A[i] * 2; // Register
        C[i] = t + B[i];
    }
    ```

### **S15: Recursion to Iteration (递归转迭代)**
*   **适用场景**: 算法包含递归调用（DFS、树遍历、快速排序），HLS 不支持动态栈。
*   **原理**: 使用显式的 `stack` 数组模拟递归过程，将递归逻辑改写为 `while` 循环状态机。
*   **Code Pattern**:
    ```cpp
    // Explicit Stack
    int stack[1024]; 
    int top = 0;
    stack[top++] = root_node;
    
    while (top > 0) {
        int node = stack[--top];
        process(node);
        if (node->left) stack[top++] = node->left;
        if (node->right) stack[top++] = node->right;
    }
    ```

---

## 四、 软件工程模式映射 (Software Patterns to HLS)

| 软件模式 (SW Pattern) | HLS 映射 (HLS Mapping) | 目的 |
| :--- | :--- | :--- |
| **Data Oriented Design (DOD)** | **AoS to SoA (S8)** | 将 `struct {r,g,b}` 数组改为 `r[], g[], b[]` 独立数组，便于分别 Bank 化和并行访问。 |
| **Working Set Reduction** | **Tiling (S1)** | 确保工作集（Working Set）能完全装入片上 BRAM，避免 Cache Miss（在 FPGA 则是避免 DDR 访问）。 |
| **Producer-Consumer** | **Stream Fusion (S11)** | 消除中间临时数组。`FuncA -> TempArray -> FuncB` 改为 `FuncA -> Stream -> FuncB`，节省 BRAM 并降低延迟。 |
| **Lookup Table (LUT)** | **ROM Inference** | 复杂数学函数（如 `sigmoid`, `exp`）若精度要求不高，预计算存入 ROM，避免消耗 DSP。 |
| **Double Buffering** | **Ping-Pong Buffer** | 在 Dataflow 区域间使用 `PPO` (Ping-Pong RAM)，掩盖数据传输时间。 |
