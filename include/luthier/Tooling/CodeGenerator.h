//===-- CodeGenerator.h - Luthier Code Generator ----------------*- C++ -*-===//
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
/// \brief 本文件描述了 Luthier 的代码生成器，这是一个单例类，负责根据插桩任务和变体函数对提升的表示进行插桩。
/// This file describes Luthier's code generator, a singleton in charge of
/// instrumenting lifted representation given an instrumentation task and
/// a mutator function.
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_TOOLING_CODE_GENERATOR_H
#define LUTHIER_TOOLING_CODE_GENERATOR_H
#include "luthier/Common/Singleton.h"
#include "luthier/Intrinsic/IntrinsicProcessor.h"
#include "luthier/Rocprofiler/ApiTableSnapshot.h"

namespace llvm {

class MachineModuleInfoWrapperPass;

} // namespace llvm

namespace luthier {

class InstrumentationTask;

class LiftedRepresentation;

namespace hsa {

class ISA;

} // namespace hsa

/// \brief 负责生成插桩机器代码的单例类
/// \details <tt>CodeGenerator</tt> 执行以下任务：\n
/// 1. 在插桩 <tt>llvm::Module</tt> (<tt>IModule</tt>) 中创建对钩子的调用，在 <tt>IModule</tt> 内创建注入负载 <tt>llvm::Function</tt> 的集合。\n
/// 2. 对插桩模块运行 IR 优化流水线以优化插桩函数。\n
/// 3. 运行 Luthier intrinsic 的 IR lowering 函数。\n
/// 4. 对插桩模块运行修改后的 LLVM CodeGen 流水线，包括：a) 运行正常的 ISEL，b) 对 intrinsic 调用 MIR lowering 函数，c) 虚拟化对物理寄存器的访问，并在 MIR 中表达寄存器约束，d) 在插桩 Module 函数内的栈操作数寄存器分配和 lowering 之后进行自定义帧 lowering。\n
/// 5. 跟踪每个 intrinsic 如何 lowering；有一组为 Luthier 内置的 intrinsic（例如 <tt>readReg</tt>），还有一组工具编写者可以通过描述它们的 lowering 方式来注册的 intrinsic。
/// \brief Singleton in charge of generating instrumented machine code
/// \details <tt>CodeGenerator</tt> performs the following tasks:
/// 1. Create calls to hooks inside an instrumentation
/// <tt>llvm::Module</tt> (<tt>IModule</tt>), creating a collection of
/// injected payload <tt>llvm::Function</tt>s inside the <tt>IModule</tt>. \n
/// 2. Run the IR optimization pipeline on the instrumentation module to
/// optimize the instrumentation functions. \n
/// 3. Run the IR lowering functions of Luthier intrinsics. \n
/// 4. Run a modified version of the LLVM CodeGen pipeline on the
/// instrumentation module, which involves: a) running normal ISEL,
/// b) calling MIR lowering functions on intrinsics, c) virtualizing access to
/// physical registers, and expressing register constraints in MIR, d)
/// a custom frame lowering after register allocation and lowering of stack
/// operands inside instrumentation Module functions.
/// 5. Keep track of how each intrinsic is lowered; There are a set of
/// intrinsics built-in for Luthier (e.g. <tt>readReg</tt>) and there are a set
/// of intrinsics which a tool writer can register by describing how they
/// are lowered.
class CodeGenerator : public Singleton<CodeGenerator> {
private:
  /// 保存有关如何 lowering Luthier intrinsic 的信息
  /// Holds information regarding how to lower Luthier intrinsics
  llvm::StringMap<IntrinsicProcessor> IntrinsicsProcessors;

  /// HSA Core API 表快照
  /// HSA Core API table snapshot
  const rocprofiler::HsaApiTableSnapshot<::CoreApiTable> &CoreApiSnapshot;

  const rocprofiler::HsaExtensionTableSnapshot<HSA_EXTENSION_AMD_LOADER>
      &LoaderApiSnapshot;

public:
  explicit CodeGenerator(
      const rocprofiler::HsaApiTableSnapshot<::CoreApiTable> &CoreApiSnapshot,
      const rocprofiler::HsaExtensionTableSnapshot<HSA_EXTENSION_AMD_LOADER>
          &LoaderApiSnapshot)
      : CoreApiSnapshot(CoreApiSnapshot),
        LoaderApiSnapshot(LoaderApiSnapshot) {};

  /// 向 <tt>CodeGenerator</tt> 注册一个 Luthier intrinsic，并提供一种将其 lowering 为 Machine IR 的方法
  /// \param Name intrinsic 的非混淆函数名称，不带模板参数，但带有其绑定定义的命名空间（例如 <tt>"luthier::readReg"</tt>）
  /// \param Processor 描述如何 lowering Luthier intrinsic 的 \c IntrinsicProcessor
  /// Register a Luthier intrinsic with the <tt>CodeGenerator</tt> and provide a
  /// way to lower it to Machine IR
  /// \param Name the demangled function name of the intrinsic, without the
  /// template arguments but with the namespace(s) its binding is defined
  /// (e.g. <tt>"luthier::readReg"</tt>)
  /// \param Processor the \c IntrinsicProcessor describing how to lower the
  /// Luthier intrinsic
  void registerIntrinsic(llvm::StringRef Name, IntrinsicProcessor Processor) {
    IntrinsicsProcessors.insert({Name, std::move(Processor)});
  }

