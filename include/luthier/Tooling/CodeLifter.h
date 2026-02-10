//===-- CodeLifter.h - Luthier's Code Lifter  -------------------*- C++ -*-===//
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
// \file
/// \brief 本文件描述了 Luthier 的代码提升器，这是一个单例类，负责将代码对象反汇编为 MC 和 MIR 表示。
/// This file describes Luthier's Code Lifter, a singleton in charge of
/// disassembling code objects into MC and MIR representations.
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_TOOLING_CODE_LIFTER_H
#define LUTHIER_TOOLING_CODE_LIFTER_H
#include "AMDGPUTargetMachine.h"
#include "luthier/Common/Singleton.h"
#include "luthier/HSA/Agent.h"
#include "luthier/HSA/Executable.h"
#include "luthier/HSA/ExecutableSymbol.h"
#include "luthier/HSA/ISA.h"
#include "luthier/HSA/Instr.h"
#include "luthier/HSA/LoadedCodeObject.h"
#include "luthier/HSA/LoadedCodeObjectCache.h"
#include "luthier/HSA/LoadedCodeObjectDeviceFunction.h"
#include "luthier/HSA/LoadedCodeObjectKernel.h"
#include "luthier/HSA/hsa.h"
#include "luthier/LLVM/Cloning.h"
#include "luthier/Object/AMDGCNObjectFile.h"
#include "luthier/Object/ObjectFileUtils.h"
#include "luthier/Rocprofiler/ApiTableSnapshot.h"
#include "luthier/Tooling/LiftedRepresentation.h"
#include "luthier/Tooling/TargetManager.h"
#include "luthier/types.h"
#include <functional>
#include <llvm/ADT/DenseMap.h>
#include <llvm/CodeGen/MachineInstr.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCInstrAnalysis.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#undef DEBUG_TYPE
#define DEBUG_TYPE "luthier-code-lifter"

namespace luthier {

/// \brief 一个单例类，负责：\n
/// 1. 使用 LLVM MC 反汇编类型为 \c KERNEL 或 \c DEVICE_FUNCTION 的 \c hsa::ExecutableSymbol，并将其作为 \c hsa::Instr 的向量返回，不对操作数进行符号化。\n
/// 2. 将从 LLVM MC 获得的反汇编信息与从支持的 \p hsa::Executable 获得的附加信息转换为 LLVM Machine IR (MIR)，并将其作为 \c LiftedRepresentation 暴露给用户。\n
/// 3. TODO：如果反汇编/提升的 \p hsa::LoadedCodeObject 中存在调试信息，MC 表示和 MIR 表示也将包含调试信息（如果请求）。
/// \details \p CodeLifter 提升的 MIR 可以具有以下粒度级别：\n
/// 1. 内核级别，其中 Module 和 MMI 仅包含足够的信息，使单个内核能够独立于其父级（未插桩的 \c hsa::Executable 和 \c hsa::LoadedCodeObject）运行。\n
/// 2. 可执行文件级别，其中 Module 和 MMI 包含可以从单个 \p hsa::Executable 提取的所有信息。\n
/// \p CodeLifter 的所有操作都会被尽可能地缓存，并在包含所有被检查项的 \p hsa::Executable 被运行时销毁时失效。
/// \brief A singleton class in charge of: \n
/// 1. disassembling an \c hsa::ExecutableSymbol of type \c KERNEL or
/// \c DEVICE_FUNCTION using LLVM MC and returning them as a vector of \c
/// hsa::Instr, without symbolizing the operands. \n
/// 2. Converting the disassembled information obtained from LLVM MC plus
/// additional information obtained from the backing \p hsa::Executable to
/// LLVM Machine IR (MIR) and exposing them as a \c LiftedRepresentation to the
/// user. \n
/// 3. TODO: In the presence of debug information in the disassembled/lifted
/// \p hsa::LoadedCodeObject, both the MC representation and MIR representation
/// will also contain the debug information, if requested.
/// \details The MIR lifted by the \p CodeLifter can have the following levels
/// of granularity:\n
/// 1. Kernel-level, in which the Module and MMI only contains enough
/// information to make a single kernel run independently from its parents, the
/// un-instrumented \c hsa::Executable and \c hsa::LoadedCodeObject.\n
/// 2. Executable-level, in which the Module and MMI contain all the information
/// that could be extracted from a single \p hsa::Executable.\n
/// All operations done by the \p CodeLifter is meant to be cached to the
/// best of ability, and invalidated once the \p hsa::Executable containing all
/// the inspected items are destroyed by the runtime.
class CodeLifter : public Singleton<CodeLifter> {

