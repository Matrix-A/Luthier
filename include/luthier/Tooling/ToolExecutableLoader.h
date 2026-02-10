//===-- ToolExecutableLoader.h ----------------------------------*- C++ -*-===//
// ToolExecutableLoader.h 工具可执行文件加载器头文件
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
/// Describes Luthier's Tool Executable Loader Singleton, in charge of:
/// - Managing all loaded instrumentation modules loaded automatically or
/// manually
/// - The lifetime of the instrumented executables
/// - Providing the instrumented versions of the original kernels
/// 描述 Luthier 的工具可执行文件加载器单例，负责：
/// - 管理自动或手动加载的所有插桩模块
/// - 插桩可执行文件的生命周期
/// - 提供原始内核的插桩版本
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_TOOLING_TOOL_EXECUTABLE_LOADER_H
#define LUTHIER_TOOLING_TOOL_EXECUTABLE_LOADER_H
#include "luthier/Common/Singleton.h"
#include "luthier/HSA/Agent.h"
#include "luthier/HSA/CodeObjectReader.h"
#include "luthier/HSA/Executable.h"
#include "luthier/HSA/ExecutableSymbol.h"
#include "luthier/HSA/LoadedCodeObject.h"
#include "luthier/HSA/LoadedCodeObjectKernel.h"
#include "luthier/Rocprofiler/ApiTableWrapperInstaller.h"
#include "luthier/Tooling/InstrumentationModule.h"
#include "luthier/types.h"
#include <hip/amd_detail/amd_hip_vector_types.h>
#include <hip/hip_runtime.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>
#include <vector>

namespace luthier {
namespace hsa {
class LoadedCodeObjectCache;
}

/// \brief A singleton object that keeps track of executables that belong to
/// Luthier, including instrumented executables and tool
/// instrumentation modules, plus launching instrumented kernels
/// \brief 单例对象，跟踪属于 Luthier 的可执行文件，包括插桩可执行文件和工具
/// 插桩模块，以及启动插桩内核
class ToolExecutableLoader : public Singleton<ToolExecutableLoader> {
private:
  /// Mutex to protect internal state of the loader
  /// 用于保护加载器内部状态的互斥锁
  std::recursive_mutex Mutex;

  /// Table snapshot used to invoke HSA core operations
  /// 用于调用 HSA 核心操作的表快照
  const rocprofiler::HsaApiTableSnapshot<::CoreApiTable> &CoreApiSnapshot;

  /// Table snapshot used to invoke HSA loader operations
  /// 用于调用 HSA 加载器操作的表快照
  const rocprofiler::HsaExtensionTableSnapshot<HSA_EXTENSION_AMD_LOADER>
      &LoaderApiSnapshot;

  /// Used to install wrappers for executable freeze/destroy functions
  /// 用于安装可执行文件冻结/销毁函数的包装器
  std::unique_ptr<
      const rocprofiler::HsaApiTableWrapperInstaller<::CoreApiTable>>
      CoreApiWrapperInstaller;

  /// Used to install a wrapper for __hipRegisterFunction
  /// 用于安装 __hipRegisterFunction 的包装器
  std::unique_ptr<const rocprofiler::HipCompilerApiTableWrapperInstaller>
      HipCompilerWrapperInstaller;

  /// Reference to the code object cache used to support loading/unloading of
  /// instrumented kernels
  /// 用于支持插桩内核加载/卸载的代码对象缓存的引用
  const hsa::LoadedCodeObjectCache &COC;

  /// The single static instrumentation module included in Luthier tool
  /// Luthier 工具中包含的单个静态插桩模块
  mutable StaticInstrumentationModule SIM;

  const amdgpu::hsamd::MetadataParser &MDParser;

  llvm::DenseMap<hsa_executable_symbol_t,
                 std::unique_ptr<amdgpu::hsamd::Kernel::Metadata>>
      InstrumentedKernelMetadata{};

  llvm::DenseMap<hsa_executable_t, llvm::DenseSet<hsa_executable_t>>
      OriginalExecutablesWithKernelsInstrumented{};

  static t___hipRegisterFunction UnderlyingHipRegisterFn;

  static decltype(hsa_executable_freeze) *UnderlyingHsaExecutableFreezeFn;

  static decltype(hsa_executable_destroy) *UnderlyingHsaExecutableDestroyFn;

  static void
  hipRegisterFunctionWrapper(void **modules, const void *hostFunction,
                             char *deviceFunction, const char *deviceName,
                             unsigned int threadLimit, uint3 *tid, uint3 *bid,
                             dim3 *blockDim, dim3 *gridDim, int *wSize);

  static hsa_status_t hsaExecutableFreezeWrapper(hsa_executable_t Executable,
                                                 const char *Options);

