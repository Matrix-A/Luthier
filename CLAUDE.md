# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

**Luthier** 是一个面向 AMD GPU 的动态二进制插桩（DBI）框架，类似于 NVIDIA 的 NVBit 或 Intel 的 GTPin。它支持在运行时分析、修改和插桩 AMD GPU 代码对象。

### 核心功能
- 分析加载到 GPU 上的代码对象内容（内核、设备函数、静态变量等）
- 插入对（多个）设备插桩函数的调用
- 删除/修改/添加指令
- 查询/修改 GPU ISA 的可见状态

### 支持的应用
- 基于 ROCm 运行时的任何应用程序（HIP、OpenMP、OpenCL、直接使用 ROCr）

### 不支持的应用
- 基于 PAL（Platform Abstraction Layer）的应用程序
- Windows 系统

## 构建命令

### 前置要求
- ROCm 6.2.2+
- LLVM amd-staging 分支（版本 21）
- CMake 3.21+
- C++23 编译器
- Python 3 + cxxheaderparser + pcpp

### 构建步骤
```bash
# 1. 构建 LLVM（amd-staging 分支）
git clone --depth 1 https://github.com/ROCm/llvm-project/
mkdir llvm-project/build && cd llvm-project/build
cmake -G Ninja \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_INSTALL_PREFIX=/opt/luthier/llvm/ \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD="AMDGPU;X86" \
  -DLLVM_ENABLE_PROJECTS="llvm;clang;lld;compiler-rt;clang-tools-extra" \
  -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
  -DLLVM_ENABLE_RTTI=ON \
  -DLLVM_OPTIMIZED_TABLEGEN=ON \
  -DCLANG_ENABLE_AMDCLANG=ON \
  -DLLVM_AMDGPU_ALLOW_NPI_TARGETS=ON \
  ../llvm && ninja install

# 2. 构建 AMD device-libs
cd ../amd/device-libs && mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/luthier/ \
  -DCMAKE_PREFIX_PATH="/opt/luthier/" ../ && ninja install

# 3. 构建 Luthier
cd /path/to/luthier && mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH="/opt/luthier;/opt/rocm" \
  -DCMAKE_HIP_COMPILER=/opt/luthier/llvm/bin/clang++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_HIP_FLAGS="-O3" \
  -G Ninja ..
ninja
```

### CMake 构建选项
| 选项 | 说明 | 默认值 |
|------|------|--------|
| `LUTHIER_LLVM_SRC_DIR` | LLVM 源码路径 | 自动下载 |
| `LUTHIER_BUILD_EXAMPLES` | 构建示例工具 | ON |
| `LUTHIER_BUILD_UNIT_TESTS` | 构建单元测试 | OFF |
| `LUTHIER_BUILD_INTEGRATION_TESTS` | 构建集成测试 | OFF |
| `CMAKE_BUILD_TYPE` | 构建类型 | Release |
| `CMAKE_HIP_COMPILER` | HIP 编译器路径 | 必需指定 |

## 核心架构

### 核心流程：`CodeLifter` → `LiftedRepresentation` → `CodeGenerator`

```
目标应用 → HSA API → ROCProfiler SDK 回调
                              ↓
                         CodeLifter::lift()
                              ↓
                    创建 LiftedRepresentation（缓存）
                              ↓
                    用户调用 luthier::instrument()
                              ↓
                    CodeGenerator 应用 InstrumentationTask
                              ↓
                    insertHookBefore() 排队钩子
                              ↓
                    LLVM Passes → CodeGen
                              ↓
                    printAssembly() 生成目标代码
                              ↓
                    Comgr 链接 → HSA 加载
```

### 1. CodeLifter ([include/luthier/Tooling/CodeLifter.h](include/luthier/Tooling/CodeLifter.h))

**单例，负责将 GPU 代码反汇编并提升到 LLVM MIR**