  //===--------------------------------------------------------------------===//
  // CodeLifter 所有组件之间的通用和共享功能
  // Generic and shared functionality among all components of the CodeLifter
  //===--------------------------------------------------------------------===//

private:
  /// 保护代码提升器字段的互斥锁
  /// Mutex to protect fields of the code lifter
  std::recursive_mutex CacheMutex{};

  const rocprofiler::HsaApiTableSnapshot<::CoreApiTable> &CoreApiSnapshot;

  const rocprofiler::HsaExtensionTableSnapshot<HSA_EXTENSION_AMD_LOADER>
      &LoaderApiSnapshot;

  /// 由内部的 HSA 回调中的 \c Controller 调用，通知 \c CodeLifter \p Exec 已被 HSA 运行时销毁；
  /// 因此任何与 \p Exec 相关的缓存信息必须被移除，因为它们不再有效
  /// \param Exec 即将被 HSA 运行时销毁的 \p hsa::Executable
  /// \return 描述操作是否成功或遇到错误的 \p llvm::Error
  /// Invoked by the \c Controller in the internal HSA callback to notify
  /// the \c CodeLifter that \p Exec has been destroyed by the HSA runtime;
  /// Therefore any cached information related to \p Exec must be removed since
  /// it is no longer valid
  /// \param Exec the \p hsa::Executable that is about to be destroyed by the
  /// HSA runtime
  /// \return \p llvm::Error describing whether the operation succeeded or
  /// faced an error
  llvm::Error invalidateCachedExecutableItems(hsa_executable_t Exec);
  //===--------------------------------------------------------------------===//
  // 基于 MC 的反汇编功能
  // MC-backed Disassembly Functionality
  //===--------------------------------------------------------------------===//

private:
  /// \brief 包含 LLVM 执行反汇编操作所需的每个 \c hsa::Isa 的构造。不包含由 \c TargetManager 已创建的构造。
  /// \struct DisassemblyInfo
  /// \brief Contains the constructs needed by LLVM for performing a disassembly
  /// operation for each \c hsa::Isa. Does not contain the constructs already
  /// created by the \c TargetManager
  struct DisassemblyInfo {
    std::unique_ptr<llvm::MCContext> Context;
    std::unique_ptr<llvm::MCDisassembler> DisAsm;

    DisassemblyInfo() : Context(nullptr), DisAsm(nullptr) {};

    DisassemblyInfo(std::unique_ptr<llvm::MCContext> Context,
                    std::unique_ptr<llvm::MCDisassembler> DisAsm)
        : Context(std::move(Context)), DisAsm(std::move(DisAsm)) {};
  };

  /// 包含每个 \c hsa_isa_t 缓存的 \c DisassemblyInfo
  /// Contains the cached \c DisassemblyInfo for each \c hsa_isa_t
  llvm::DenseMap<hsa_isa_t, DisassemblyInfo> DisassemblyInfoMap{};

  /// 成功时，返回与给定 \p ISA 关联的 \c DisassemblyInfo 的引用。如果在 \c DisassemblyInfoMap 中不存在则创建
  /// \param ISA 要获取的 \c DisassemblyInfo 的 \c hsa_isa_t
  /// \return 成功时返回与给定 \p ISA 关联的 \c DisassemblyInfo 的引用，失败时返回描述过程中遇到问题的 \c llvm::Error
  /// On success, returns a reference to the \c DisassemblyInfo associated with
  /// the given \p ISA. Creates the info if not already present in the \c
  /// DisassemblyInfoMap
  /// \param ISA the \c hsa_isa_t of the \c DisassemblyInfo
  /// \return on success, a reference to the \c DisassemblyInfo associated with
  /// the given \p ISA, on failure, an \c llvm::Error describing the issue
  /// encountered during the process
  llvm::Expected<DisassemblyInfo &> getDisassemblyInfo(hsa_isa_t ISA);

