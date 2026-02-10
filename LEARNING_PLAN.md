# Luthier 学习计划

> 基于 Luthier 项目实现运行时插桩的学习路径

## 概述

Luthier 是一个面向 AMD GPU 的动态二进制插桩（DBI）框架。本计划帮助你理解其核心架构，以便在其他项目中实现类似功能。

**核心流程**: `CodeLifter` → `LiftedRepresentation` → `CodeGenerator`

---

## 第一阶段：基础概念（1-2天）

### 1.1 运行时插桩原理
阅读文档：
- [docs/1.overview.md](docs/1.overview.md) - 项目概述
- [docs/2.introduction.md](docs/2.introduction.md) - 介绍
- [docs/terminology.md](docs/terminology.md) - 术语表

### 1.2 HSA 运行时基础
阅读文档：
- [docs/4.hsa.md](docs/4.hsa.md) - HSA 编程模型
- [docs/5.hip.md](docs/5.hip.md) - HIP 基础

### 1.3 代码对象结构
阅读头文件：
- [include/luthier/HSA/LoadedCodeObject.h](include/luthier/HSA/LoadedCodeObject.md) - 代码对象
- [include/luthier/HSA/KernelDescriptor.h](include/luthier/HSA/KernelDescriptor.md) - 内核描述符

---

## 第二阶段：符号系统（2-3天）

### 2.1 符号层次结构
阅读头文件（按顺序）：
1. [include/luthier/HSA/LoadedCodeObjectSymbol.h](include/luthier/HSA/LoadedCodeObjectSymbol.md) - 符号基类（必读）
2. [include/luthier/HSA/LoadedCodeObjectKernel.h](include/luthier/HSA/LoadedCodeObjectKernel.md) - 内核符号
3. [include/luthier/HSA/LoadedCodeObjectDeviceFunction.h](include/luthier/HSA/LoadedCodeObjectDeviceFunction.md) - 设备函数符号
4. [include/luthier/HSA/LoadedCodeObjectVariable.h](include/luthier/HSA/LoadedCodeObjectVariable.md) - 变量符号

### 2.2 指令表示
阅读：
- [include/luthier/HSA/Instr.h](include/luthier/HSA/Instr.md) - 指令表示

### 2.3 ELF 文件处理
相关头文件：
- [include/luthier/HSA/ISA.h](include/luthier/HSA/ISA.md) - ISA 相关

---

## 第三阶段：核心架构（5-7天）

这是最关键的阶段，需要认真研读。

### 3.1 CodeLifter - 代码提升器
**优先级：最高**

头文件：
- [include/luthier/Tooling/CodeLifter.h](include/luthier/Tooling/CodeLifter.md)

实现文件：
- [src/lib/ToolingCommon/CodeLifter.cpp](src/lib/ToolingCommon/CodeLifter.cpp)

**学习目标**：
- [ ] 理解 MC 反汇编流程
- [ ] 理解缓存机制（CacheMutex）
- [ ] 掌握 `disassemble()` 和 `lift()` 方法
- [ ] 理解分支目标地址缓存

### 3.2 LiftedRepresentation - MIR 表示封装
**优先级：最高**

头文件：
- [include/luthier/Tooling/LiftedRepresentation.h](include/luthier/Tooling/LiftedRepresentation.md)

实现文件：
- [src/lib/ToolingCommon/LiftedRepresentation.cpp](src/lib/ToolingCommon/LiftedRepresentation.cpp)

**学习目标**：
- [ ] 理解 LLVM MIR 表示结构
- [ ] 掌握 MachineFunction ↔ HSA 符号映射
- [ ] 理解 `getLiftedEquivalent()` 查询机制
- [ ] 理解 MachineInstr ↔ MC 指令映射

### 3.3 InstrumentationTask - 插桩任务
**优先级：高**

头文件：
- [include/luthier/Tooling/InstrumentationTask.h](include/luthier/Tooling/InstrumentationTask.md)

实现文件：
- [src/lib/ToolingCommon/InstrumentationTask.cpp](src/lib/ToolingCommon/InstrumentationTask.cpp)

**学习目标**：
- [ ] 掌握 `insertHookBefore()` 核心方法
- [ ] 理解钩子插入队列机制
- [ ] 理解 hook_invocation_descriptor 结构

### 3.4 CodeGenerator - 代码生成器
**优先级：高**

头文件：
- [include/luthier/Tooling/CodeGenerator.h](include/luthier/Tooling/CodeGenerator.md)

实现文件：
- [src/lib/ToolingCommon/CodeGenerator.cpp](src/lib/ToolingCommon/CodeGenerator.cpp)

**学习目标**：
- [ ] 理解 LLVM IR → 目标代码编译流程
- [ ] 掌握 intrinsic 注册和 lowering 机制
- [ ] 理解 `printAssembly()` 和 `instrument()` 方法