  /// 首先通过克隆对传入的 \p LR 进行插桩，然后对其内容应用 \p Mutator
  /// \param LR 要被插桩的 \c LiftedRepresentation
  /// \param Mutator 可以修改提升表示的函数
  /// \return 包含插桩代码的新 \c LiftedRepresentation，或在过程中遇到问题时返回 \c llvm::Error
  /// Instruments the passed \p LR by first cloning it and then
  /// applying the \p Mutator onto its contents
  /// \param LR the \c LiftedRepresentation about to be instrumented
  /// \param Mutator a function that can modify the lifted representation
  /// \return a new \c LiftedRepresentation containing the instrumented code,
  /// or an \c llvm::Error in case an issue was encountered during the process
  llvm::Expected<std::unique_ptr<LiftedRepresentation>>
  instrument(const LiftedRepresentation &LR,
             llvm::function_ref<llvm::Error(InstrumentationTask &,
                                            LiftedRepresentation &)>
                 Mutator);

  /// 对 \p Module 和 \p MMIWP 的 \c llvm::MachineModuleInfo 运行 \c llvm::AsmPrinter pass 以生成可重定位文件
  /// \note 此函数不以线程安全的方式访问 Module 的 \c llvm::LLVMContext
  /// \note 打印后，\p MMIWP 将被用于打印汇编文件的旧版 pass 管理器删除
  /// \param [in] Module 要打印的 \c llvm::Module
  /// \param [in] TM 创建 \p MMIWP 时的 \c llvm::GCNTargetMachine
  /// \param [in] MMIWP 要打印的 \c llvm::MachineModuleInfoWrapperPass
  /// \param [out] CompiledObjectFile 编译后的可重定位文件
  /// \param FileType <tt>CompiledObjectFile</tt> 的类型；可以是 \c llvm::CodeGenFileType::AssemblyFile 或 \c llvm::CodeGenFileType::ObjectFile
  /// \return 如果过程中遇到任何问题返回 \c llvm::Error
  /// Runs the \c llvm::AsmPrinter pass on the \p Module and the
  /// \c llvm::MachineModuleInfo of the \p MMIWP to generate a relocatable file
  /// \note This function does not access the Module's \c llvm::LLVMContext in a
  /// thread-safe manner
  /// \note After printing, \p MMIWP will be deleted by the legacy pass manager
  /// used to print the assembly file
  /// \param [in] Module the \c llvm::Module to be printed
  /// \param [in] TM the \c llvm::GCNTargetMachine the \p MMIWP was created with
  /// \param [in] MMIWP the \c llvm::MachineModuleInfoWrapperPass to be printed;
  /// \param [out] CompiledObjectFile the compiled relocatable file
  /// \param FileType type of the <tt>CompiledObjectFile</tt>;
  /// Either \c llvm::CodeGenFileType::AssemblyFile or
  /// \c llvm::CodeGenFileType::ObjectFile
  /// \return an \c llvm::Error in case of any issues encountered during the
  /// process
  static llvm::Error
  printAssembly(llvm::Module &Module, llvm::GCNTargetMachine &TM,
                std::unique_ptr<llvm::MachineModuleInfoWrapperPass> &MMIWP,
                llvm::SmallVectorImpl<char> &CompiledObjectFile,
                llvm::CodeGenFileType FileType);

private:
  /// 将插桩任务 \p Task 应用于 \p LR 的提升表示\n
  /// \p Task 由 <tt>CodeGenerator::instrument</tt> 中的变体函数创建和填充
  /// \param [in] Task 应用于 \p LR 的 \c InstrumentationTask，包含一组将在目标应用程序的一组 <tt>llvm::MachineInstr</tt> 之前注入的钩子调用
  /// \param [in, out] LR 被插桩的 \c LiftedRepresentation
  /// \return 指示过程中是否遇到问题的 \c llvm::Error
  /// Applies the instrumentation task \p Task to the lifted representation
  /// of \p LR \n
  /// The \p Task is created and populated by the mutator function
  /// in <tt>CodeGenerator::instrument</tt>
  /// \param [in] Task the \c InstrumentationTask applied to the \p LR which
  /// contains a set of hook calls that will be injected before a set of
  /// <tt>llvm::MachineInstr</tt>s of the target application
  /// \param [in, out] LR the \c LiftedRepresentation being instrumented
  /// \return an \c llvm::Error indicating if any issues where encountered
  /// during the process
  llvm::Error applyInstrumentationTask(const InstrumentationTask &Task,
                                       LiftedRepresentation &LR);
};

} // namespace luthier

#endif
