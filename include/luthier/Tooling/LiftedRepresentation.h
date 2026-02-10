//===-- LiftedRepresentation.h ----------------------------------*- C++ -*-===//
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
/// \brief 本文件描述了 <tt>LiftedRepresentation</tt>，它封装了内核符号在 LLVM MIR 中的表示，以及 <tt>hsa::LoadedCodeObjectSymbol</tt> 与其提升的 LLVM 对等体之间的映射。
/// This file describes the <tt>LiftedRepresentation</tt>, which encapsulates
/// the representation of a kernel symbol in LLVM MIR, as well as mappings
/// between <tt>hsa::LoadedCodeObjectSymbol</tt>s and their lifted LLVM
/// equivalent.
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_TOOLING_LIFTED_REPRESENTATION_H
#define LUTHIER_TOOLING_LIFTED_REPRESENTATION_H
#include "AMDGPUTargetMachine.h"
#include <llvm/ADT/DenseMap.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include "luthier/HSA/Instr.h"
#include "luthier/HSA/LoadedCodeObjectDeviceFunction.h"
#include "luthier/HSA/LoadedCodeObjectKernel.h"

namespace luthier {

class CodeLifter;

namespace hsa {

class LoadedCodeObjectVariable;

class LoadedCodeObjectExternSymbol;

} // namespace hsa

/// \brief 保存有关提升的 AMD GPU 内核的信息，以及表示中涉及的 HSA 和 LLVM 对象之间的映射
/// \details "Lifting"（提升）在 Luthier 中是指检查加载在设备上的 AMDGPU 二进制内容的过程，以恢复有效的 LLVM Machine IR 表示，等效于或非常接近应用程序编译器最初用于创建被检查二进制文件的表示。Machine IR 允许对二进制指令进行灵活修改。\n
/// Luthier 的 \c CodeLifter 是唯一允许构造或克隆 <tt>LiftedRepresentation</tt> 的实体。这允许内部缓存和其他组件的线程安全访问。内核的可执行文件被销毁时，表示的缓存副本将失效。\n
/// 每个提升的内核都有一个独立的 \c llvm::orc::ThreadSafeContext，以允许多个线程进行独立处理和同步。提升表示的后续克隆使用相同的线程安全上下文。
/// \brief Holds information regarding a lifted AMD GPU kernel and the mapping
/// between the HSA and LLVM objects involved in the representation
/// \details "Lifting" in Luthier is the process of inspecting the contents
/// of AMDGPU binaries loaded on a device to recover a valid LLVM Machine IR
/// representation equivalent or very close to what the application's compiler
/// used originally for creating the inspected binaries. The Machine IR allows
/// for flexible modification of the binary's instruction.\n
/// Luthier's \c CodeLifter is the only entity allowed to construct or clone a
/// <tt>LiftedRepresentation</tt>. This allows internal caching and thread-safe
/// access to its instances by other components. The cached copy of the
/// representation gets invalidated when the executable of the kernel gets
/// destroyed. \n Each lifted kernel has an independent \c
/// llvm::orc::ThreadSafeContext for independent processing and synchronization
/// by multiple threads. Subsequent clones of the lifted
/// representation use the same thread-safe context.
class LiftedRepresentation {
  /// 只有 Luthier 的 CodeLifter 能够创建 <tt>LiftedRepresentation</tt>s
  /// Only Luthier's CodeLifter is able to create <tt>LiftedRepresentation</tt>s
  friend luthier::CodeLifter;

private:
  /// MMIWP 的目标机器
  /// Target machine of the \c MMIWP
  std::unique_ptr<llvm::GCNTargetMachine> TM{};

  /// 拥有所有线程安全模块的线程安全上下文；
  /// 每个 LiftedRepresentation 被赋予自己的上下文，以允许与其他上下文进行独立处理
  /// A thread-safe context that owns all the thread-safe modules;
  /// Each LiftedRepresentation is given its own context to allow for
  /// independent processing from others\n
  llvm::orc::ThreadSafeContext Context{};

  /// 提升内核的加载代码对象
  /// Loaded code object of the lifted kernel
  hsa_loaded_code_object_t LCO{};

  /// 提升内核的模块
  /// Module of the lifted kernel
  std::unique_ptr<llvm::Module> Module{};

  /// 提升内核的 MMIWP
  /// MMIWP of the lifted kernel
  std::unique_ptr<llvm::MachineModuleInfoWrapperPass> MMIWP{};

  /// 提升内核的符号
  /// The symbol of the lifted kernel
  std::unique_ptr<hsa::LoadedCodeObjectKernel> Kernel{};

  /// 提升内核的 MF
  /// MF of the lifted kernel
  llvm::MachineFunction *KernelMF{};