  /// 由 \c CodeLifter 已反汇编的内核/设备函数符号的缓存。\n
  /// 向量句柄本身分配为唯一指针，以阻止映射过早调用其析构函数。\n
  /// 一旦与符号关联的可执行文件被销毁，条目就会失效。
  /// Cache of kernel/device function symbols already disassembled by the
  /// \c CodeLifter.\n
  /// The vector handles themselves are allocated as a unique pointer to
  /// stop the map from calling its destructor prematurely.\n
  /// Entries get invalidated once the executable associated with the symbols
  /// get destroyed.
  std::unordered_map<
      std::unique_ptr<hsa::LoadedCodeObjectSymbol>,
      std::unique_ptr<llvm::SmallVector<hsa::Instr>>,
      hsa::LoadedCodeObjectSymbolHash<hsa::LoadedCodeObjectSymbol>,
      hsa::LoadedCodeObjectSymbolEqualTo<hsa::LoadedCodeObjectSymbol>>
      MCDisassembledSymbols{};

  /// LLVM evaluateBranch 的修正版本
  /// TODO: 将此修复合并到上游 LLVM
  /// The corrected version of LLVM's evaluate branch
  /// TODO: Merge this fix to upstream LLVM
  static bool evaluateBranch(const llvm::MCInst &Inst, uint64_t Addr,
                             uint64_t Size, uint64_t &Target);

public:
  CodeLifter(
      const rocprofiler::HsaApiTableSnapshot<::CoreApiTable> &CoreApiSnapshot,
      const rocprofiler::HsaExtensionTableSnapshot<HSA_EXTENSION_AMD_LOADER>
          &LoaderApiSnapshot)
      : Singleton<luthier::CodeLifter>(), CoreApiSnapshot(CoreApiSnapshot),
        LoaderApiSnapshot(LoaderApiSnapshot) {};

