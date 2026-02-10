//===-- InstrumentationModule.h ---------------------------------*- C++ -*-===//
// InstrumentationModule.h 插桩模块头文件
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
/// This file describes Luthier's Instrumentation Module, which contains
/// an LLVM bitcode buffer as well as static variables loaded onto each GPU
/// device. The lifetime of an Instrumentation Module is managed by the
/// <tt>ToolExecutableLoader</tt>.
/// 此文件描述 Luthier 的插桩模块，它包含 LLVM 位码缓冲区以及加载到每个 GPU 设备上的静态变量。
/// 插桩模块的生命周期由 <tt>ToolExecutableLoader</tt> 管理
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_TOOLING_INSTRUMENTATION_MODULE_H
#define LUTHIER_TOOLING_INSTRUMENTATION_MODULE_H
#include "luthier/HSA/Agent.h"
#include "luthier/HSA/Executable.h"
#include "luthier/HSA/ExecutableSymbol.h"
#include "luthier/HSA/LoadedCodeObjectVariable.h"
#include "luthier/Rocprofiler/ApiTableSnapshot.h"
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/Error.h>
#include <optional>
#include <shared_mutex>
#include <string>

namespace luthier {

class ToolExecutableLoader;

//===----------------------------------------------------------------------===//
// Instrumentation Module
//===----------------------------------------------------------------------===//
// 插桩模块
//===----------------------------------------------------------------------===//

/// \brief Similar to HIP Modules in concept; Consists of an LLVM bitcode buffer
/// + All static variable addresses it uses on each GPU device
/// \brief 类似于 HIP 模块的概念；由 LLVM 位码缓冲区 + 它在每个 GPU 设备上使用的所有静态变量地址组成
class InstrumentationModule {
public:
  virtual ~InstrumentationModule() = default;
  /// Discriminator for LLVM-style RTTI (dyn_cast<> et al.)
  /// 用于 LLVM 风格 RTTI 的鉴别器 (dyn_cast<> 等)
  enum ModuleKind { MK_Static, MK_Dynamic };

protected:
  /// Only CodeObjectManager is allowed to create Instrumentation
  /// Modules
  /// 只有 CodeObjectManager 被允许创建插桩模块
  friend ToolExecutableLoader;

  explicit InstrumentationModule(ModuleKind Kind) : Kind(Kind) {};

  /// Compile Unit ID of the Module. This is an identifier generated
  /// by Clang to create a correspondence between the host and the device code.
  /// Presence of CUID is a requirement of all Luthier tool code
  /// 模块的编译单元 ID。这是 Clang 生成的标识符，用于在主机和设备代码之间建立对应关系。
  /// CUID 的存在是所有 Luthier 工具代码的要求
  std::string CUID{};

private:
  const ModuleKind Kind;

protected:
  /// List of static symbols without the agent information
  /// 不包含代理信息的静态符号列表
  llvm::SmallVector<std::string, 4> GlobalVariables{};

public:
  [[nodiscard]] ModuleKind getKind() const { return Kind; }

  /// Global Variable names iteration functions
  /// 全局变量名迭代函数

  using const_gv_names_iterator = decltype(GlobalVariables)::const_iterator;

  [[nodiscard]] const_gv_names_iterator gv_names_begin() const {
    return GlobalVariables.begin();
  }

  [[nodiscard]] const_gv_names_iterator gv_names_end() const {
    return GlobalVariables.end();
  }

  [[nodiscard]] llvm::iterator_range<const_gv_names_iterator> gv_names() const {
    return llvm::make_range(gv_names_begin(), gv_names_end());
  }

  [[nodiscard]] bool gv_names_empty() const { return GlobalVariables.empty(); }

  [[nodiscard]] size_t gv_names_size() const { return GlobalVariables.size(); }

  /// Reads the bitcode of this InstrumentationModule into a new
  /// \c llvm::Module backed by the \p Ctx
  /// \param Ctx an \c LLVMContext to back the returned Module
  /// \return an \c llvm::Module, or an \c llvm::Error if any problem was
  /// encountered during the process
  /// 将此 InstrumentationModule 的位码读入由 \p Ctx 支持的新 \c llvm::Module
  virtual llvm::Expected<std::unique_ptr<llvm::Module>>
  readBitcodeIntoContext(llvm::LLVMContext &Ctx, hsa_agent_t Agent) const = 0;