  /// 潜在被调用的设备函数符号与其 \c llvm::MachineFunction 之间的映射
  /// Mapping between the potentially called device function
  /// symbols and their \c llvm::MachineFunction
  std::unordered_map<
      std::unique_ptr<hsa::LoadedCodeObjectDeviceFunction>,
      llvm::MachineFunction *,
      hsa::LoadedCodeObjectSymbolHash<hsa::LoadedCodeObjectDeviceFunction>,
      hsa::LoadedCodeObjectSymbolEqualTo<hsa::LoadedCodeObjectDeviceFunction>>
      Functions{};

  /// 内核可能使用的静态变量与其 \c llvm::GlobalVariable 之间的映射\n
  /// 此映射还包括提升内核的 \c LCO 中的其他内核
  /// Mapping between static variables potentially used by the kernel and
  /// their \c llvm::GlobalVariable \n
  /// This map also includes other kernels inside the \c LCO of the
  /// lifted kernel as well
  std::unordered_map<
      std::unique_ptr<hsa::LoadedCodeObjectSymbol>, llvm::GlobalVariable *,
      hsa::LoadedCodeObjectSymbolHash<hsa::LoadedCodeObjectSymbol>,
      hsa::LoadedCodeObjectSymbolEqualTo<hsa::LoadedCodeObjectSymbol>>
      Variables{};

  /// 一个 \c llvm::MachineInstr（在其中一个 MMI 中）与其 HSA 表示 \c hsa::Instr 之间的映射。
  /// 这在用户想要查看机器指令的原始 \c llvm::MCInst 或关于指令在运行时加载位置的任何其他信息时很有用。\n
  /// 此映射仅在任何 LLVM pass 在 MMI 上运行之前有效；之后，每个机器指令的指针被底层分配器更改，此映射将失效
  /// A mapping between an \c llvm::MachineInstr in one of the MMIs and
  /// its HSA representation, \c hsa::Instr. This is useful to have in case
  /// the user wants to peak at the original \c llvm::MCInst of the machine
  /// instruction or any other information about where the instruction is loaded
  /// during runtime. \n
  /// This mapping is only valid before any LLVM pass is run over the MMIs;
  /// After that pointers of each machine instruction gets changed by the
  /// underlying allocator, and this map becomes invalid
  llvm::DenseMap<llvm::MachineInstr *, hsa::Instr *> MachineInstrToMCMap{};

  LiftedRepresentation();

public:
  /// 析构函数
  /// Destructor
  ~LiftedRepresentation();

  /// 禁止复制构造
  /// Disallowed copy construction
  LiftedRepresentation(const LiftedRepresentation &) = delete;

  /// 禁止赋值操作
  /// Disallowed assignment operation
  LiftedRepresentation &operator=(const LiftedRepresentation &) = delete;

  /// \return 提升表示的机器模块信息的目标机器
  /// \return the Target Machine of the lifted representation's machine module
  /// info
  [[nodiscard]] const llvm::GCNTargetMachine &getTM() const { return *TM; }

  /// \return 提升表示的机器模块信息的目标机器
  /// \return the Target Machine of the lifted representation's machine module
  /// info
  [[nodiscard]] llvm::GCNTargetMachine &getTM() { return *TM; }

  /// \return 此提升表示的 \c LLVMContext 的引用
  /// \return a reference to the \c LLVMContext of this Lifted Representation
  llvm::LLVMContext &getContext() { return *Context.getContext(); }

  /// \return 此提升表示的 \c LLVMContext 的常量引用
  /// \return a const reference to the \c LLVMContext of this
  /// Lifted Representation
  [[nodiscard]] const llvm::LLVMContext &getContext() const {
    return *Context.getContext();
  }

  /// \return 保护此 \c LiftedRepresentation 的 Context 和 TargetMachine 的作用域锁
  /// \return a scoped lock protecting the Context and the TargetMachine of this
  /// \c LiftedRepresentation
  llvm::orc::ThreadSafeContext::Lock getLock() const {
    return Context.getLock();
  }

  /// \return 提升内核的加载代码对象
  /// \return the loaded code object of the lifted kernel
  hsa_loaded_code_object_t getLoadedCodeObject() const { return LCO; }

  /// \return 提升表示的 \c llvm::Module
  /// \return the \c llvm::Module of the lifted representation
  [[nodiscard]] const llvm::Module &getModule() const { return *Module; }

  /// \return 提升表示的 \c llvm::Module
  /// \return the \c llvm::Module of the lifted representation
  [[nodiscard]] llvm::Module &getModule() { return *Module; }

  /// \return 提升表示的 \c llvm::MachineModuleInfo
  /// \return the \c llvm::MachineModuleInfo of the lifted representation
  [[nodiscard]] const llvm::MachineModuleInfo &getMMI() const {
    return MMIWP->getMMI();
  }