  /// 反汇编函数类型 \p Symbol 的内容并返回其 <tt>hsa::Instr</tt> 数组的引用\n
  /// 不执行任何符号化或控制流分析\n
  /// 支持的 \c hsa::LoadedCodeObject 的 \c hsa::ISA 将用于反汇编 \p Symbol\n
  /// 此操作的结果在首次调用时被缓存
  /// \tparam ST 加载的代码对象符号的类型；必须为 \p KERNEL 或 \p DEVICE_FUNCTION 类型
  /// \param Symbol 要反汇编的符号
  /// \return 成功时返回缓存的反汇编指令的常量引用；失败时返回 \p llvm::Error
  /// \sa hsa::Instr
  /// Disassembles the contents of the function-type \p Symbol and returns
  /// a reference to its disassembled array of <tt>hsa::Instr</tt>s\n
  /// Does not perform any symbolization or control flow analysis\n
  /// The \c hsa::ISA of the backing \c hsa::LoadedCodeObject will be used to
  /// disassemble the \p Symbol\n
  /// The results of this operation gets cached on the first invocation
  /// \tparam ST type of the loaded code object symbol; Must be of
  /// type \p KERNEL or \p DEVICE_FUNCTION
  /// \param Symbol the symbol to be disassembled
  /// \return on success, a const reference to the cached disassembled
  /// instructions; On failure, an \p llvm::Error
  /// \sa hsa::Instr
  template <typename ST,
            typename = std::enable_if<
                std::is_same_v<ST, hsa::LoadedCodeObjectDeviceFunction> ||
                std::is_same_v<ST, hsa::LoadedCodeObjectKernel>>>
  llvm::Expected<llvm::ArrayRef<hsa::Instr>> disassemble(const ST &Symbol) {
    std::lock_guard Lock(CacheMutex);
    if (!MCDisassembledSymbols.contains(&Symbol)) {
      // 获取与符号关联的 ISA
      // Get the ISA associated with the Symbol
      hsa_loaded_code_object_t LCO = Symbol.getLoadedCodeObject();

      llvm::Expected<luthier::object::AMDGCNObjectFile &> ObjFileOrErr =
          hsa::LoadedCodeObjectCache::instance().getAssociatedObjectFile(LCO);
      LUTHIER_RETURN_ON_ERROR(ObjFileOrErr.takeError());

      auto LLVMIsa = object::getObjectFileTargetTuple(*ObjFileOrErr);
      LUTHIER_RETURN_ON_ERROR(LLVMIsa.takeError());

      auto ISA =
          hsa::isaFromLLVM(CoreApiSnapshot.getTable(), std::get<0>(*LLVMIsa),
                           std::get<1>(*LLVMIsa), std::get<2>(*LLVMIsa));
      LUTHIER_RETURN_ON_ERROR(ISA.takeError());
      // 在主机上定位符号的加载内容
      // Locate the loaded contents of the symbol on the host
      auto MachineCodeOnDevice =
          Symbol.getLoadedSymbolContents(LoaderApiSnapshot.getTable());
      LUTHIER_RETURN_ON_ERROR(MachineCodeOnDevice.takeError());
      auto MachineCodeOnHost = hsa::convertToHostEquivalent(
          LoaderApiSnapshot.getTable(), *MachineCodeOnDevice);
      LUTHIER_RETURN_ON_ERROR(MachineCodeOnHost.takeError());

      auto InstructionsAndAddresses = disassemble(*ISA, *MachineCodeOnHost);
      LUTHIER_RETURN_ON_ERROR(InstructionsAndAddresses.takeError());
      auto [Instructions, Addresses] = *InstructionsAndAddresses;

      auto &Out =
          MCDisassembledSymbols
              .emplace(Symbol.clone(),
                       std::make_unique<llvm::SmallVector<hsa::Instr>>())
              .first->second;
      Out->reserve(Instructions.size());

      auto TargetInfo = TargetManager::instance().getTargetInfo(*ISA);
      LUTHIER_RETURN_ON_ERROR(TargetInfo.takeError());

      auto MII = TargetInfo->getMCInstrInfo();

      auto BaseLoadedAddress =
          reinterpret_cast<luthier::address_t>(MachineCodeOnDevice->data());

      luthier::address_t PrevInstAddress = BaseLoadedAddress;

      for (unsigned int I = 0; I < Instructions.size(); ++I) {
        auto &Inst = Instructions[I];
        auto Address = Addresses[I] + BaseLoadedAddress;
        auto Size = Address - PrevInstAddress;
        if (MII->get(Inst.getOpcode()).isBranch()) {
          LLVM_DEBUG(

              llvm::dbgs() << "Instruction ";
              Inst.dump_pretty(llvm::dbgs(), TargetInfo->getMCInstPrinter(),
                               " ", TargetInfo->getMCRegisterInfo());
              llvm::dbgs() << llvm::formatv(
                  " at idx {0}, address {1:x}, size {2} is a branch; "
                  "Evaluating its target.\n",
                  I, Address, Size);

          );
          luthier::address_t Target;
          if (evaluateBranch(Inst, Address, Size, Target)) {
            LLVM_DEBUG(llvm::dbgs() << llvm::formatv(
                           "Evaluated address {0:x} as the branch target.\n",
                           Target););
            addDirectBranchTargetAddress(LCO, Target);
          } else {
            LLVM_DEBUG(llvm::dbgs()
                       << "Failed to evaluate the branch target.\n");
          }
        }
        PrevInstAddress = Address;
        Out->push_back(hsa::Instr(Inst, Symbol, Address, Size));
      }
    }
    return *MCDisassembledSymbols.find(&Symbol)->second;
  }

  /// 为给定的 \p ISA 反汇编 \p code 封装的机器码
  /// \param ISA \p Code 的 \p hsa::Isa
  /// \param Code 指向机器码开头和结尾的 \p llvm::ArrayRef
  /// \return 成功时返回 \p llvm::MCInst 的 \p std::vector 和包含每条指令起始地址的 \p std::vector
  /// Disassembles the machine code encapsulated by \p code for the given \p ISA
  /// \param ISA the \p hsa::Isa of the \p Code
  /// \param Code an \p llvm::ArrayRef pointing to the beginning and end of the
  ///  machine code
  /// \return on success, returns a \p std::vector of \p llvm::MCInst and
  /// a \p std::vector containing the start address of each instruction
  llvm::Expected<std::pair<std::vector<llvm::MCInst>, std::vector<address_t>>>
  disassemble(hsa_isa_t ISA, llvm::ArrayRef<uint8_t> Code);