#### 关键成员
```cpp
class CodeLifter : public Singleton<CodeLifter> {
private:
  std::recursive_mutex CacheMutex{};  // 缓存互斥锁

  // MC 反汇编缓存: Symbol → 指令数组
  std::unordered_map<符号, unique_ptr<SmallVector<hsa::Instr>>>
      MCDisassembledSymbols{};

  // 提升表示缓存: Kernel → LiftedRepresentation
  std::unordered_map<Kernel符号, unique_ptr<LiftedRepresentation>>
      LiftedKernelSymbols{};

  // 分支目标地址缓存
  llvm::DenseMap<hsa_loaded_code_object_t, DenseSet<address_t>>
      DirectBranchTargetLocations{};

  // 重定位信息缓存
  llvm::DenseMap<hsa_loaded_code_object_t, DenseMap<address_t, LCORelocationInfo>>
      Relocations{};
};
```

#### 关键方法
```cpp
// 反汇编函数符号（模板，支持 Kernel 和 DeviceFunction）
template <typename ST>
llvm::Expected<llvm::ArrayRef<hsa::Instr>> disassemble(const ST &Symbol);

// 提升内核到 MIR
llvm::Expected<const LiftedRepresentation &>
lift(const hsa::LoadedCodeObjectKernel &KernelSymbol);

// 克隆提升的表示
llvm::Expected<std::unique_ptr<LiftedRepresentation>>
cloneRepresentation(const LiftedRepresentation &SrcLR);
```

### 2. LiftedRepresentation ([include/luthier/Tooling/LiftedRepresentation.h](include/luthier/Tooling/LiftedRepresentation.h))

**封装内核的 LLVM MIR 表示，"Lifting" 是核心概念**

"Lifting"：将 GPU 上加载的 AMDGPU 二进制内容检查过程，以恢复等效或接近原始编译器使用的 LLVM Machine IR 表示。

#### 核心成员
```cpp
class LiftedRepresentation {
private:
  std::unique_ptr<llvm::GCNTargetMachine> TM{};           // 目标机器
  llvm::orc::ThreadSafeContext Context{};                  // 线程安全上下文
  hsa_loaded_code_object_t LCO{};                          // 加载的代码对象
  std::unique_ptr<llvm::Module> Module{};                  // LLVM 模块
  std::unique_ptr<llvm::MachineModuleInfoWrapperPass> MMIWP{}; // MIR 信息
  std::unique_ptr<hsa::LoadedCodeObjectKernel> Kernel{};  // 内核符号

  // 设备函数映射: 符号 → MachineFunction
  std::unordered_map<DeviceFunction符号, llvm::MachineFunction *> Functions{};

  // 静态变量映射: 符号 → GlobalVariable
  std::unordered_map<符号, llvm::GlobalVariable *> Variables{};

  // MI → MC 映射（仅在运行 LLVM Pass 前有效）
  llvm::DenseMap<llvm::MachineInstr *, hsa::Instr *> MachineInstrToMCMap{};
};
```

#### 关键方法
```cpp
// 获取内核 MachineFunction
llvm::MachineFunction &getKernelMF();
llvm::MachineModuleInfo &getMMI();

// 迭代所有定义的函数类型（包括内核和设备函数）
llvm::Error iterateAllDefinedFunctionTypes(
    const std::function<llvm::Error(const hsa::LoadedCodeObjectSymbol &,
                                    llvm::MachineFunction &)> &Lambda);

// HSA → LLVM 映射查询
const hsa::Instr *getLiftedEquivalent(const llvm::MachineInstr &MI) const;
llvm::GlobalValue *getLiftedEquivalent(const hsa::LoadedCodeObjectKernel &Symbol);
llvm::Function *getLiftedEquivalent(const hsa::LoadedCodeObjectDeviceFunction &Symbol);
```

### 3. CodeGenerator ([include/luthier/Tooling/CodeGenerator.h](include/luthier/Tooling/CodeGenerator.h))

**单例，负责生成插桩后的机器码**

