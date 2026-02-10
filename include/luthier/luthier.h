//===-- luthier.h - Luthier High-level Interface  ---------------*- C++ -*-===//
// Copyright 2022-2025 @ Northeastern University Computer Architecture Lab
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===----------------------------------------------------------------------===//
//
/// \file
/// \brief 本文件定义了 Luthier 的面向公众的接口。
/// This file defines the public-facing interface of Luthier.
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_H
#define LUTHIER_H
/// HIP_ENABLE_WARP_SYNC_BUILTINS 启用 HIP 中的 warp sync 内置函数
/// 必须在包含 HIP 运行时头文件之前定义
/// HIP_ENABLE_WARP_SYNC_BUILTINS enables the warp sync built-ins in HIP
/// Must be defined before HIP runtime headers are included
#define HIP_ENABLE_WARP_SYNC_BUILTINS
#include "luthier/Common/ErrorCheck.h"
#include "luthier/consts.h"
#include <llvm/Support/Error.h>
/// 取消定义 HIP 头文件中定义不当的 \c ICMP_NE
/// Undef the ill-defined \c ICMP_NE in HIP headers
#undef ICMP_NE
#include "luthier/HSA/Instr.h"
#include "luthier/HSA/KernelDescriptor.h"
#include "luthier/HSA/LoadedCodeObjectKernel.h"
#include "luthier/HSA/LoadedCodeObjectSymbol.h"
#include "luthier/Intrinsic/Intrinsics.h"
#include "luthier/Tooling/InstrumentationTask.h"
#include "luthier/Tooling/LiftedRepresentation.h"
#include "luthier/types.h"