  //===--------------------------------------------------------------------===//
  // 代码提升功能的开始
  // Beginning of Code Lifting Functionality
  //===--------------------------------------------------------------------===//

private:
  //===--------------------------------------------------------------------===//
  // MachineBasicBlock 解析
  // MachineBasicBlock resolving
  //===--------------------------------------------------------------------===//

  /// \brief 包含每个 \c hsa::LoadedCodeObject 中作为其他分支指令目标的 HSA 指令的地址
  /// \details 此映射在将 MC 指令提升到 MIR 期间使用，用于指示每个 \p llvm::MachineBasicBlock 的开始/结束。它在函数 MC 反汇编期间被填充
  /// \brief Contains the addresses of the HSA instructions that are
  /// target of other branch instructions, per
  /// \c hsa::LoadedCodeObject
  /// \details This map is used during lifting of MC instructions to MIR to
  /// indicate start/end of each \p llvm::MachineBasicBlock. It gets populated
  /// by during MC disassembly of functions
  llvm::DenseMap<hsa_loaded_code_object_t, llvm::DenseSet<address_t>>
      DirectBranchTargetLocations{};

  /// 检查给定的 \p Address 是否是直接分支指令目标的开始
  /// \param LCO 在其加载区域包含 \p Address 的 \p hsa::LoadedCodeObject
  /// \param Address \p hsa::LoadedCodeObject 中的设备地址
  /// \return 如果 Address 是另一个分支指令目标的开始则为 \c true；否则为 \c false
  /// Checks whether the given \p Address is the start of a target of a
  /// direct branch instruction
  /// \param LCO an \p hsa::LoadedCodeObject that contains the \p Address
  /// in its loaded region
  /// \param \c Address a device address in the \p hsa::LoadedCodeObject
  /// \return true if the Address is the start of the target of another branch
  /// instruction; \c false otherwise
  bool isAddressDirectBranchTarget(hsa_loaded_code_object_t LCO,
                                   address_t Address);

  /// 由 MC 反汇编器功能使用，通知 \c BranchLocations 作为直接分支指令目标的指令的加载地址
  /// \param LCO 在其加载区域包含 \p Address 的 \p hsa::LoadedCodeObject
  /// \param Address \p hsa::LoadedCodeObject 中的设备地址
  /// Used by the MC disassembler functionality to notify
  /// \c BranchLocations about the loaded address of an instruction
  /// that is the target of a direct branch instruction
  /// \param LCO an \p hsa::LoadedCodeObject that contains the \p Address
  /// in its loaded region
  /// \param Address a device address in the \p hsa::LoadedCodeObject
  void addDirectBranchTargetAddress(hsa_loaded_code_object_t LCO,
                                    address_t Address);

  //===--------------------------------------------------------------------===//
  // 重定位解析
  // Relocation resolving
  //===--------------------------------------------------------------------===//

  typedef struct {
    std::unique_ptr<hsa::LoadedCodeObjectSymbol>
        Symbol; /// 被重定位引用的 HSA 可执行文件符号 /// The HSA Executable Symbol
                /// referenced by the relocation
    llvm::object::ELFRelocationRef
        Relocation; /// ELF 重定位信息 /// The ELF relocation information
                    /// Safe to store directly since
                    /// LCO caches the ELF
  } LCORelocationInfo;

  /// 每个提升的 \c hsa::LoadedCodeObject 中每个加载地址的 \c LCORelocationInfo 信息缓存\n
  /// 将所有部分的重定位信息组合到此映射中
  /// Cache of \c LCORelocationInfo information per loaded address in each
  /// lifted \c hsa::LoadedCodeObject\n
  /// Combines relocation information from all sections into this map
  llvm::DenseMap<hsa_loaded_code_object_t,
                 llvm::DenseMap<address_t, LCORelocationInfo>>
      Relocations{};

