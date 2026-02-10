//===-- IntrinsicProcessor.h ------------------------------------*- C++ -*-===//
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
/// \brief 本文件描述了 Luthier 的 Intrinsic Processor 结构和函数，工具需要这些来定义自定义 Luthier intrinsic。
/// This file describes Luthier's Intrinsic Processor structs and functions,
/// required to define custom Luthier intrinsics by a tool.
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_INTRINSIC_INTRINSIC_PROCESSOR_H
#define LUTHIER_INTRINSIC_INTRINSIC_PROCESSOR_H

#include "luthier/types.h"
#include <functional>
#include <llvm/ADT/Any.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/MCRegister.h>
#include <llvm/Support/Error.h>
#include <string>

namespace llvm {

class MachineFunction;

class MachineInstr;

class TargetRegisterInfo;

class TargetInstrInfo;

class TargetRegisterClass;

class Register;

class Value;

class Function;

class CallInst;

class GCNTargetMachine;

class MachineInstrBuilder;
} // namespace llvm

namespace luthier {

/// \brief 一组内核参数，Luthier 的 intrinsic lowering 机制可以确保访问
/// \details 这些值仅作为"参数"对内核可用，因为它们要么预先加载在 S/VGPR 中，要么作为"隐藏"参数在内核参数缓冲区中传递。由于这些值（或访问它们的方式）存储在 GPR 中，一旦插桩应用程序不再使用它们，它们就会被覆盖。为了确保在插桩例程中访问这些值，Luthier 必须在内核原始 prologue 的顶部发出 prologue，以将这些值保存到未使用的寄存器中，或将其溢出到插桩堆栈缓冲区的顶部，以便在需要时加载
/// \brief a set of kernel arguments Luthier's intrinsic lowering mechanism
/// can ensure access to
/// \details these values are only available to the kernel as "arguments"
/// as they come either preloaded in S/VGPRs or they are passed as "hidden"
/// arguments in the kernel argument buffer. As these values (or the way to
/// access them) are stored in GPRs they can be overwritten the moment they
/// are unused by the instrumented app. To ensure access to these values
/// in instrumentation routines, Luthier must emit a prologue on top of the
/// kernel's original prologue to save these values in an unused register,
/// or spill them to the top of the instrumentation stack's buffer to be
/// loaded when necessary
enum KernelArgumentType {
  /// 波前的私有段缓冲区
  /// Wavefront's private segment buffer
  WAVEFRONT_PRIVATE_SEGMENT_BUFFER = 0,
  /// 标记始终在 SGPR 上传递的内核参数开头的枚举
  /// Enum marking the beginning of kernel arguments always passed on SGPRs
  ALWAYS_IN_SGPR_BEGIN = WAVEFRONT_PRIVATE_SEGMENT_BUFFER,
  /// 内核参数缓冲区的 64 位地址
  /// 64-bit address of the kernel's argument buffer
  KERNARG_SEGMENT_PTR = 1,
  /// 从内核参数缓冲区开头到内核隐藏参数开始的 32 位偏移量
  /// 32-bit offset from the beginning of the kernel's argument buffer where
  /// the kernel's hidden arguments starts
  HIDDEN_KERNARG_OFFSET = 2,
  /// 从内核参数缓冲区开头到插桩传递的（即用户）参数缓冲区开始的 32 位偏移量
  /// 32-bit offset from the beginning of the kernel's argument buffer where
  /// the instrumentation-passed (i.e. user) argument buffer starts
  USER_KERNARG_OFFSET = 3,
  /// 内核的 64 位调度 ID
  /// 64-bit Dispatch ID of the kernel
  DISPATCH_ID = 4,
  /// 波前的 64 位扁平 scratch 基地址
  /// 64-bit flat scratch base address of the wavefront
  FLAT_SCRATCH = 5,
  /// 32 位私有段波偏移量
  /// 32-bit private segment wave offset
  PRIVATE_SEGMENT_WAVE_BYTE_OFFSET = 6,
  /// 标记始终在 SGPR 上传递的内核参数结尾的枚举
  /// Enum marking the end of kernel arguments always passed on SGPRs
  ALWAYS_IN_SGPR_END = PRIVATE_SEGMENT_WAVE_BYTE_OFFSET,
  /// 被执行内核的调度数据包的 64 位地址
  /// 64-bit address of the dispatch packet of the kernel being executed
  DISPATCH_PTR = 7,
  /// 标记可以在 SGPR 或隐藏内核参数上传递的内核参数开头的枚举
  /// Enum marking the beginning of kernel arguments that can either be passed
  /// on SGPRs or hidden kernel arguments
  EITHER_IN_SGPR_OR_HIDDEN_BEGIN = DISPATCH_PTR,
  /// 用于启动内核的 HSA 队列的 64 位地址
  /// 64-bit address of the HSA queue used to launch the kernel
  QUEUE_PTR = 8,
  /// 工作项私有段的大小
  /// Size of a work-item's private segment
  WORK_ITEM_PRIVATE_SEGMENT_SIZE = 9,
  /// 标记在 SGPR 或隐藏内核参数上传递的内核参数结尾的枚举
  /// Enum marking the end of kernel arguments that are either passed on the
  /// SGPRs or hidden kernel arguments
  EITHER_IN_SGPR_OR_HIDDEN_END = WORK_ITEM_PRIVATE_SEGMENT_SIZE,
  /// X 维度的调度工作组工作项计数
  /// Dispatch workgroup work-item count for the x dimension
  BLOCK_COUNT_X = 10,
  /// 标记仅隐藏内核参数开头的枚举
  /// Enum marking the beginning of hidden-only kernel arguments
  HIDDEN_BEGIN = BLOCK_COUNT_X,
  /// Y 维度的调度工作组工作项计数
  /// Dispatch workgroup work-item count for the y dimension
  BLOCK_COUNT_Y = 11,
  /// Z 维度的调度工作组工作项计数
  /// Dispatch workgroup work-item count for the z dimension
  BLOCK_COUNT_Z = 12,
  GROUP_SIZE_X = 13,
  GROUP_SIZE_Y = 14,
  GROUP_SIZE_Z = 15,
  REMAINDER_X = 16,
  REMAINDER_Y = 17,
  REMAINDER_Z = 18,
  GLOBAL_OFFSET_X = 19,
  GLOBAL_OFFSET_Y = 20,
  GLOBAL_OFFSET_Z = 21,
  PRINT_BUFFER = 22,
  HOSTCALL_BUFFER = 23,
  DEFAULT_QUEUE = 24,
  COMPLETION_ACTION = 25,
  MULTIGRID_SYNC = 26,
  GRID_DIMS = 27,
  HEAP_V1 = 28,
  DYNAMIC_LDS_SIZE = 29,
  PRIVATE_BASE = 30,
  SHARED_BASE = 31,
  HIDDEN_END = SHARED_BASE,
  WORK_ITEM_X = 32,
  WORK_ITEM_Y = 33,
  WORK_ITEM_Z = 34
};

/// \brief 包含有关 \c llvm::CallInst 对 Luthier Intrinsic 的调用所使用的/定义的值及其内联汇编约束（例如 'v'、's' 等）的信息
/// \details 此结构体用于跟踪 LLVM IR 值（由 \c llvm::CallInst 对 Luthier Intrinsic 的调用使用/定义）应如何映射到 \c llvm::Register；例如，如果 IR 调用指令使用的 <tt>%1</tt>
/// \code
/// %1 = tail call i32 @"luthier::myIntrinsic.i32"(i32 %0)
/// \endcode
/// 在 ISEL 传递完成后需要成为 SGPR，则 <tt>%1</tt> 将具有 <tt>'s'</tt> \c Constraint
/// \brief Contains information about the values used/defined by
/// a \c llvm::CallInst to a Luthier Intrinsic, and its inline assembly
/// constraint (e.g. 'v', 's', etc)
/// \details This struct is used to keep track of how an LLVM IR value
/// used/defined by a \c llvm::CallInst to a Luthier Intrinsic should be mapped
/// to a \c llvm::Register; For example,
/// if value <tt>%1</tt> used by the IR call instruction
/// \code
/// %1 = tail call i32 @"luthier::myIntrinsic.i32"(i32 %0)
/// \endcode
/// needs to become an SGPR after ISEL passes are finished, <tt>%1</tt> will
/// have an <tt>'s'</tt> \c Constraint
struct IntrinsicValueLoweringInfo {
  const llvm::Value *Val; ///< 要 lowering 的 IR 值 /// The IR value to be lowered
  std::string Constraint; ///< 描述如何 lowering \c Val 的内联 asm 约束 /// The inline asm constraint describing how \c Val
                          /// should be lowered
};

/// \brief 包含 intrinsic IR 调用指令的 IR 处理阶段输出的信息，包括 Luthier intrinsic 使用/定义的所有值（即其输出和输入参数）必须如何 lowering 到寄存器
/// \details 此结构体是 <tt>IntrinsicIRProcessorFunc</tt> 的返回值。在内部，\c luthier::CodeGenerator 存储所有 IR 处理器函数调用的结果，然后在 ISEL 传递完成后将它们传递给 <tt>IntrinsicMIRProcessorFunc</tt> 以在其位置生成 <tt>llvm::MachineInstr</tt>
/// \brief Holds information about the output of the IR processing stage of an
/// intrinsic IR call instruction,
/// including how all values used/defined by a Luthier
/// intrinsic use (i.e. its output and input arguments) must be
/// lowered to registers
/// \details This struct is the return value for the
/// <tt>IntrinsicIRProcessorFunc</tt>. Internally, \c luthier::CodeGenerator
/// stores the results of all IR processor function calls, and then passes
/// them to the <tt>IntrinsicMIRProcessorFunc</tt> after ISEL passes are
/// complete to generate <tt>llvm::MachineInstr</tt>s in its place
struct IntrinsicIRLoweringInfo {
private:
  /// intrinsic 的名称；用于 \c luthier::CodeGenerator 在 MIR 阶段跟踪 lowering 操作
  /// Name of the intrinsic; Used by \c luthier::CodeGenerator for keeping
  /// track of the lowering operation at the MIR stage
  std::string IntrinsicName{};
  /// 用作 intrinsic 占位符的内联汇编，直到指令选择之后；用于 \c luthier::CodeGenerator
  /// The inline assembly that serves as a place holder for the intrinsic
  /// until after instruction selection; Used by \c luthier::CodeGenerator
  const llvm::InlineAsm *PlaceHolderInlineAsm{nullptr};
  /// 输出值（如果存在）必须如何 lowering 到 \c llvm::Register
  /// How the output value (if present) must be lowered to a
  /// \c llvm::Register
  IntrinsicValueLoweringInfo OutValue{nullptr, ""};
  /// 参数值（如果存在）必须如何 lowering 到 \c llvm::Register
  /// How the argument values (if present) must be lowered to a
  /// \c llvm::Register
  llvm::SmallVector<IntrinsicValueLoweringInfo, 4> Args{};
  /// 任意数据（如果需要）从 IR 处理阶段传递到 MIR 处理阶段
  /// An arbitrary data (if needed) to be passed from the IR processing stage to
  /// the MIR processing stage
  llvm::Any Data{};