#### 关键成员
```cpp
class CodeGenerator : public Singleton<CodeGenerator> {
private:
  // Intrinsic 处理器映射
  llvm::StringMap<IntrinsicProcessor> IntrinsicsProcessors{};

  const rocprofiler::HsaApiTableSnapshot<::CoreApiTable> &CoreApiSnapshot;
  const rocprofiler::HsaExtensionTableSnapshot<HSA_EXTENSION_AMD_LOADER>
      &LoaderApiSnapshot;
};
```

#### 关键方法
```cpp
// 注册 intrinsic 及其 lowering 方式
void registerIntrinsic(llvm::StringRef Name, IntrinsicProcessor Processor);

// 插桩 LiftedRepresentation
llvm::Expected<std::unique_ptr<LiftedRepresentation>>
instrument(const LiftedRepresentation &LR,
           llvm::function_ref<llvm::Error(InstrumentationTask &,
                                          LiftedRepresentation &)> Mutator);

// 打印汇编/目标文件
static llvm::Error
printAssembly(llvm::Module &Module,
             llvm::GCNTargetMachine &TM,
             std::unique_ptr<llvm::MachineModuleInfoWrapperPass> &MMIWP,
             llvm::SmallVectorImpl<char> &CompiledObjectFile,
             llvm::CodeGenFileType FileType);
```

### 4. InstrumentationTask ([include/luthier/Tooling/InstrumentationTask.h](include/luthier/Tooling/InstrumentationTask.h))

**描述如何修改 LiftedRepresentation**

#### 关键成员
```cpp
class InstrumentationTask {
private:
  LiftedRepresentation &LR;                    // 被插桩的表示
  const InstrumentationModule &IM;              // 插桩模块

  // 钩子插入任务: MI → 钩子列表
  typedef llvm::DenseMap<llvm::MachineInstr *,
                         llvm::SmallVector<hook_invocation_descriptor, 1>>
      hook_insertion_tasks;
  hook_insertion_tasks HookInsertionTasks{};
};
```

#### 关键方法
```cpp
// 在指令前排队钩子插入任务（唯一的前端修改方式）
llvm::Error insertHookBefore(
    llvm::MachineInstr &MI,
    const void *Hook,                          // 钩子句柄（来自 LUTHIER_GET_HOOK_HANDLE）
    llvm::ArrayRef<std::variant<llvm::Constant *, llvm::MCRegister>> Args = {});
```

### 5. Context ([include/luthier/Tooling/Context.h](include/luthier/Tooling/Context.h))

**单例，管理所有其他单例和 rocprofiler-sdk 注册**

#### 关键成员
```cpp
class Context : public Singleton<Context> {
private:
  CodeGenerator *CG{nullptr};           // 代码生成器
  ToolExecutableLoader *TEL{nullptr};   // 工具加载器
  CodeLifter *CL{nullptr};              // 代码提升器
  TargetManager *TM{nullptr};           // 目标管理器

  // ROCProfiler API 表快照
  rocprofiler::HipApiTableSnapshot<ROCPROFILER_HIP_COMPILER_TABLE> *HipCompilerTableSnapshot{nullptr};
  rocprofiler::HsaApiTableSnapshot<::CoreApiTable> *HsaCoreApiTableSnapshot{nullptr};
  rocprofiler::HsaApiTableSnapshot<::AmdExtTable> *HsaAmdExtTableSnapshot{nullptr};
  rocprofiler::HsaExtensionTableSnapshot<HSA_EXTENSION_AMD_LOADER> *VenLoaderSnapshot{nullptr};

  // 缓存和监控
  hsa::LoadedCodeObjectCache *CodeObjectCache{nullptr};
  amdgpu::hsamd::MetadataParser *MDParser{nullptr};
  hsa::PacketMonitor *PacketMonitor{nullptr};
};
```

### 6. InstrumentationModule ([include/luthier/Tooling/InstrumentationModule.h](include/luthier/Tooling/InstrumentationModule.h))

**管理工具的 LLVM 位码缓冲区和静态变量**

