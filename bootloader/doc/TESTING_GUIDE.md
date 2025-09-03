# Bootloader 统一测试指南

**最后更新**: 2025年9月3日

## 1. 概述

本文档是 `xv6-riscv-bootloader` 项目的官方测试手册。它汇总了所有可用的测试脚本和方法，旨在为开发者提供一个清晰、从易到难的测试路径，以验证 Bootloader 的各项功能。

所有测试命令都应在项目的 `bootloader` 目录下执行。

**工作目录**:
```bash
cd /path/to/your/project/oslab/bootloader
```

---

## 2. 测试层级与流程

我们设计了由浅入深的五个测试层级。建议按顺序执行，以确保高效定位问题。

| 层级 | 测试脚本 | 目标 | 预期时间 |
| :--- | :--- | :--- | :--- |
| **L1** | `make` | **构建验证**: 确认代码可编译 | < 5秒 |
| **L2** | `test/test_stage2.sh` | **核心功能测试**: 验证Stage 2能否加载一个“测试内核” | ~20秒 |
| **L3** | `test/test_stage3_simple.sh` | **高级功能验证**: 快速检查Stage 3所有新功能是否正常 | < 5秒 |
| **L4** | `test/test_stage3.sh` | **完整系统集成测试**: 尝试引导真正的xv6内核 | > 30秒 |
| **L5** | `test/demo_stage3.sh` | **功能演示**: 快速、非交互式地展示Stage 3成果 | ~5秒 |

---

## 3. 详细测试步骤

### Level 1: 构建验证 (Sanity Check)

这是最基础的检查，用于确认开发环境正常，且代码没有语法错误。

- **目的**: 确保 `stage1.bin` 和 `stage2.bin` 可以成功生成。
- **执行命令**:
  ```bash
  make clean && make stage2.bin
  ```
- **预期结果**:
  1.  终端无错误信息输出。
  2.  `bootloader` 目录下生成 `stage1.bin` 和 `stage2.bin` 文件。
  3.  输出显示 `stage1.bin` 大小远小于512字节，`stage2.bin` 小于32768字节。

### Level 2: Stage 2 核心功能测试

此测试验证 Bootloader 的核心驱动（VirtIO）和加载能力是否正常。它使用一个轻量级的“测试内核” (`test/test_kernel.c`) 代替完整的 xv6 内核。

- **目的**: 验证 Stage 1 -> Stage 2 -> 测试内核 的完整加载链。
- **执行命令**:
  ```bash
  bash test/test_stage2.sh
  ```
- **预期结果**:
  1.  QEMU 窗口启动。
  2.  终端首先输出 Stage 1 的 `BOOT` 和 `LDG2`。
  3.  接着输出 Stage 2 的 VirtIO 驱动初始化信息和 ELF 加载进度。
  4.  最后，看到来自 `test_kernel.c` 的成功信息，如下所示：
      ```
      ========================================
          TEST KERNEL LOADED SUCCESSFULLY
      ========================================

      Bootloader Stage 2 completed!
      Two-stage loading mechanism works!
      ```

### Level 3: Stage 3 高级功能验证

此脚本通过检查 `stage2.bin` 的内容和运行一个短时间的 QEMU 实例，快速验证 Stage 3 的所有高级功能是否已集成并能打印预期信息。

- **目的**: 快速、自动化地验证内存布局、设备树、引导接口等高级功能。
- **执行命令**:
  ```bash
  bash test/test_stage3_simple.sh
  ```
- **预期结果**:
  1.  脚本无错误退出。
  2.  终端输出一系列 `✓ PASSED` 的检查项，覆盖 Stage 3.1 到 3.3 的所有核心功能点，例如：
      ```
      ✓ PASSED: Stage 3.3.1: Memory layout code present
      ✓ PASSED: Stage 3.3.2: Hardware detection code present
      ✓ PASSED: Stage 3.3.2: Device tree code present
      ✓ PASSED: Stage 3.2: Boot info magic present
      ```
  3.  最后显示 `Status: Stage 3 COMPLETE`。

### Level 4: 完整系统集成测试

这是最关键的测试，它将尝试使用我们的 Bootloader 引导一个完整的、未经修改的 xv6 操作系统。

- **目的**: 验证 Bootloader 是否能成功加载并启动真正的 xv6 内核，最终进入 shell。
- **执行命令**:
  ```bash
  bash test/test_stage3.sh
  ```
- **预期结果**:
  1.  QEMU 启动，并显示从 Stage 1 到 Stage 2 的日志。
  2.  Stage 2 开始加载内核，打印 ELF 解析和段加载信息。
  3.  **成功标志**: xv6 内核的启动信息出现，并最终停止在 `init: starting sh`，然后显示 shell 提示符 `$`。
  4.  **已知问题**: 由于 VirtIO 驱动在连续高速读取下可能存在不稳定性，测试有时会卡在内核加载阶段（例如 `Failed to read kernel sector 73`）。这属于 **Stage 3.4** 需要解决的问题。如果遇到此情况，表明 Stage 3 的功能已基本完成，但系统稳定性和驱动鲁棒性有待提升。

### Level 5: 功能演示

这是一个用于快速展示 Stage 3 所有已实现功能的演示脚本。它不会等待内核完全启动，而是通过一个简短的 QEMU 运行来展示 Bootloader 的输出。

- **目的**: 在会议或报告中快速展示 Stage 3 的工作成果。
- **执行命令**:
  ```bash
  bash test/demo_stage3.sh
  ```
- **预期结果**:
  1.  脚本首先在终端打印出 Stage 3 实现的功能清单。
  2.  然后启动 QEMU 运行5秒，在此期间，你可以看到 Stage 2 打印的内存布局、硬件平台、设备树等信息。
  3.  5秒后 QEMU 自动退出，脚本打印出文档和后续步骤的指引。

---

## 4. 总结

通过以上五个层级的测试，您可以全面地验证 Bootloader 的正确性和完整性。

- 从 **L1** 和 **L2** 开始，确保基础功能稳固。
- 使用 **L3** 快速迭代和验证新的高级功能。
- 以 **L4** 作为项目阶段性成功的最终标准。
- 使用 **L5** 进行成果展示。