  static hsa_status_t hsaExecutableDestroyWrapper(hsa_executable_t Executable);

public:
  ToolExecutableLoader(
      const rocprofiler::HsaApiTableSnapshot<::CoreApiTable> &CoreApiSnapshot,
      const rocprofiler::HsaExtensionTableSnapshot<HSA_EXTENSION_AMD_LOADER>
          &LoaderApiSnapshot,
      const hsa::LoadedCodeObjectCache &COC,
      const amdgpu::hsamd::MetadataParser &MDParser, llvm::Error &Err);

  /// Loads a list of instrumented code objects into a new executable and
  /// freezes it, allowing the instrumented version of the \p OriginalKernel
  /// to run on its own
  /// This is useful for when the user wants to instrumentAndLoad a single
  /// kernel
  /// \param InstrumentedElfs a list of instrumented code objects that isolate
  /// the requirements of \p OriginalKernel in a single executable
  /// \param OriginalKernel the \c hsa::ExecutableSymbol of the original kernel
  /// \param Preset the preset name of the instrumentation
  /// \param ExternVariables a mapping between the name and the address of
  /// external variables of the instrumented code objects
  /// \return an \p llvm::Error if an issue was encountered in the process
  /// 将插桩代码对象列表加载到新的可执行文件并冻结它，允许 \p OriginalKernel
  /// 的插桩版本独立运行
  llvm::Error
  loadInstrumentedKernel(llvm::ArrayRef<uint8_t> InstrumentedElfs,
                         const hsa::LoadedCodeObjectKernel &OriginalKernel,
                         llvm::StringRef Preset,
                         const llvm::StringMap<const void *> &ExternVariables);

  /// Returns the instrumented kernel's \c hsa::ExecutableSymbol given its
  /// original un-instrumented version's \c hsa::ExecutableSymbol and the
  /// preset name it was instrumented under \n
  /// Used to run the instrumented version of the kernel when requested by the
  /// user
  /// \param OriginalKernel symbol of the un-instrumented original kernel
  /// \return symbol of the instrumented version of the target kernel, or
  /// \p llvm::Error
  /// 给定原始未插桩版本的 \c hsa::ExecutableSymbol 及其插桩的预设名称，
  /// 返回插桩内核的 \c hsa::ExecutableSymbol
  [[nodiscard]] llvm::Expected<std::pair<
      hsa_executable_symbol_t, const amdgpu::hsamd::Kernel::Metadata &>>
  getInstrumentedKernel(hsa_executable_symbol_t OriginalKernel,
                        llvm::StringRef Preset) const;

  /// Checks if the given \p Kernel is instrumented under the given \p Preset
  /// \return \c true if it's instrumented, \c false otherwise
  /// 检查给定的 \p Kernel 是否在给定的 \p Preset 下被插桩
  [[nodiscard]] bool
  isKernelInstrumented(const hsa::LoadedCodeObjectKernel &Kernel,
                       llvm::StringRef Preset) const;

  [[nodiscard]] const StaticInstrumentationModule &
  getStaticInstrumentationModule() const {
    return SIM;
  }

  ~ToolExecutableLoader() override;

private:
  void insertInstrumentedKernelIntoMap(
      const hsa_executable_t OriginalExecutable,
      const hsa_executable_symbol_t OriginalKernel, llvm::StringRef Preset,
      const hsa_executable_t InstrumentedExecutable,
      const hsa_executable_symbol_t InstrumentedKernel) {
    // Create an entry for the OriginalKernel if it doesn't already exist in the
    // map
    auto OriginalKernelEntry =
        OriginalToInstrumentedKernelsMap.find(OriginalKernel);
    if (OriginalKernelEntry == OriginalToInstrumentedKernelsMap.end()) {
      OriginalKernelEntry =
          OriginalToInstrumentedKernelsMap
              .emplace(OriginalKernel,
                       llvm::StringMap<hsa_executable_symbol_t>{})
              .first;
    }
    OriginalKernelEntry->second.insert({Preset, InstrumentedKernel});
    auto OriginalExecutableEntry =
        OriginalExecutablesWithKernelsInstrumented.find(OriginalExecutable);
    if (OriginalExecutableEntry ==
        OriginalExecutablesWithKernelsInstrumented.end()) {
      OriginalExecutableEntry = OriginalExecutablesWithKernelsInstrumented
                                    .insert({OriginalExecutable, {}})
                                    .first;
    }
    OriginalExecutableEntry->second.insert(InstrumentedExecutable);
  }
  /// \brief a mapping between the pair of an instrumented kernel, given
  /// its original kernel, and its instrumentation preset
  /// 插桩内核与其原始内核及其插桩预设之间的映射
  std::unordered_map<hsa_executable_symbol_t,
                     llvm::StringMap<hsa_executable_symbol_t>>
      OriginalToInstrumentedKernelsMap{};
};
}; // namespace luthier

#endif