```cpp
class InstrumentationModule {
public:
  enum ModuleKind { MK_Static, MK_Dynamic };

protected:
  std::string CUID{};                              // 编译单元 ID
  llvm::SmallVector<std::string, 4> GlobalVariables{}; // 全局变量名列表

public:
  // 读取位码到上下文
  virtual llvm::Expected<std::unique_ptr<llvm::Module>>
  readBitcodeIntoContext(llvm::LLVMContext &Ctx, hsa_agent_t Agent) const = 0;

  // 获取全局变量加载地址
  virtual llvm::Expected<std::optional<luthier::address_t>>
  getGlobalVariablesLoadedOnAgent(llvm::StringRef GVName, hsa_agent_t Agent) const = 0;
};

// 静态插桩模块（通过 HIP FAT 二进制加载）
class StaticInstrumentationModule final : public InstrumentationModule {
private:
  llvm::SmallDenseMap<hsa_agent_t, llvm::ArrayRef<char>, 8>
      PerAgentBitcodeBufferMap{};     // 每个 Agent 的位码缓冲

  llvm::DenseMap<hsa_agent_t, hsa_executable_t> PerAgentModuleExecutables{}; // 模块可执行文件

  llvm::DenseMap<const void *, llvm::StringRef> HookHandleMap{}; // 钩子句柄映射
};
```

## 符号层次结构

### LoadedCodeObjectSymbol 基类 ([include/luthier/HSA/LoadedCodeObjectSymbol.h](include/luthier/HSA/LoadedCodeObjectSymbol.h))

```cpp
enum SymbolKind {
  SK_KERNEL,           // 内核
  SK_DEVICE_FUNCTION,  // 设备函数
  SK_VARIABLE,         // 变量
  SK_EXTERNAL          // 外部符号
};

class LoadedCodeObjectSymbol {
protected:
  hsa_loaded_code_object_t BackingLCO{};              // 所属代码对象
  luthier::object::AMDGCNObjectFile &StorageELF;      // 存储 ELF
  llvm::object::ELFSymbolRef Symbol;                  // ELF 符号
  SymbolKind Kind;                                    // 符号类型
  std::optional<hsa_executable_symbol_t> ExecutableSymbol; // HSA 符号
};
```

### 内核符号 ([include/luthier/HSA/LoadedCodeObjectKernel.h](include/luthier/HSA/LoadedCodeObjectKernel.h))

```cpp
class LoadedCodeObjectKernel final : public LoadedCodeObjectSymbol {
private:
  const llvm::object::ELFSymbolRef KDSymbol;                  // 内核描述符符号
  std::unique_ptr<amdgpu::hsamd::Kernel::Metadata> MD;        // 内核元数据

public:
  // 获取内核描述符
  llvm::Expected<const KernelDescriptor *> getKernelDescriptor(...) const;

  // 获取内核元数据
  const amdgpu::hsamd::Kernel::Metadata &getKernelMetadata() const;
};
```

### 设备函数符号 ([include/luthier/HSA/LoadedCodeObjectDeviceFunction.h](include/luthier/HSA/LoadedCodeObjectDeviceFunction.h))

```cpp
class LoadedCodeObjectDeviceFunction final : public LoadedCodeObjectSymbol {
  // 继承自 LoadedCodeObjectSymbol，类型为 SK_DEVICE_FUNCTION
};
```

## 指令表示 ([include/luthier/HSA/Instr.h](include/luthier/HSA/Instr.h))

```cpp
class Instr {
private:
  const llvm::MCInst Inst;                       // MC 指令
  const address_t LoadedDeviceAddress;           // 设备加载地址
  const LoadedCodeObjectSymbol &Symbol;           // 所属符号
  const size_t Size;                              // 指令大小

public:
  llvm::MCInst getMCInst() const;
  address_t getLoadedDeviceAddress() const;
  size_t getSize() const;
  const LoadedCodeObjectSymbol &getLoadedCodeObjectSymbol() const;
};
```

## 公共 API ([include/luthier/luthier.h](include/luthier/luthier.h))