namespace luthier {

//===----------------------------------------------------------------------===//
//  检查 API
//  Inspection APIs
//===----------------------------------------------------------------------===//

/// 将 \p Kernel 反汇编为 <tt>hsa::Instr</tt> 的列表\n
/// 反汇编仅在此函数首次对 \p Kernel 调用时发生。后续调用将使用内部缓存的结果\n
/// \note 此函数仅提供指令的原始 LLVM MC 视图；要进行插桩，请改用 <tt>lift</tt>
/// \param Kernel 要反汇编的内核符号
/// \return 指向内部缓存的 <tt>hsa::Instr</tt> 向量的 \c llvm::ArrayRef，或过程中遇到问题时返回 \c llvm::Error
/// Disassembles the \p Kernel into a list of <tt>hsa::Instr</tt>\n
/// Disassembly only occurs on the first time this function is invoked on
/// the \p Kernel. Subsequent calls will use a result cached internally\n
/// \note This function only provides a raw LLVM MC view of the instructions;
/// For instrumentation, use <tt>lift</tt> instead
/// \param Kernel the kernel symbol to be disassembled
/// \return an \c llvm::ArrayRef to an internally cached vector of
/// <tt>hsa::Instr</tt>s, or an \c llvm::Error if an issue was encountered
/// during the process
llvm::Expected<llvm::ArrayRef<hsa::Instr>>
disassemble(const hsa::LoadedCodeObjectKernel &Kernel);

/// 将 \p Func 反汇编为 <tt>hsa::Instr</tt> 的列表。\n
/// 反汇编仅在此函数首次对 \p Func 调用时发生。后续调用将使用内部缓存的结果。\n
/// \note 此函数仅提供指令的原始 LLVM MC 视图；要进行插桩，请改用 <tt>lift</tt>
/// \param Func 要反汇编的设备函数
/// \return 指向内部缓存的 <tt>hsa::Instr</tt> 向量的 \c llvm::ArrayRef，或过程中遇到问题时返回 \c llvm::Error
/// Disassembles the \p Func into a list of <tt>hsa::Instr</tt>.\n
/// Disassembly only occurs on the first time this function is invoked on
/// \p Func. Subsequent calls will use a result cached internally.\n
/// \note This function only provides a raw LLVM MC view of the instructions;
/// For instrumentation, use <tt>lift</tt> instead
/// \param Func the device function to be disassembled
/// \return an \c llvm::ArrayRef to an internally cached vector of
/// <tt>hsa::Instr</tt>s, or an \c llvm::Error if an issue was encountered
/// during the process
llvm::Expected<llvm::ArrayRef<hsa::Instr>>
disassemble(const hsa::LoadedCodeObjectDeviceFunction &Func);

/// 提升给定的 \p Kernel 并返回其 <tt>LiftedRepresentation</tt> 的引用。\n
/// 提升结果在首次调用时内部缓存。\n
/// \param [in] Kernel 要提升的 \c hsa::LoadedCodeObjectKernel
/// \return 成功时返回内部缓存的 <tt>LiftedRepresentation</tt> 的引用，或描述遇到问题的 \c llvm::Error
/// \sa LiftedRepresentation, \sa lift
/// Lifts the given \p Kernel and return a reference to its
/// <tt>LiftedRepresentation</tt>.\n
/// The lifted result gets cached internally on the first invocation.
/// \param [in] Kernel an \c hsa::LoadedCodeObjectKernel to be lifted
/// \return a reference to the internally-cached <tt>LiftedRepresentation</tt>
/// if successful, or an \c llvm::Error describing the issue encountered.
/// \sa LiftedRepresentation, \sa lift
llvm::Expected<const luthier::LiftedRepresentation &>
lift(const hsa::LoadedCodeObjectKernel &Kernel);

//===----------------------------------------------------------------------===//
//  插桩 API
//  Instrumentation API
//===----------------------------------------------------------------------===//

/// 通过对 \p LR 应用 \p Mutator 来对 \p LR 进行插桩
/// \param LR 要被插桩的 \c LiftedRepresentation
/// \param Mutator 对 \p LR 进行插桩和修改的函数
/// \return 返回包含插桩代码的新 \c LiftedRepresentation，或插桩过程中遇到问题时返回 \c llvm::Error
/// Instruments the \p LR by applying the \p Mutator to it
/// \param LR the \c LiftedRepresentation about to be instrumented
/// \param Mutator a function that instruments and modifies the \p LR
/// \return returns a new \c LiftedRepresentation containing the instrumented
/// code, or an \c llvm::Error if an issue was encountered during
/// instrumentation
llvm::Expected<std::unique_ptr<LiftedRepresentation>>
instrument(const LiftedRepresentation &LR,
           llvm::function_ref<llvm::Error(InstrumentationTask &,
                                          LiftedRepresentation &)>
               Mutator);

/// 对 \p LR 应用汇编打印 pass，为其每个 <tt>llvm::Module</tt> 和 <tt>llvm::MachineModuleInfo</tt> 生成目标文件或汇编文件
/// \note 打印后，所有 <tt>LR</tt> 的 <tt>llvm::MachineModuleInfo</tt> 将被删除；这是由于 LLVM 的设计缺陷，正在努力解决
/// \param [in] LR 要打印到汇编文件的 \c LiftedRepresentation；其 \c llvm::TargetMachine 可用于控制编译过程的 \c llvm::TargetOptions
/// \param [out] CompiledObjectFiles 打印的汇编文件
/// \param [in] FileType 打印的汇编文件的类型；可以是 \c llvm::CodeGenFileType::AssemblyFile 或 \c llvm::CodeGenFileType::ObjectFile
/// \return 如果过程中遇到任何问题返回 \c llvm::Error
/// Applies the assembly printer pass on the \p LR to generate object files or
/// assembly files for each of its <tt>llvm::Module</tt>s and
/// <tt>llvm::MachineModuleInfo</tt>s
/// \note After printing, and all of the <tt>LR</tt>'s
/// <tt>llvm::MachineModuleInfo</tt>s will be deleted; This is due to an LLVM
/// design shortcoming which is being worked on
/// \param [in] LR the \c LiftedRepresentation to be printed into an assembly
/// file; Its \c llvm::TargetMachine can be used to control the
/// \c llvm::TargetOptions of the compilation process
/// \param [out] CompiledObjectFiles the printed assembly file
/// \param [in] FileType Type of the assembly file printed; Can be either
/// \c llvm::CodeGenFileType::AssemblyFile or
/// \c llvm::CodeGenFileType::ObjectFile
/// \return an \c llvm::Error in case of any issues encountered during the
/// process
llvm::Error printLiftedRepresentation(
    LiftedRepresentation &LR, llvm::SmallVectorImpl<char> &CompiledObjectFile,
    llvm::CodeGenFileType FileType = llvm::CodeGenFileType::ObjectFile);

// TODO: Implement link to executable, and load methods individually +
//  update the instrumentAndLoad docs

/// 通过对 <tt>Kernel</tt> 的提升表示 \p LR 应用插桩任务 <tt>ITask</tt> 来对其进行插桩。\n
/// 插桩后，将插桩后的代码加载到与 \p Kernel 相同的设备上
/// \param Kernel 即将被插桩的内核
/// \param LR \p Kernel 的提升表示
/// \param ITask 描述要对 <tt>kernel</tt> 的 <tt>LR</tt> 执行的插桩任务的插桩任务
/// \return 描述操作成功或失败的 \c llvm::Error
/// Instruments the <tt>Kernel</tt>'s lifted representation \p LR by
/// applying the instrumentation task <tt>ITask</tt> to it.\n After
/// instrumentation, loads the instrumented code onto the same device as the
/// \p Kernel
/// \param Kernel the kernel that's about to be instrumented
/// \param LR the lifted representation of the \p Kernel
/// \param ITask the instrumentation task, describing the instrumentation to
/// be performed on the <tt>kernel</tt>'s <tt>LR</tt>
/// \return an \c llvm::Error describing if the operation succeeded or
/// failed
llvm::Error
instrumentAndLoad(const hsa::LoadedCodeObjectKernel &Kernel,
                  const LiftedRepresentation &LR,
                  llvm::function_ref<llvm::Error(InstrumentationTask &,
                                                 LiftedRepresentation &)>
                      Mutator,
                  llvm::StringRef Preset);

/// 检查 \p Kernel 是否在给定的 \p Preset 下被插桩
/// \param [in] 应用的 \c hsa::LoadedCodeObjectKernel
/// \param [in] 内核被插桩的预设名称
/// \return 成功时返回 \c true（如果 \p Kernel 被插桩），否则返回 \c false。如果 \p Kernel HSA 符号句柄无效，返回 \c llvm::Error
/// Checks if the \p Kernel is instrumented under the given \p Preset or not
/// \param [in] Kernel the \c hsa::LoadedCodeObjectKernel of the app
/// \param [in] Preset the preset name the kernel was instrumented under
/// \return on success, returns \c true if the \p Kernel is instrumented, \c
/// false otherwise. Returns an \c llvm::Error if the \p Kernel HSA symbol
/// handle is invalid
llvm::Expected<bool>
isKernelInstrumented(const hsa::LoadedCodeObjectKernel &Kernel,
                     llvm::StringRef Preset);

/// 用给定 \p Preset 下的插桩版本覆盖数据包的内核对象字段，强制 HSA 启动插桩版本而不是原始版本\n
/// 如需要，修改其余启动配置（如私有段大小）\n
/// 请注意，每当需要启动插桩的内核时都应调用此函数，因为调度数据包的内容总是被目标应用程序设置为原始的未插桩版本\n
/// 要启动内核的原始版本，只需不调用此函数
/// \param Packet 从 HSA 队列拦截的 HSA 调度数据包，包含内核启动参数/配置
/// \param Preset 内核被插桩的预设
/// \return 报告错误的 \c llvm::Error
/// Overrides the kernel object field of the Packet with its instrumented
/// version under the given \p Preset, forcing HSA to launch the
/// instrumented version instead\n Modifies the rest of the launch
/// configuration (e.g. private segment size) if needed\n Note that this
/// function should be called every time an instrumented kernel needs to be
/// launched, since the content of the dispatch packet will always be set by
/// the target application to the original, un-instrumented version\n To
/// launch the original version of the kernel, simply refrain from calling
/// this function
/// \param Packet Packet the HSA dispatch packet intercepted from an HSA
/// queue,
/// containing the kernel launch parameters/configuration
/// \param Preset the preset the kernel was instrumented under
/// \return an \c llvm::Error reporting
llvm::Error overrideWithInstrumented(hsa_kernel_dispatch_packet_t &Packet,
                                     llvm::StringRef Preset);

/// \brief 如果工具包含插桩钩子，它\b必须使用此宏一次。Luthier 钩子通过 \p LUTHIER_HOOK_CREATE 宏进行注解。\n
///
/// \p MARK_LUTHIER_DEVICE_MODULE 宏在工具设备代码中定义一个类型为 \p char、名为 \p __luthier_reserved 的托管变量。
/// 此托管变量确保：\n
/// 1. <b>HIP 运行时被迫在设备上启动第一个 HIP 内核之前加载工具代码对象，而无需启用急切二进制加载</b>：Clang 编译器将 Luthier 工具及其位码的设备代码嵌入到工具共享对象中捆绑的静态 HIP FAT 二进制文件中。在运行时，工具的 FAT 二进制文件向 HIP 运行时注册；但是，默认情况下，HIP 运行时以惰性方式加载 FAT 二进制文件，仅在以下情况下将其加载到设备：
/// a. 从所述设备上的内核启动，或
/// b. 它包含托管变量。\n
/// 包含托管变量是确保工具的 FAT 二进制文件及时加载而不干扰 HIP 运行时加载机制的唯一方法。
/// \n
/// 2. <b>Luthier 可以通过常量时间符号哈希查找轻松识别工具的代码对象</b>。
/// \n
/// 如果目标应用程序没有使用 HIP 运行时，则 HIP 运行时不会启动任何内核，这意味着工具的 FAT 二进制文件永远不会被加载。在这种情况下，由于 HIP 运行时仅用于 Luthier 的功能，必须将 `HIP_ENABLE_DEFERRED_LOADING` 环境变量设置为零，以确保 Luthier 工具代码对象立即在所有设备上加载。
/// \sa LUTHIER_HOOK_ANNOTATE
/// \brief If a tool contains an instrumentation hook it \b must
/// use this macro once. Luthier hooks are annotated via the the
/// \p LUTHIER_HOOK_CREATE macro. \n
///
/// \p MARK_LUTHIER_DEVICE_MODULE macro defines a managed variable of
/// type \p char named \p __luthier_reserved in the tool device code.
/// This managed variable ensures that: \n
/// 1. <b>The HIP runtime is forced to load the tool code object before the
/// first HIP kernel is launched on the device, without requiring eager binary
/// loading to be enabled</b>: The Clang compiler embeds the device code of a
/// Luthier tool and its bitcode into a static HIP FAT binary bundled within the
/// tool's shared object. During runtime, the tool's FAT binary gets
/// registered with the HIP runtime; However, by default, the HIP runtime loads
/// FAT binaries in a lazy fashion, only loading it onto a device if:
/// a. a kernel is launched from it on the said device, or
/// b. it contains a managed variable. \n
/// Including a managed variable is the only way to ensure the tool's FAT binary
/// is loaded in time without interfering with the loading mechanism of HIP
/// runtime.
/// \n
/// 2. <b>Luthier can easily identify a tool's code object by a constant time
/// symbol hash lookup</b>.
/// \n
/// If the target application is not using the HIP runtime, then no kernel is
/// launched by the HIP runtime, meaning that the tool FAT binary does not ever
/// get loaded. In that scenario, as the HIP runtime is present solely for
/// Luthier's function, the `HIP_ENABLE_DEFERRED_LOADING` environment
/// variable must be set to zero to ensure Luthier tool code objects get loaded
/// right away on all devices.
/// \sa LUTHIER_HOOK_ANNOTATE
#define MARK_LUTHIER_DEVICE_MODULE                                             \
  __attribute__((managed, used)) char LUTHIER_RESERVED_MANAGED_VAR = 0;

#define LUTHIER_HOOK_ANNOTATE                                                  \
  __attribute__((                                                              \
      device, used,                                                            \
      annotate(LUTHIER_STRINGIFY(LUTHIER_HOOK_ATTRIBUTE)))) extern "C" void

#define LUTHIER_EXPORT_HOOK_HANDLE(HookName)                                   \
  __attribute__((global, used)) extern "C" void LUTHIER_CAT(                   \
      LUTHIER_HOOK_HANDLE_PREFIX, HookName)(){};

#define LUTHIER_GET_HOOK_HANDLE(HookName)                                      \
  reinterpret_cast<const void *>(                                              \
      LUTHIER_CAT(LUTHIER_HOOK_HANDLE_PREFIX, HookName))
} // namespace luthier

#endif