  /// Returns the loaded address of the global variable on the given \p Agent if
  /// already loaded, or \c std::nullopt if it is not loaded at the time of
  /// the query \n
  /// Mostly used when loading an instrumented executable
  /// \param GVName the name of the global variable queried
  /// \param Agent The \c hsa::GpuAgent to look for the global variable variable
  /// on
  /// \return A \c luthier::address_t if the variable was located on the \p
  /// Agent, an \c std::nullopt if not loaded, or an \c llvm::Error if an issue
  /// was encountered
  /// \sa luthier::hsa::Executable::defineExternalAgentGlobalVariable
  /// 如果已加载，返回给定 \p Agent 上全局变量的加载地址；如果查询时尚未加载，则返回 \c std::nullopt
  [[nodiscard]] virtual llvm::Expected<std::optional<luthier::address_t>>
  getGlobalVariablesLoadedOnAgent(llvm::StringRef GVName,
                                  hsa_agent_t Agent) const = 0;
};

//===----------------------------------------------------------------------===//
// Static Instrumentation Module
//===----------------------------------------------------------------------===//
// 静态插桩模块
//===----------------------------------------------------------------------===//

/// \brief Keeps track of instrumentation code loaded via a static HIP FAT
/// binary
/// \details an implementation of \c InstrumentationModule which keeps track of
/// <b>the</b> static HIP FAT binary embedded in the shared object of a Luthier
/// tool.\n
/// For now we anticipate that only a single Luthier tool will be loaded at any
/// given time; i.e. we don't think there is a case to instrument an already
/// instrumented GPU device code. \c ToolExecutableManager
/// enforces this by keeping a single instance of this variable, as
/// well as keeping its constructor private to itself. \n
/// Furthermore, If two or more Luthier tools are loaded then
/// \c StaticInstrumentationModule will detect it by checking the compile unit
/// ID of each executable passed to it.\n
/// For each GPU Agent, the HIP runtime extracts an ISA-compatible
/// code object from the static FAT binary and loads it into a single
/// executable. This is done in a lazy fashion if deferred loading is enabled,
/// meaning the loading only occurs on a device if the app starts using it. \n
/// \c StaticInstrumentationModule gets notified when a new \c hsa::Executable
/// of the FAT binary gets loaded onto each device. On the first occurrence,
/// it will record the CUID of the module, and creates a list of global
/// variables in the module, as well as their associated \c
/// hsa::ExecutableSymbol on the loaded \c hsa::GpuAgent. On subsequent
/// executable loads, it only updates the global variable list. It should be
/// clear by now that \c StaticInstrumentationModule does not do any GPU memory
/// management and relies solely on HIP for loading.\n A similar mechanism is in
/// place to detect unloading of the instrumentation module's executables; As
/// they get destroyed, the affected \c hsa::ExecutableSymbols get invalidated
/// as well.\n
/// \c StaticInstrumentationModule also gets notified of the kernel shadow host
/// pointers of each hook, and converts them to the correct hook name to
/// be found in the module later on.
/// \sa InstrumentationModule::BitcodeBuffer, LUTHIER_HOOK_ANNOTATE,
/// LUTHIER_EXPORT_HOOK_HANDLE
class StaticInstrumentationModule final : public InstrumentationModule {
private:
  friend ToolExecutableLoader;

  const rocprofiler::HsaExtensionTableSnapshot<HSA_EXTENSION_AMD_LOADER>
      &LoaderSnapshot;

  /// Private default constructor only accessible by \c
  /// ToolExecutableManager
  /// \c ToolExecutableManager 可访问的私有默认构造函数
  explicit StaticInstrumentationModule(
      const rocprofiler::HsaExtensionTableSnapshot<HSA_EXTENSION_AMD_LOADER>
          &LoaderSnapshot)
      : InstrumentationModule(MK_Static), LoaderSnapshot(LoaderSnapshot) {};

  /// Mutex to protect the contents of the static instrumentation module as
  /// it gets updated
  /// 互斥锁，用于保护静态插桩模块的内容（在更新时）
  mutable std::shared_mutex Mutex;

  /// A mapping between the bitcode extracted from each \c hsa_agent_t
  /// 从每个 \c hsa_agent_t 提取的位码之间的映射
  llvm::SmallDenseMap<hsa_agent_t, llvm::ArrayRef<char>, 8>
      PerAgentBitcodeBufferMap{};

  /// Each static HIP module gets loaded on each device as a single HSA
  /// executable \n
  /// This is a mapping from agents to said executables that belong to this
  /// static Module \n
  /// If HIP deferred loading is enabled, this map will be updated as the
  /// app utilizes multiple GPU devices and the HIP runtime loads the module on
  /// each utilized device \n
  /// Since HIP only loads a single LCO per executable, there's no need to save
  /// LCOs here
  /// 每个静态 HIP 模块作为单个 HSA 可执行文件加载到每个设备上
  llvm::DenseMap<hsa_agent_t, hsa_executable_t> PerAgentModuleExecutables{};

  /// Keeps track of the copies of the bitcode's global variables on each device
  /// 跟踪每个设备上位码的全局变量的副本
  llvm::DenseMap<
      hsa_agent_t,
      llvm::StringMap<std::unique_ptr<hsa::LoadedCodeObjectVariable>>>
      PerAgentGlobalVariables{};