### 检查 API
```cpp
// 反汇编内核为指令数组（首次调用会缓存结果）
llvm::Expected<llvm::ArrayRef<hsa::Instr>>
disassemble(const hsa::LoadedCodeObjectKernel &Kernel);

// 提升内核并返回 LiftedRepresentation
llvm::Expected<const LiftedRepresentation &>
lift(const hsa::LoadedCodeObjectKernel &Kernel);
```

### 插桩 API
```cpp
// 应用 Mutator 插桩 LiftedRepresentation
llvm::Expected<std::unique_ptr<LiftedRepresentation>>
instrument(const LiftedRepresentation &LR,
           llvm::function_ref<llvm::Error(InstrumentationTask &,
                                          LiftedRepresentation &)> Mutator);

// 插桩并加载到设备
llvm::Error
instrumentAndLoad(const hsa::LoadedCodeObjectKernel &Kernel,
                  const LiftedRepresentation &LR,
                  llvm::function_ref<llvm::Error(InstrumentationTask &,
                                                 LiftedRepresentation &)> Mutator,
                  llvm::StringRef Preset);

// 检查内核是否已被插桩
llvm::Expected<bool>
isKernelInstrumented(const hsa::LoadedCodeObjectKernel &Kernel,
                     llvm::StringRef Preset);

// 覆盖内核对象以使用插桩版本
llvm::Error overrideWithInstrumented(hsa_kernel_dispatch_packet_t &Packet,
                                     llvm::StringRef Preset);
```

## 编写工具

### 必需的工具组件

#### 1. 设备钩子函数
```cpp
// 钩子注解：设备函数 + luthier_hook 属性
LUTHIER_HOOK_ANNOTATE void myHook(int arg1, float arg2) {
    // 设备端代码
}

// 导出钩子句柄（生成 __luthier_hook_handle_myHook）
LUTHIER_EXPORT_HOOK_HANDLE(myHook);

// 获取钩子句柄（用于 insertHookBefore）
auto hookHandle = LUTHIER_GET_HOOK_HANDLE(myHook);
```

#### 2. MARK_LUTHIER_DEVICE_MODULE 宏
```cpp
// 必需放在工具代码中，强制 HIP 运行时加载工具 FAT 二进制
MARK_LUTHIER_DEVICE_MODULE
```
原理：创建 `__attribute__((managed))` 变量，HIP 运行时遇到托管变量会立即加载 FAT 二进制。

#### 3. 插桩回调
```cpp
static llvm::Error instrumentKernel(InstrumentationTask &IT,
                                    LiftedRepresentation &LR) {
    // 遍历所有 MachineBasicBlock 和 MachineInstr
    return LR.iterateAllDefinedFunctionTypes(
        [&](const hsa::LoadedCodeObjectSymbol &Sym,
            llvm::MachineFunction &MF) -> llvm::Error {
            for (auto &MBB : MF) {
                for (auto &MI : MBB) {
                    // 在每个指令前插入钩子
                    IT.insertHookBefore(MI,
                        LUTHIER_GET_HOOK_HANDLE(myHook),
                        {arg1, arg2});
                }
            }
            return llvm::Error::success();
        });
}
```

#### 4. ROCProfiler 入口点
```cpp
extern "C" __attribute__((used)) rocprofiler_tool_configure_result_t *
rocprofiler_configure(uint32_t Version, const char *RuntimeVersion,
                     uint32_t Priority, rocprofiler_client_id_t *ClientID) {
    // 初始化工具
    luthier::atToolInit(nullptr);

    ClientID->name = "My Tool Name";

    static auto Cfg = rocprofiler_tool_configure_result_t{
        sizeof(rocprofiler_tool_configure_result_t),
        nullptr,  // 配置回调
        &luthier::atToolFini,  // 清理回调
        nullptr};
    return &Cfg;
}
```

### 钩子中可用的 Intrinsic ([include/luthier/Intrinsic/Intrinsics.h](include/luthier/Intrinsic/Intrinsics.h))