  /// 需要被此 intrinsic 访问的物理寄存器集合
  /// A set of physical registers that needs to be accessed by this intrinsic
  llvm::SmallDenseSet<llvm::MCRegister, 4> AccessedPhysicalRegisters{};

  /// 需要被此 intrinsic 访问的内核参数集合
  /// A set of kernel arguments that needs to be accessed by this intrinsic
  llvm::SmallDenseSet<KernelArgumentType, 4> AccessedKernelArguments{};

public:
  /// \param Name 被 lowering 的 intrinsic 的名称
  /// \note 此函数在返回 \c IntrinsicIRProcessorFunc 结果后由 Luthier 内部调用；因此在 IR 处理器中设置 intrinsic 的名称没有效果
  /// \param Name the name of the intrinsic being lowered
  /// \note this function is called internally by Luthier on the result of
  /// \c IntrinsicIRProcessorFunc is returned; Hence setting the name of the
  /// intrinsic inside the IR processor has no effect
  void setIntrinsicName(llvm::StringRef Name) { this->IntrinsicName = Name; }

  /// \returns 被 lowering 的 intrinsic 的名称
  /// \returns the name of the intrinsic being lowered
  [[nodiscard]] llvm::StringRef getIntrinsicName() const {
    return IntrinsicName;
  }