  /// 如果 \p address 没有与之关联的重定位信息，返回 \c std::nullopt，否则返回关联的 \c LCORelocationInfo
  /// \param LCO 在其加载范围内包含 \p Address 的 \c hsa::LoadedCodeObject
  /// \param Address 被查询的加载地址
  /// \return 成功时返回与给定 \p Address 关联的 \c LCORelocationInfo（如果地址有重定位信息），否则返回 \c std::nullopt；失败时返回描述遇到问题的 \c llvm::Error
  /// Returns an \c std::nullopt if the \p address doesn't have any relocation
  /// information associated with it, or the \c LCORelocationInfo associated
  /// with it otherwise
  /// \param LCO The \c hsa::LoadedCodeObject which contains \p Address inside
  /// its loaded range
  /// \param Address the loaded address being queried
  /// \return on success, the the \c LCORelocationInfo associated with
  /// the given \p Address if the address has a relocation info, or
  /// an \c std::nullopt otherwise; an \c llvm::Error on failure describing the
  /// issue encountered
  llvm::Expected<const CodeLifter::LCORelocationInfo *>
  resolveRelocation(hsa_loaded_code_object_t LCO, address_t Address);

  //===--------------------------------------------------------------------===//
  // 与函数相关的代码提升功能
  // Function-related code-lifting functionality
  //===--------------------------------------------------------------------===//

  /// 初始化与 \p LR 中 \p LCO 关联的条目
  /// 为 \p LCO 创建 \c llvm::Module 和 \c llvm::MachineModuleInfo
  /// \param [in] LCO 要提升的 \c hsa::LoadedCodeObject
  /// \param [in, out] LR 要更新的提升表示
  /// \return 如果过程中遇到任何问题则返回 \c llvm::Error
  /// Initializes an entry associated with the \p LCO inside the \p LR
  /// Creates an \c llvm::Module and \c llvm::MachineModuleInfo for the
  /// \p LCO
  /// \param [in] LCO the \c hsa::LoadedCodeObject to be lifted
  /// \param [in, out] LR the lifted representation to be updated
  /// \return an \c llvm::Error if any issues were encountered during the
  /// process
  llvm::Error initLR(LiftedRepresentation &LR,
                     const hsa::LoadedCodeObjectKernel &Kernel);

  /// 初始化与 \p LR 中 \p GV 关联的模块条目
  /// 不检查传递的 \p GV 是否确实是变量类型
  /// \param [in] LCO \p GV 所属的 \c hsa::LoadedCodeObject
  /// \param [in] GV 要提升的类型为变量的 \c hsa::ExecutableSymbol
  /// \param [in, out] LR 要更新的提升表示
  /// \return 如果过程中遇到任何问题则返回 \c llvm::Error
  /// Initializes a module entry associated with the \p GV inside the \p LR
  /// Does not check if the passed \p GV is indeed is of type variable
  /// \param [in] LCO the \c hsa::LoadedCodeObject \p GV belongs to
  /// \param [in] GV the \c hsa::ExecutableSymbol of type variable to be lifted
  /// \param [in, out] LR the lifted representation to be updated
  /// \return an \c llvm::Error if any issues were encountered during the
  /// process
  llvm::Error
  initLiftedGlobalVariableEntry(hsa_loaded_code_object_t LCO,
                                const hsa::LoadedCodeObjectSymbol &GV,
                                LiftedRepresentation &LR);

  /// 初始化与 \p LR 中 \p Kernel 关联的模块条目
  /// \p Func 必须为 KERNEL 类型
  /// 不检查传递的符号是否确实是内核
  /// \param [in] LCO \p Kernel 所属的 \c hsa::LoadedCodeObject
  /// \param [in] Kernel 要初始化的 \c hsa::ExecutableSymbol
  /// \param [in, out] LR 要更新的提升表示
  /// \return 如果过程中遇到任何问题则返回 \c llvm::Error
  /// Initializes a module entry associated with the \p Kernel inside the \p LR
  /// \p Func must be of type KERNEL
  /// Does not check if the passed symbol is indeed a kernel
  /// \param [in] LCO the \c hsa::LoadedCodeObject \p Kernel belongs to
  /// \param [in] Kernel the \c hsa::ExecutableSymbol to be initialized
  /// \param [in, out] LR the lifted representation to be updated
  /// \return an \c llvm::Error if any issues were encountered during the
  /// process
  llvm::Error initLiftedKernelEntry(const hsa::LoadedCodeObjectKernel &Kernel,
                                    LiftedRepresentation &LR);

