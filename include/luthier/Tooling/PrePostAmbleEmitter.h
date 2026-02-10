//===-- PrePostAmbleEmitter.h -----------------------------------*- C++ -*-===//
// PrePostAmbleEmitter.h 前后导码发射器头文件
// Copyright 2022-2025 @ Northeastern University Computer Architecture Lab
//
// Licensed under the Apache License, Version 2.0 (the "License");
// 您可以在遵守许可证的情况下使用此文件
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===----------------------------------------------------------------------===//
///
/// \file
/// This file describes the Pre and post amble emitter,
/// which will emits code before and after
/// using the information gathered from code gen passes when generating
/// the hooks. It also describes the \c FunctionPreambleDescriptor and its
/// analysis pass, which describes the preamble specs for each function
/// inside the target application.
/// 此文件描述前后导码发射器，它使用从代码生成传递中收集的信息在生成钩子时
/// 发出之前和之后的代码。它还描述了 \c FunctionPreambleDescriptor 及其
/// 分析传递，描述了目标应用程序中每个函数的导码规范
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_TOOLING_PRE_POST_AMBLE_EMITTER_H
#define LUTHIER_TOOLING_PRE_POST_AMBLE_EMITTER_H
#include "luthier/HSA/LoadedCodeObjectDeviceFunction.h"
#include "luthier/HSA/LoadedCodeObjectKernel.h"
#include "luthier/Intrinsic/IntrinsicProcessor.h"
#include "luthier/Tooling/LiftedRepresentation.h"
#include <llvm/ADT/DenseSet.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/Support/Error.h>

namespace luthier {

class SVStorageAndLoadLocations;

class LiftedRepresentation;

/// \brief a struct which aggregates information about the preamble code
/// required to be emitted for each function inside a \c LiftedRepresentation
/// 聚合 \c LiftedRepresentation 中每个函数所需的导码信息的结构体
struct FunctionPreambleDescriptor {
  /// \brief struct describing the specifications of the preamble code for
  /// each kernel inside the \c LR
  /// 描述 \c LR 中每个内核的导码规范的结构体
  typedef struct KernelPreambleSpecs {
    [[nodiscard]] bool usesSVA() const {
      return RequiresScratchAndStackSetup ||
             RequestedAdditionalStackSizeInBytes ||
             !RequestedKernelArguments.empty();
    }
    /// Whether the preamble requires setting up scratch and an instrumentation
    /// stack
    /// 导码是否需要设置scratch和插桩栈
    bool RequiresScratchAndStackSetup{false};
    /// Number of bytes of scratch space requested on top of the application
    /// stack; This value is hard coded in the preamble assembly code
    /// 应用程序栈之上请求的scratch空间字节数；该值硬编码在导码汇编代码中
    unsigned int RequestedAdditionalStackSizeInBytes{0};
    /// A set of kernel arguments that are accessed by the injected payload
    /// functions
    /// 注入的有效负载函数访问的内核参数集合
    llvm::SmallDenseSet<KernelArgumentType, 8> RequestedKernelArguments{};
  } KernelPreambleSpecs;

  /// \brief struct describing the specifications of the preamble code for
  /// each kernel inside the \c LR
  /// 描述 \c LR 中每个内核的导码规范的结构体
  typedef struct DeviceFunctionPreambleSpecs {
    /// Whether or not any hooks inside the device function access the
    /// state value array
    /// 设备函数内部的钩子是否访问状态值数组
    bool UsesStateValueArray{false};
    /// Indicates if the device function requires additional code before and
    /// and after it to pop/push the state value array off of the application
    /// stack
    /// 指示设备函数是否需要在其前后添加额外代码以将状态值数组弹出/推入应用程序栈
    bool RequiresPreAndPostAmble{false};
    /// Whether the device function makes use of stack/scratch
    /// 设备函数是否使用栈/scratch
    bool RequiresScratchAndStackSetup{false};
    /// A set of kernel arguments accessed by the device function injected
    /// payloads
    /// 设备函数注入的有效负载访问的内核参数集合
    llvm::SmallDenseSet<KernelArgumentType, 8> RequestedKernelArguments{};
  } DeviceFunctionPreambleSpecs;

  FunctionPreambleDescriptor(const llvm::MachineModuleInfo &TargetMMI,
                             const llvm::Module &TargetModule);

  /// preamble specs for each kernel inside the \c LR
  /// \c LR 中每个内核的导码规范
  llvm::SmallDenseMap<const llvm::MachineFunction *, KernelPreambleSpecs, 4>
      Kernels{};

  /// pre/post amble specs for each device function inside the \c LR
  /// \c LR 中每个设备函数的前后导码规范
  llvm::SmallDenseMap<const llvm::MachineFunction *,
                      DeviceFunctionPreambleSpecs, 4>
      DeviceFunctions{};

  /// Never invalidate the results
  /// 永远不使结果失效
  bool invalidate(llvm::Module &, const llvm::PreservedAnalyses &,
                  llvm::ModuleAnalysisManager::Invalidator &) {
    return false;
  }
};

class FunctionPreambleDescriptorAnalysis
    : public llvm::AnalysisInfoMixin<FunctionPreambleDescriptorAnalysis> {
  friend llvm::AnalysisInfoMixin<FunctionPreambleDescriptorAnalysis>;

  static llvm::AnalysisKey Key;

public:
  FunctionPreambleDescriptorAnalysis() = default;

  using Result = FunctionPreambleDescriptor;

  Result run(llvm::Module &TargetModule,
             llvm::ModuleAnalysisManager &TargetMAM);
};

class PrePostAmbleEmitter : public llvm::PassInfoMixin<PrePostAmbleEmitter> {

public:
  explicit PrePostAmbleEmitter() = default;

  llvm::PreservedAnalyses run(llvm::Module &TargetModule,
                              llvm::ModuleAnalysisManager &TargetMAM);
};

} // namespace luthier

#endif