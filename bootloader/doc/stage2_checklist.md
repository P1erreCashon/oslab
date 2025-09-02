# 第二阶段实施检查清单

## 📅 每日任务检查表

### 第1天：基础框架搭建 ✅ 准备开始

#### 上午任务 (9:00-12:00)
- [ ] **分析kernel代码复用性**
  - [ ] 阅读`kernel/virtio.h`获取常量定义
  - [ ] 分析`kernel/virtio_disk.c`的设备初始化流程
  - [ ] 识别`kalloc`, `memset`, `panic`等依赖函数
  - [ ] 确定需要简化实现的部分

- [ ] **创建目录结构**
  ```bash
  mkdir -p bootloader/stage1 bootloader/stage2 bootloader/common bootloader/test
  ```

- [ ] **创建基础头文件**
  - [ ] `bootloader/common/boot_types.h` - 基础类型定义
  - [ ] `bootloader/common/virtio_boot.h` - virtio接口定义

#### 下午任务 (14:00-18:00)
- [ ] **实现内存管理模块**
  - [ ] `bootloader/common/memory.c` - 简化的内存分配器
  - [ ] 实现`boot_alloc()`, `boot_alloc_page()`函数
  - [ ] 实现`memset()`, `memmove()`函数

- [ ] **实现UART输出模块**
  - [ ] `bootloader/common/uart.c` - 串口输出功能
  - [ ] 实现`uart_putc()`, `uart_puts()`函数
  - [ ] 实现`uart_put_hex()`, `uart_put_dec()`调试函数

#### 验收检查
- [ ] 所有文件编译通过（即使还没有主函数）
- [ ] 内存分配器基本功能正确
- [ ] UART输出功能正常
- [ ] 代码风格与kernel保持一致

---

### 第2天：virtio驱动实现 🔄 待开始

#### 上午任务 (9:00-12:00)
- [ ] **virtio设备初始化**
  - [ ] 创建`bootloader/common/virtio_boot.c`
  - [ ] 实现`virtio_disk_boot_init()`函数
  - [ ] 复用kernel的设备验证逻辑
  - [ ] 实现特性协商流程

- [ ] **队列管理实现**
  - [ ] 实现描述符分配：`alloc_desc()`, `free_desc()`
  - [ ] 实现队列初始化逻辑
  - [ ] 设置virtio队列物理地址

#### 下午任务 (14:00-18:00)
- [ ] **磁盘读取功能**
  - [ ] 实现`virtio_disk_read_sync()`函数
  - [ ] 构建virtio请求描述符链
  - [ ] 实现轮询等待机制（不使用中断）
  - [ ] 添加错误处理和超时机制

- [ ] **单元测试编写**
  - [ ] 创建简单的测试程序验证磁盘读取
  - [ ] 测试读取固定扇区的数据

#### 验收检查
- [ ] virtio设备能够成功初始化
- [ ] 能够读取磁盘扇区数据
- [ ] 错误处理机制工作正常
- [ ] 调试输出信息详细

---

### 第3天：两阶段加载实现 🔄 待开始

#### 上午任务 (9:00-12:00)
- [ ] **扩展第一阶段代码**
  - [ ] 修改`bootloader/boot.S`支持C函数调用
  - [ ] 创建`bootloader/stage1/boot_stage1.c`
  - [ ] 实现`load_stage2()`函数
  - [ ] 从磁盘扇区1加载32KB第二阶段代码

- [ ] **第二阶段入口**
  - [ ] 创建`bootloader/stage2/stage2_start.S`
  - [ ] 设置第二阶段栈和环境
  - [ ] 跳转到C主函数

#### 下午任务 (14:00-18:00)
- [ ] **第二阶段主程序**
  - [ ] 创建`bootloader/stage2/main.c`
  - [ ] 实现`bootloader_main()`函数
  - [ ] 读取内核ELF头并验证
  - [ ] 简单的内核加载逻辑

- [ ] **链接脚本配置**
  - [ ] 创建`bootloader/stage2/stage2.ld`
  - [ ] 配置第二阶段代码加载地址
  - [ ] 确保内存布局不冲突

#### 验收检查
- [ ] 第一阶段能够加载第二阶段
- [ ] 第二阶段能够启动并显示信息
- [ ] 两阶段之间跳转正常
- [ ] 内存布局规划合理

---

### 第4天：构建系统与测试 🔄 待开始