  /// 初始化与 \p LR 中 \p Func 关联的模块条目
  /// \p Func 必须为 DEVICE_FUNC 类型
  /// 不检查传递的符号是否确实是设备函数
  /// \tparam HT 提升的基元的基础类型
  /// \param [in] LCO \p Kernel 所属的 \c hsa::LoadedCodeObject
  /// \param [in] Func 要初始化的 \c hsa::ExecutableSymbol
  /// \param [in, out] LR 要更新的提升表示
  /// \return 如果过程中遇到任何问题则返回 \c llvm::Error
  /// Initializes a module entry associated with the \p Func inside the \p LR
  /// \p Func must be of type DEVICE_FUNC
  /// Does not check if the passed symbol is indeed a device function
  /// \tparam HT underlying type of the lifted primitive
  /// \param [in] LCO the \c hsa::LoadedCodeObject \p Kernel belongs to
  /// \param [in] Func the \c hsa::ExecutableSymbol to be initialized
  /// \param [in, out] LR the lifted representation to be updated
  /// \return an \c llvm::Error if any issues were encountered during the
  /// process
  llvm::Error
  initLiftedDeviceFunctionEntry(const hsa::LoadedCodeObjectDeviceFunction &Func,
                                LiftedRepresentation &LR);

  llvm::Error liftFunction(const hsa::LoadedCodeObjectSymbol &Symbol,
                           llvm::MachineFunction &MF, LiftedRepresentation &LR);

  //===--------------------------------------------------------------------===//
  // 缓存的提升表示
  // Cached Lifted Representations
  //===--------------------------------------------------------------------===//

  std::unordered_map<
      std::unique_ptr<hsa::LoadedCodeObjectKernel>,
      std::unique_ptr<LiftedRepresentation>,
      hsa::LoadedCodeObjectSymbolHash<hsa::LoadedCodeObjectKernel>,
      hsa::LoadedCodeObjectSymbolEqualTo<hsa::LoadedCodeObjectKernel>>
      LiftedKernelSymbols{};

  //===--------------------------------------------------------------------===//
  // 面向公众的代码提升功能
  // Public-facing code-lifting functionality
  //===--------------------------------------------------------------------===//
public:
  /// 返回与给定 \p Symbol 关联的 \c LiftedRepresentation\n
  /// 该表示隔离了单个内核可以独立于其父级 \c hsa::LoadedCodeObject 或 \c hsa::Executable 互依赖运行的要求\n
  /// 该表示在首次调用时被缓存
  /// \param KernelSymbol 类型为 \c KERNEL 的 \c hsa::ExecutableSymbol
  /// \return 成功时返回内核符号的提升表示；失败时返回描述过程中遇到问题的 \c llvm::Error
  /// \sa LiftedRepresentation
  /// Returns the \c LiftedRepresentation associated
  /// with the given \p Symbol\n
  /// The representation isolates the requirements of a single kernel can run
  /// interdependently from its parent \c hsa::LoadedCodeObject or \c
  /// hsa::Executable\n
  /// The representation gets cached on the first invocation
  /// \param KernelSymbol an \c hsa::ExecutableSymbol of type \c KERNEL
  /// \return on success, the lifted representation of the kernel symbol; an
  /// \c llvm::Error on failure, describing the issue encountered during the
  /// process
  /// \sa LiftedRepresentation
  llvm::Expected<const LiftedRepresentation &>
  lift(const hsa::LoadedCodeObjectKernel &KernelSymbol);

  llvm::Expected<std::unique_ptr<LiftedRepresentation>>
  cloneRepresentation(const LiftedRepresentation &SrcLR);
};

} // namespace luthier

#undef DEBUG_TYPE

#endif
