# 术语表

本章汇总了 Luthier 中常用的术语，方便查阅。

- **AMD GPU 代码对象**：一个可以加载并在 AMD GPU 上运行的 ELF 共享对象文件。其规范在 [LLVM AMDGPU 后端文档](https://llvm.org/docs/AMDGPUUsage.html)中有详细描述。每个 AMD GPU 代码对象大致与 HIP 模块（`hipModule_t`）有一对一的关系。

- **HIP FAT 二进制文件**：针对多个 GPU 架构的 AMD GPU 代码对象集合。编译 HIP 代码时，`hipcc` 可以指示为多个 GPU 架构编译相同的代码，以便它可以在不同的 GPU 上运行。HIP FAT 二进制文件与主机可执行文件捆绑在一起，并在启动期间通过 HIP 运行时编译器 API 被 HIP 运行时识别。HIP 运行时将 FAT 二进制文件解析为单个 AMD GPU 代码对象，并将它们加载到适当的 GPU 设备上。

- **HIP 运行时编译器 API**：一组以 `__hipRegister` 为前缀的 C 库启动函数，负责加载 HIP FAT 二进制文件以及注册...

- **动态/静态 HIP 模块**：如果 HIP 模块是从 HIP FAT 二进制文件使用 HIP 编译器 API 提取的，则它是静态模块；如果 HIP 模块是使用 `hipModuleLoad` 函数加载的，则它是动态模块。

## ROCT

ROCm Thunk Interface 是用于与 AMDGPU 驱动程序接口的用户级接口。

## ROCr (HSA)

ROCm 运行时是 ROCm 平台上 Linux 上计算任务的 AMD GPU 编程的最低级别。

在文档中，除非明确指定，ROCr 和 HSA 可以互换使用。

## RocCLR

现在称为 ROCm 中的[计算语言运行时](https://github.com/roCm/clr)的一部分，RocCLR 作为一个设备、运行时和操作系统无关的抽象层，直接位于高级应用程序运行时（如 HIP、OpenCL）之下，并在低级运行时（如 ROCr、PAL）之上。它包含 HIP 和 OpenCL 使用的通用代码，允许它们使用低级别的 ROCr/PAL/OS 功能。

## HIP FAT 二进制文件

HIP FAT 二进制文件是针对不同目标架构编译的 AMDGPU 代码对象的集合，由 HIP (clang) 编译器驱动程序生成。它主要由 HIP 运行时使用。加载后，HIP 运行时将它们解析为单个 AMDGPU 代码对象，然后在适当时使用 RocCLR 将它们加载到设备上。

## Hip 编译器调度函数

以 `__hipRegister` 为前缀的函数列表，在 HIP 应用程序启动期间调用以注册...

## 静态 HIP FAT 二进制文件

使用 Clang 卸载捆绑器与主机可执行文件捆绑在一起的 HIP FAT 二进制文件被认为是静态的。静态 HIP FAT 二进制文件通常用于直接声明和使用自己的 HIP 代码的 C/C++ 应用程序。在此类应用程序的启动期间，HIP 编译器调度函数会以 `__hipRegister` 为前缀的函数列表启动并通知 HIP 运行时。静态 FAT 二进制文件可以以惰性方式加载，除非通过将 `HIP_ENABLE_DEFERRED_LOADING` 环境变量设置为零来显式禁用。

## 动态 HIP FAT 二进制文件

（此部分待补充）

## HSA (异构系统架构)

（此部分待补充）

## 工具代码对象

（此部分待补充）

## 已插桩的

（此部分待补充）

## 插桩

（此部分待补充）

## 原始（目标）

（此部分待补充）

（后续内容缺失）