  /// 设置内联汇编占位符指令
  /// Sets the inline assembly placeholder instruction
  void setPlaceHolderInlineAsm(llvm::InlineAsm &IA) {
    this->PlaceHolderInlineAsm = &IA;
  }

  /// 获取内联汇编占位符指令
  /// Gets the inline assembly placeholder instruction
  [[nodiscard]] const llvm::InlineAsm &getPlaceHolderInlineAsm() const {
    return *this->PlaceHolderInlineAsm;
  }

  /// 为给定的 \p Val 设置内联 asm 约束为 \p Constraint
  /// Sets the inline asm constraint to \p Constraint for the given
  /// \p Val
  void setReturnValueInfo(const llvm::Value *Val, llvm::StringRef Constraint) {
    OutValue.Val = Val;
    OutValue.Constraint = Constraint;
  }

  /// \returns 返回值的 \c IntrinsicValueLoweringInfo
  /// \returns the return value's \c IntrinsicValueLoweringInfo
  [[nodiscard]] const IntrinsicValueLoweringInfo &getReturnValueInfo() const {
    return OutValue;
  }

  /// 添加带有 \p Val 和描述其 \c IntrinsicValueLoweringInfo 的 \p Constraint 的新参数
  /// Adds a new argument, with \p Val and \p Constraint describing its
  /// \c IntrinsicValueLoweringInfo
  void addArgInfo(const llvm::Value *Val, llvm::StringRef Constraint) {
    Args.emplace_back(Val, std::string(Constraint));
  }