  /// A mapping between the shadow host pointer of a hook and its name
  /// Gets updated whenever \c __hipRegisterFunction is called by
  /// \c ToolExecutableManager
  /// 钩子的阴影主机指针与其名称之间的映射
  llvm::DenseMap<const void *, llvm::StringRef> HookHandleMap{};

  /// Registers this executable into the static Instrumentation Module \n
  /// On first invocation this function extracts the bitcode in the ELF of
  /// \p Exec, and creates a list of global variables, as well as their
  /// \c hsa::ExecutableSymbol on the device the executable was loaded on \n
  /// On subsequent calls it only updates the global variable list for the
  /// new device \n
  /// This function is only called by \c ToolExecutableManager whenever
  /// it confirms a newly frozen executable is a copy of a Luthier
  /// static FAT binary for instrumentation
  /// \param Exec the static Luthier tool executable that was just frozen by
  /// the HIP runtime
  /// \return an \c llvm::Error if any issues were encountered during the
  /// process
  /// 将此可执行文件注册到静态插桩模块
  llvm::Error registerExecutable(hsa_executable_t Exec);

  /// Unregisters the executable from the Module \n
  /// As this function gets invoked for each executable on the device
  /// the instrumentation module was loaded on, the internal global variable
  /// list removes the defunct \c hsa::ExecutableSymbols. When the last
  /// executable of this module gets destroyed, the bitcode is wiped as well
  /// as any other internal state \n
  /// This function is only called by \c ToolExecutableManager whenever
  /// it confirms an executable that is about to be destroyed is a copy of
  /// a Luthier static FAT binary for instrumentation
  /// \param Exec handle to the module executable about to be destroyed
  /// \return an \c llvm::Error if any issue was encountered during the process
  /// 从模块中取消注册可执行文件
  llvm::Error unregisterExecutable(hsa_executable_t Exec);

  /// Same as \c getLCOGlobalVariableOnAgent except with no lock
  [[nodiscard]] llvm::Expected<const hsa::LoadedCodeObjectVariable *>
  getLCOGlobalVariableOnAgentNoLock(llvm::StringRef GVName,
                                    hsa_agent_t Agent) const;

public:
  [[nodiscard]] llvm::Expected<std::optional<luthier::address_t>>
  getGlobalVariablesLoadedOnAgent(llvm::StringRef GVName,
                                  hsa_agent_t Agent) const override;

  [[nodiscard]] llvm::Expected<std::unique_ptr<llvm::Module>>
  readBitcodeIntoContext(llvm::LLVMContext &Ctx,
                         hsa_agent_t Agent) const override;

  /// Same as <tt>getGlobalVariablesLoadedOnAgent</tt>,
  /// except it returns the ExecutableSymbol of the variables
  /// Use this function only if \c getGlobalVariablesLoadedOnAgent does not
  /// provide sufficient information.
  /// \param Agent The \c hsa::GpuAgent where a copy (executable) of this module
  /// is loaded
  /// \return a const reference to the mapping between variable names and their
  /// Executable Symbols, or an \c llvm::Error if an issue is encountered
  /// 与 <tt>getGlobalVariablesLoadedOnAgent</tt> 相同，只是它返回变量的 ExecutableSymbol
  [[nodiscard]] llvm::Expected<const hsa::LoadedCodeObjectVariable *>
  getLCOGlobalVariableOnAgent(llvm::StringRef GVName, hsa_agent_t Agent) const;

  /// Converts the shadow host pointer \p Handle to the name of the hook it
  /// represents
  /// \param Handle Shadow host pointer of the hook handle
  /// \return the name of the hook \c llvm::Function, or and \c llvm::Error if
  /// the \p Handle doesn't exist
  /// 将阴影主机指针 \p Handle 转换为其所表示的钩子名称
  llvm::Expected<llvm::StringRef>
  convertHookHandleToHookName(const void *Handle) const;

  /// A helper function which detects if the passed executable is part of the
  /// static instrumentation module. \n
  /// Used by \c ToolExecutableManager to detect and register/unregister
  /// static instrumentation executables
  /// \param Exec an \c hsa::Executable
  /// \return \c true if this is a static instrumentation module copy, false if
  /// not, or an \c llvm::Error if any issues were encountered during the
  /// process
  /// 辅助函数，用于检测传递的可执行文件是否属于静态插桩模块
  static llvm::Expected<bool> isStaticInstrumentationModuleExecutable(
      const hsa::ApiTableContainer<::CoreApiTable> &CoreApi,
      const hsa_ven_amd_loader_1_03_pfn_t &LoaderApi, hsa_executable_t Exec);

  static bool classof(const InstrumentationModule *IM) {
    return IM->getKind() == MK_Static;
  }
};
} // namespace luthier
#endif