#### 上午任务 (9:00-12:00)
- [ ] **完善构建系统**
  - [ ] 更新`bootloader/Makefile`支持分阶段构建
  - [ ] 添加大小检查和验证
  - [ ] 创建磁盘镜像构建目标
  - [ ] 集成到主项目Makefile

- [ ] **创建测试内核**
  - [ ] 创建`bootloader/test/test_kernel.c`
  - [ ] 实现简单的测试内核程序
  - [ ] 创建`bootloader/test/test_kernel.ld`
  - [ ] 验证内核能被bootloader加载

#### 下午任务 (14:00-18:00)
- [ ] **测试脚本和自动化**
  - [ ] 创建`bootloader/test/test_stage2.sh`
  - [ ] 添加自动化构建和测试
  - [ ] 创建调试辅助脚本
  - [ ] 添加错误场景测试

- [ ] **集成测试**
  - [ ] 完整的两阶段启动流程测试
  - [ ] 从virtio磁盘启动测试
  - [ ] 内核加载和跳转测试

#### 验收检查
- [ ] 完整的构建系统工作正常
- [ ] 能够从磁盘启动进入第二阶段
- [ ] 第二阶段能够加载并跳转到测试内核
- [ ] 错误处理和调试信息完善

---

### 第5天：调试优化与文档 🔄 待开始

#### 上午任务 (9:00-12:00)
- [ ] **调试和问题修复**
  - [ ] 使用GDB调试两阶段启动流程
  - [ ] 修复发现的内存布局问题
  - [ ] 优化virtio驱动稳定性
  - [ ] 完善错误处理机制

- [ ] **性能优化**
  - [ ] 减少第一阶段代码大小
  - [ ] 优化磁盘读取效率
  - [ ] 减少启动时间

#### 下午任务 (14:00-18:00)
- [ ] **文档完善**
  - [ ] 创建`bootloader/README.md`
  - [ ] 创建`bootloader/doc/api_reference.md`
  - [ ] 更新`bootloader/doc/stage2_progress.md`
  - [ ] 完善代码注释

- [ ] **最终验证**
  - [ ] 完整功能回归测试
  - [ ] 性能指标测试
  - [ ] 代码质量检查

#### 验收检查
- [ ] 所有功能稳定可靠
- [ ] 文档完整易懂
- [ ] 代码质量达标
- [ ] 为第三阶段做好准备

---

## 🛠️ 开发工具和命令

### 常用构建命令
```bash
# 进入bootloader目录
cd bootloader

# 清理所有构建产物
make clean

# 构建第一阶段
make stage1.bin

# 构建第二阶段  
make stage2.bin

# 构建完整磁盘镜像
make bootdisk_stage2.img

# 测试运行
make test

# 调试模式
make debug
```

### 调试命令
```bash
# 查看第一阶段反汇编
make disasm-stage1

# 查看第二阶段反汇编
make disasm-stage2

# 查看磁盘镜像内容
make inspect-disk

# 查看十六进制内容
make hexdump-stage1
make hexdump-stage2
```

### GDB调试流程
```bash
# 终端1: 启动QEMU调试模式
make debug

# 终端2: 启动GDB
riscv64-unknown-elf-gdb bootloader/stage1.elf
(gdb) target remote localhost:1234
(gdb) b _start
(gdb) c
(gdb) b load_stage2
(gdb) c
```

## 📈 进度跟踪

### 完成情况记录
使用以下格式记录每日进度：

```markdown
## 第X天进度 (YYYY-MM-DD)

### 已完成 ✅
- [x] 任务描述

### 进行中 🔄  
- [ ] 任务描述 (50%完成)

### 遇到的问题 ⚠️
- 问题描述和解决方案

### 明天计划 📋
- 下一步任务安排
```

## 🚨 风险预警

### 可能遇到的问题
1. **第一阶段代码大小超限** (512字节)
   - **预防**: 严格控制功能，优化汇编代码
   - **应对**: 移除非必要功能，使用更紧凑的实现

2. **virtio驱动复杂度过高**
   - **预防**: 充分复用kernel代码，简化为同步模式
   - **应对**: 进一步简化，专注核心读取功能

3. **内存地址冲突**
   - **预防**: 详细的内存布局规划和验证
   - **应对**: 调整地址分配，使用链接脚本精确控制

4. **QEMU配置问题**
   - **预防**: 参考working xv6配置，逐步修改
   - **应对**: 对比kernel启动方式，找出差异

---
**创建时间**: 2025年9月2日  
**负责人**: GitHub Copilot  
**预期持续时间**: 5个工作日