  /// \returns 所有参数的 \c IntrinsicValueLoweringInfo
  /// \returns All arguments' \c IntrinsicValueLoweringInfo
  llvm::ArrayRef<IntrinsicValueLoweringInfo> getArgsInfo() const {
    return Args;
  }

  /// 将 lowering 数据设置为 \p D
  /// lowering 数据在发出机器指令时对 \c IntrinsicMIRProcessorFunc 可用
  /// Sets the lowering data to \p D
  /// The lowering data is made available to the \c IntrinsicMIRProcessorFunc
  template <typename T> void setLoweringData(T D) { Data = D; }

  /// \returns 将 lowering 数据，在发出机器指令时将对 \c IntrinsicMIRProcessorFunc 可用
  /// \returns the lowering data which will be made available to the
  /// \c IntrinsicMIRProcessorFunc when emitting Machine Instructions
  template <typename T> const T &getLoweringData() const {
    return *llvm::any_cast<T>(&Data);
  }

  /// 要求代码生成器在 MIR lowering 阶段确保对 \p PhysReg 的访问
  /// Asks the code generator to ensure access to the \p PhysReg during
  /// the MIR lowering stage
  void requestAccessToPhysicalRegister(llvm::MCRegister PhysReg) {
    AccessedPhysicalRegisters.insert(PhysReg);
  }

  /// intrinsic 访问的物理寄存器的迭代器/查询函数
  /// Iterators/Query functions for the physical registers accessed by the
  /// intrinsic

  using const_accessed_phys_regs_iterator =
      decltype(AccessedPhysicalRegisters)::ConstIterator;

  [[nodiscard]] llvm::iterator_range<const_accessed_phys_regs_iterator>
  accessed_phys_regs() const {
    return llvm::make_range(accessed_phys_regs_begin(),
                            accessed_phys_regs_end());
  }

  [[nodiscard]] const_accessed_phys_regs_iterator
  accessed_phys_regs_begin() const {
    return AccessedPhysicalRegisters.begin();
  }

  [[nodiscard]] const_accessed_phys_regs_iterator
  accessed_phys_regs_end() const {
    return AccessedPhysicalRegisters.end();
  }

  [[nodiscard]] bool accessed_phys_regs_empty() const {
    return AccessedPhysicalRegisters.empty();
  }

  [[nodiscard]] size_t accessed_phys_regs_size() const {
    return AccessedPhysicalRegisters.size();
  }

  /// 要求代码生成器在 MIR lowering 阶段确保对 \p KernArg 的访问
  /// Asks the code generator to ensure access to the \p KernArg during
  /// the MIR lowering stage
  void requestAccessToKernelArgument(KernelArgumentType KernArg) {
    AccessedKernelArguments.insert(KernArg);
  }

  /// intrinsic 访问的内核参数的迭代器/查询函数
  /// Iterators/Query functions for the kernel arguments accessed by the
  /// intrinsic

  using const_accessed_kernargs_iterator =
      decltype(AccessedKernelArguments)::ConstIterator;

  [[nodiscard]] const_accessed_kernargs_iterator
  accessed_kernargs_begin() const {
    return AccessedKernelArguments.begin();
  }