  /// \return 提升表示的 \c llvm::MachineModuleInfo
  /// \return the \c llvm::MachineModuleInfo of the lifted representation
  [[nodiscard]] llvm::MachineModuleInfo &getMMI() { return MMIWP->getMMI(); }

  /// \return 包含提升表示的 MIR 的 \c llvm::MachineModuleInfoWrapperPass
  /// \return the \c llvm::MachineModuleInfoWrapperPass containing the
  /// MIR of the lifted representation
  [[nodiscard]] const llvm::MachineModuleInfoWrapperPass &getMMIWP() const {
    return *MMIWP;
  }

  /// \return 包含提升表示的 MIR 的 \c llvm::MachineModuleInfoWrapperPass
  /// \note MMIWP 在对其运行旧版代码生成 pass 后将被删除，有效地使整个提升表示失效
  /// \return the \c llvm::MachineModuleInfoWrapperPass containing the
  /// MIR of the lifted representation
  /// \note the MMIWP will be deleted after running legacy codegen passes on
  /// it, effectively invalidating the entire lifted representation
  [[nodiscard]] std::unique_ptr<llvm::MachineModuleInfoWrapperPass> &
  getMMIWP() {
    return MMIWP;
  }

  /// \return 提升内核的符号
  /// \return the symbol of the lifted kernel
  const hsa::LoadedCodeObjectKernel &getKernel() const { return *Kernel; }

  /// \return 包含提升内核的 <tt>llvm::MachineInstr</tt>s 的 \c llvm::MachineFunction
  /// \return the \c llvm::MachineFunction containing the
  /// <tt>llvm::MachineInstr</tt>s of the lifted kernel
  [[nodiscard]] const llvm::MachineFunction &getKernelMF() const {
    return *KernelMF;
  }

  /// \return 包含提升内核的 <tt>llvm::MachineInstr</tt>s 的 \c llvm::MachineFunction
  /// \return the \c llvm::MachineFunction containing the
  /// <tt>llvm::MachineInstr</tt>s of the lifted kernel
  [[nodiscard]] llvm::MachineFunction &getKernelMF() { return *KernelMF; }

  /// 相关函数迭代器
  /// Related function iterator
  using function_iterator = decltype(Functions)::iterator;
  /// 相关函数常量迭代器
  /// Related function constant iterator
  using const_function_iterator = decltype(Functions)::const_iterator;
  /// 相关全局变量迭代器
  /// Related Global Variable iterator.
  using global_iterator = decltype(Variables)::iterator;
  /// 全局变量常量迭代器
  /// The Global Variable constant iterator.
  using const_global_iterator = decltype(Variables)::const_iterator;

  /// 函数迭代
  /// Function iteration
  function_iterator function_begin() { return Functions.begin(); }
  [[nodiscard]] const_function_iterator function_begin() const {
    return Functions.begin();
  }

  function_iterator function_end() { return Functions.end(); }
  [[nodiscard]] const_function_iterator function_end() const {
    return Functions.end();
  }

  [[nodiscard]] size_t function_size() const { return Functions.size(); };

  [[nodiscard]] bool function_empty() const { return Functions.empty(); };

  llvm::iterator_range<function_iterator> functions() {
    return llvm::make_range(function_begin(), function_end());
  }

  [[nodiscard]] llvm::iterator_range<const_function_iterator>
  functions() const {
    return llvm::make_range(function_begin(), function_end());
  }

  /// 全局变量迭代
  /// Global Variable iteration
  global_iterator global_begin() { return Variables.begin(); }
  [[nodiscard]] const_global_iterator global_begin() const {
    return Variables.begin();
  }

  global_iterator global_end() { return Variables.end(); }
  [[nodiscard]] const_global_iterator global_end() const {
    return Variables.end();
  }

  [[nodiscard]] size_t global_size() const { return Variables.size(); };

  [[nodiscard]] bool global_empty() const { return Variables.empty(); };

  llvm::iterator_range<global_iterator> globals() {
    return llvm::make_range(global_begin(), global_end());
  }

  [[nodiscard]] llvm::iterator_range<const_global_iterator> globals() const {
    return llvm::make_range(global_begin(), global_end());
  }

  /// 遍历提升表示中所有定义的函数，并对所有函数应用 \p Lambda 函数
  /// 定义的函数包括提升的内核以及内核的加载代码对象中包含的所有设备函数
  /// Iterates over all defined functions in the lifted representation
  /// and applies the \p Lambda function on all of them
  /// Defined functions include the lifted kernel,
  /// as well as all device functions included in the kernel's loaded code
  /// object
  llvm::Error iterateAllDefinedFunctionTypes(
      const std::function<llvm::Error(const hsa::LoadedCodeObjectSymbol &,
                                      llvm::MachineFunction &)> &Lambda);