```cpp
// 读取 GPU 寄存器
template <typename T, ...>
LUTHIER_INTRINSIC_ANNOTATE T readReg(llvm::MCRegister Reg);

// 写入 GPU 寄存器
template <typename T, ...>
LUTHIER_INTRINSIC_ANNOTATE void writeReg(llvm::MCRegister Reg, T Val);

// 写入 exec 寄存器
LUTHIER_INTRINSIC_ANNOTATE void writeExec(uint64_t Val);

// 隐式参数指针
LUTHIER_INTRINSIC_ANNOTATE uint32_t *implicitArgPtr();

// 工作组 ID
LUTHIER_INTRINSIC_ANNOTATE uint32_t workgroupIdX();
LUTHIER_INTRINSIC_ANNOTATE uint32_t workgroupIdY();
LUTHIER_INTRINSIC_ANNOTATE uint32_t workgroupIdZ();

// 标量原子加法
template <typename T, ...>
LUTHIER_INTRINSIC_ANNOTATE T sAtomicAdd(T *Address, T Value);
```

### 常量定义 ([include/luthier/consts.h](include/luthier/consts.h))

```cpp
#define LUTHIER_HOOK_HANDLE_PREFIX __luthier_hook_handle_      // 钩子句柄前缀
#define LUTHIER_HOOK_ATTRIBUTE luthier_hook                    // 钩子属性
#define LUTHIER_RESERVED_MANAGED_VAR __luthier_reserved        // 保留的托管变量名
#define LUTHIER_INTRINSIC_ATTRIBUTE luthier_intrinsic          // 内在函数属性
#define LUTHIER_HIP_CUID_PREFIX __hip_cuid_                    // HIP CUID 前缀
#define LUTHIER_INJECTED_PAYLOAD_ATTRIBUTE luthier_injected_payload // 注入负载属性
```

## 示例工具结构

参考 [examples/OpcodeHistogram/OpcodeHistogram.hip](examples/OpcodeHistogram/OpcodeHistogram.hip):

1. 包含必要头文件
2. 定义设备端钩子函数
3. 使用 `MARK_LUTHIER_DEVICE_MODULE`
4. 实现 `instrumentationLoop` 回调
5. 实现 HSA 队列拦截回调
6. 实现 `atToolInit` 和 `atToolFini`
7. 导出 `rocprofiler_configure`

## 重要的实现细节

1. **C++23** 主机端代码要求
2. **HIP** 用于编写设备端工具代码
3. **ROCProfiler SDK** 处理 HSA/HIP API 拦截（不是直接调用 HSA）
4. **缓存机制**：反汇编和提升结果按 `hsa_executable_t` 缓存；可执行文件被销毁时缓存失效
5. **线程安全**：每个 `LiftedRepresentation` 有独立的 `ThreadSafeContext`
6. **钩子限制**：只能插入到指令*之前*（不支持之后插入，避免终止符问题）
7. **错误处理**：全程使用 `llvm::Expected<T>` 和 `llvm::Error`
8. **不支持**：PAL 应用程序和 Windows

## 关键文件位置

| 组件 | 头文件 | 实现 |
|------|--------|------|
| 主 API | `include/luthier/luthier.h` | - |
| CodeLifter | `include/luthier/Tooling/CodeLifter.h` | `src/lib/ToolingCommon/CodeLifter.cpp` |
| LiftedRepresentation | `include/luthier/Tooling/LiftedRepresentation.h` | `src/lib/ToolingCommon/LiftedRepresentation.cpp` |
| CodeGenerator | `include/luthier/Tooling/CodeGenerator.h` | `src/lib/ToolingCommon/CodeGenerator.cpp` |
| InstrumentationTask | `include/luthier/Tooling/InstrumentationTask.h` | `src/lib/ToolingCommon/InstrumentationTask.cpp` |
| Context | `include/luthier/Tooling/Context.h` | `src/lib/ToolingCommon/Context.cpp` |
| HSA 符号 | `include/luthier/HSA/LoadedCodeObject*.h` | `src/lib/HSA/*.cpp` |
| Intrinsics | `include/luthier/Intrinsic/Intrinsics.h` | `src/lib/Intrinsic/*.cpp` |