  [[nodiscard]] const_accessed_kernargs_iterator accessed_kernargs_end() const {
    return AccessedKernelArguments.end();
  }

  [[nodiscard]] bool accessed_kernargs_empty() const {
    return AccessedKernelArguments.empty();
  }

  [[nodiscard]] size_t accessed_kernargs_size() const {
    return AccessedKernelArguments.size();
  }
};

/// \brief 描述每个 Luthier intrinsic 用于处理其在 LLVM IR 中的使用的函数类型，并返回描述其使用/定义值如何 lowering 到 <tt>llvm::MachineOperand</tt> 的 \c IntrinsicIRLoweringInfo，以及从 IR 处理阶段传递到 MIR 处理阶段所需的任意信息
/// \brief describes a function type used by each Luthier intrinsic to process
/// its uses in LLVM IR, and return a \c IntrinsicIRLoweringInfo which will
/// describe how its use/def values will be lowered to
/// <tt>llvm::MachineOperand</tt>s, as well as any arbitrary information
/// required to be passed down from the IR processing stage to the MIR
/// processing stage
typedef std::function<llvm::Expected<IntrinsicIRLoweringInfo>(
    const llvm::Function &, const llvm::CallInst &,
    const llvm::GCNTargetMachine &)>
    IntrinsicIRProcessorFunc;

/// \brief 描述用于每个 intrinsic 的函数类型，以生成 <tt>llvm::MachineInstr</tt> 来代替其 IR 调用。
/// MIR 处理器接收其 <c IntrinsicIRProcessorFunc 生成的 <c IntrinsicIRLoweringInfo，以及 lowering 的寄存器及其内联汇编标志（用于其使用/定义的值）。还传递了一个 lambda，用于在给定指令操作码的发射位置创建 <c llvm::MachineInstr
/// \brief describes a function type used for each intrinsic to generate
/// <tt>llvm::MachineInstr</tt>s in place of its IR calls.
/// The MIR processor takes in the
/// \c IntrinsicIRLoweringInfo generated by its \c IntrinsicIRProcessorFunc as
/// well as the lowered registers and their inline assembly flags for
/// its used/defined values. A lambda which will create an
/// \c llvm::MachineInstr at the place of emission given an instruction opcode
/// is also passed to this function
typedef std::function<llvm::Error(
    const IntrinsicIRLoweringInfo &,
    llvm::ArrayRef<std::pair<llvm::InlineAsm::Flag, llvm::Register>>,
    const std::function<llvm::MachineInstrBuilder(int)> &,
    const std::function<llvm::Register(const llvm::TargetRegisterClass *)> &,
    const std::function<llvm::Register(KernelArgumentType)> &,
    const llvm::MachineFunction &,
    const std::function<llvm::Register(llvm::MCRegister)> &,
    llvm::DenseMap<llvm::MCRegister, llvm::Register> &)>
    IntrinsicMIRProcessorFunc;

/// \brief 在 \c luthier::CodeGenerator 内部使用，以跟踪注册的 intrinsic 及其处理方式
/// \brief Used internally by \c luthier::CodeGenerator to keep track of
/// registered intrinsics and how to process them
struct IntrinsicProcessor {
  IntrinsicIRProcessorFunc IRProcessor{};
  IntrinsicMIRProcessorFunc MIRProcessor{};
};

/// 如果传递的 MI 是内联汇编指令并且是 Luthier intrinsic 的占位符，则返回与其关联的唯一索引
/// \param MI 被检查的 \c llvm::MachineInstr
/// \return MI 内联汇编字符串中的唯一索引，如果 \p MI 不是内联汇编或其内联汇编字符串为空则返回 -1，如果其汇编字符串无法转换为无符号整数则返回 \c llvm::Error
/// If the passed MI is an inline assembly instruction and a place holder
/// for a Luthier intrinsic, returns the unique index associated with it
/// \param MI the \c llvm::MachineInstr being inspected
/// \return the unique index in the MI's inline assembly string, -1 if
/// \p MI is not an inline assembly or its inline assembly string is empty,
/// or an \c llvm::Error if its assembly string fails to convert to an
/// unsigned int
llvm::Expected<unsigned int>
getIntrinsicInlineAsmPlaceHolderIdx(const llvm::MachineInstr &MI);

} // namespace luthier

#endif