  /// \return 与 \p VariableSymbol 关联的 \c llvm::GlobalVariable（如果存在），否则为 \c nullptr
  /// \return the \c llvm::GlobalVariable associated with
  /// \p VariableSymbol if exists \c nullptr otherwise
  [[nodiscard]] llvm::GlobalVariable *
  getLiftedEquivalent(const hsa::LoadedCodeObjectVariable &VariableSymbol);

  /// \return 与 \p VariableSymbol 关联的 \c llvm::GlobalVariable（如果存在），否则为 \c nullptr
  /// \return the \c llvm::GlobalVariable associated with
  /// \p VariableSymbol if exists \c nullptr otherwise
  [[nodiscard]] const llvm::GlobalVariable *getLiftedEquivalent(
      const hsa::LoadedCodeObjectVariable &VariableSymbol) const;

  /// \return 与 \p VariableSymbol 关联的 \c llvm::GlobalVariable（如果存在），否则为 \c nullptr
  /// \return the \c llvm::GlobalVariable associated with
  /// \p VariableSymbol if exists \c nullptr otherwise
  [[nodiscard]] const llvm::GlobalVariable *getLiftedEquivalent(
      const hsa::LoadedCodeObjectExternSymbol &ExternSymbol) const;

  /// \return 与 \p VariableSymbol 关联的 \c llvm::GlobalVariable（如果存在），否则为 \c nullptr
  /// \return the \c llvm::GlobalVariable associated with
  /// \p VariableSymbol if exists \c nullptr otherwise
  [[nodiscard]] llvm::GlobalVariable *
  getLiftedEquivalent(const hsa::LoadedCodeObjectExternSymbol &ExternSymbol);

  /// \return 与 \p VariableSymbol 关联的 \c llvm::GlobalVariable（如果存在），否则为 \c nullptr
  /// \return the \c llvm::GlobalVariable associated with
  /// \p VariableSymbol if exists \c nullptr otherwise
  [[nodiscard]] const llvm::GlobalValue *
  getLiftedEquivalent(const hsa::LoadedCodeObjectKernel &KernelSymbol) const;

  /// \return 与 \p VariableSymbol 关联的 \c llvm::GlobalVariable（如果存在），否则为 \c nullptr
  /// \return the \c llvm::GlobalVariable associated with
  /// \p VariableSymbol if exists \c nullptr otherwise
  [[nodiscard]] llvm::GlobalValue *
  getLiftedEquivalent(const hsa::LoadedCodeObjectKernel &KernelSymbol);

  /// \return 与 \p DevFunc 关联的 \c llvm::Function（如果存在），否则为 \c nullptr
  /// \return the \c llvm::Function associated with
  /// \p DevFunc if exists \c nullptr otherwise
  [[nodiscard]] const llvm::Function *
  getLiftedEquivalent(const hsa::LoadedCodeObjectDeviceFunction &DevFunc) const;

  /// \return 与 \p DevFunc 关联的 \c llvm::Function（如果存在），否则为 \c nullptr
  /// \return the \c llvm::Function associated with
  /// \p DevFunc if exists \c nullptr otherwise
  [[nodiscard]] llvm::Function *
  getLiftedEquivalent(const hsa::LoadedCodeObjectDeviceFunction &DevFunc);

  /// \return 与 \p Symbol 关联的 \c llvm::GlobalValue（如果存在）；否则为 \c nullptr
  /// \return the \c llvm::GlobalValue associated with \p Symbol if exists;
  /// \c nullptr otherwise
  [[nodiscard]] const llvm::GlobalValue *
  getLiftedEquivalent(const hsa::LoadedCodeObjectSymbol &Symbol) const;

  /// \return 与 \p Symbol 关联的 \c llvm::GlobalValue（如果存在）；否则为 \c nullptr
  /// \return the \c llvm::GlobalValue associated with \p Symbol if exists;
  /// \c nullptr otherwise
  [[nodiscard]] llvm::GlobalValue *
  getLiftedEquivalent(const hsa::LoadedCodeObjectSymbol &Symbol);

  /// \returns \p MI 被提升自的 \c hsa::Instr；如果 \p MI 不是提升代码的一部分，返回 <tt>nullptr</tt>
  /// \returns the \c hsa::Instr that the \p MI was lifted from; If
  /// the \p MI was not part of the lifted code, returns <tt>nullptr</tt>
  [[nodiscard]] const hsa::Instr *
  getLiftedEquivalent(const llvm::MachineInstr &MI) const;
};

} // namespace luthier

#endif