### 3.5 Context - 生命周期管理
**优先级：中**

头文件：
- [include/luthier/Tooling/Context.h](include/luthier/Tooling/Context.md)

实现文件：
- [src/lib/ToolingCommon/Context.cpp](src/lib/ToolingCommon/Context.cpp)

**学习目标**：
- [ ] 理解单例管理模式
- [ ] 掌握 ROCProfiler SDK 注册流程

---

## 第四阶段：钩子系统（2-3天）

### 4.1 设备端 Intrinsics
阅读：
- [include/luthier/Intrinsic/Intrinsics.h](include/luthier/Intrinsic/Intrinsics.md)
- [include/luthier/Intrinsic/ImplicitArgPtr.h](include/luthier/Intrinsic/ImplicitArgPtr.md)

实现：
- [src/lib/Intrinsic/*.cpp](src/lib/Intrinsic/)

### 4.2 常量定义
阅读：
- [include/luthier/consts.h](include/luthier/consts.md)

### 4.3 钩子宏和注解
理解以下关键宏：
- `LUTHIER_HOOK_ANNOTATE`
- `LUTHIER_EXPORT_HOOK_HANDLE()`
- `LUTHIER_GET_HOOK_HANDLE()`
- `MARK_LUTHIER_DEVICE_MODULE`

---

## 第五阶段：公共 API（1-2天）

阅读公共接口：
- [include/luthier/luthier.h](include/luthier/luthier.md) - 主 API

**重点方法**：
- `disassemble()` - 反汇编
- `lift()` - 提升
- `instrument()` - 插桩
- `instrumentAndLoad()` - 插桩并加载

---

## 第六阶段：示例分析（3-4天）

### 6.1 完整示例
按顺序分析示例工具：

1. **OpcodeHistogram** - 最完整的示例
   - [examples/OpcodeHistogram/OpcodeHistogram.hip](examples/OpcodeHistogram/OpcodeHistogram.hip)
   - 包含完整钩子定义、插桩回调、HSA 拦截

2. **InstrCount** - 简单计数器
   - [examples/InstrCount/InstrCount.hip](examples/InstrCount/InstrCount.hip)

3. **LDSBankConflict** - LDS 银行冲突检测
   - [examples/LDSBankConflict/LDSBankConflict.hip](examples/LDSBankConflict/LDSBankConflict.hip)

### 6.2 文档参考
- [docs/tool-code.md](docs/tool-code.md) - 工具代码编写指南
- [docs/hook_insertion.md](docs/hook_insertion.md) - 钩子插入
- [docs/rocprofiler.md](docs/rocprofiler.md) - ROCProfiler SDK

---

## 第七阶段：实践练习（5-7天）

### 练习 1：最小工具模板
创建一个最简单的插桩工具：
- 定义一个设备钩子函数
- 实现基本的插桩回调
- 编译和运行

### 练习 2：指令计数
扩展练习 1：
- 统计每种 opcode 的出现次数
- 使用 LDS 存储计数器

### 练习 3：寄存器读写
添加寄存器读写功能：
- 使用 `readReg()` / `writeReg()`
- 读取执行掩码等

### 练习 4：完整工具
实现一个自定义的运行时分析工具

---

## 附录：关键文件索引

| 组件 | 头文件 | 实现文件 |
|------|--------|----------|
| 主 API | `include/luthier/luthier.h` | - |
| CodeLifter | `include/luthier/Tooling/CodeLifter.h` | `src/lib/ToolingCommon/CodeLifter.cpp` |
| LiftedRepresentation | `include/luthier/Tooling/LiftedRepresentation.h` | `src/lib/ToolingCommon/LiftedRepresentation.cpp` |
| CodeGenerator | `include/luthier/Tooling/CodeGenerator.h` | `src/lib/ToolingCommon/CodeGenerator.cpp` |
| InstrumentationTask | `include/luthier/Tooling/InstrumentationTask.h` | `src/lib/ToolingCommon/InstrumentationTask.cpp` |
| Context | `include/luthier/Tooling/Context.h` | `src/lib/ToolingCommon/Context.cpp` |
| HSA 符号 | `include/luthier/HSA/LoadedCodeObject*.h` | `src/lib/HSA/*.cpp` |
| Intrinsics | `include/luthier/Intrinsic/Intrinsics.h` | `src/lib/Intrinsic/*.cpp` |

---

## 学习建议

1. **不要跳读**：每个头文件都要完整阅读，包括注释
2. **配合实现**：只看头文件不够，必须阅读对应的 .cpp 实现
3. **动手实践**：看完一个模块后尝试修改示例代码验证理解
4. **绘制图表**：理解核心流程后，自己绘制数据流图
5. **提问验证**：理解每个模块的输入、输出和副作用
