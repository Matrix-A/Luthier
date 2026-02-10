//===-- Instr.h - HSA Instruction  ------------------------------*- C++ -*-===//
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
/// \brief 本文件在 \c luthier::hsa 命名空间下描述了 \c Instr 类，
/// 它通过解析加载的 \c LoadedCodeObjectSymbol（类型为 \c LoadedCodeObjectSymbol::SK_KERNEL 或
/// <tt>LoadedCodeObjectSymbol::SK_DEVICE_FUNCTION</tt>）的内容，跟踪由 LLVM 反汇编的指令。
/// This file describes the \c Instr class under the \c luthier::hsa namespace,
/// which keeps track of an instruction disassembled by LLVM via parsing the
/// contents of the loaded contents of a \c LoadedCodeObjectSymbol of type
/// \c LoadedCodeObjectSymbol::SK_KERNEL or
/// <tt>LoadedCodeObjectSymbol::SK_DEVICE_FUNCTION</tt>
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_HSA_INSTR_H
#define LUTHIER_HSA_INSTR_H
#include <llvm/MC/MCInst.h>
#include <llvm/Support/Error.h>
#include <luthier/types.h>

namespace luthier::hsa {

class LoadedCodeObjectSymbol;

class LoadedCodeObjectKernel;

class LoadedCodeObjectDeviceFunction;

/// \brief 表示通过检查加载到设备内存中的类型为 \c SK_KERNEL 或 \c SK_DEVICE_FUNCTION 的
/// \c LoadedCodeObjectSymbol 的内容而反汇编的指令
/// \details 当对函数符号调用 \c luthier::disassemble 或 <tt>luthier::lift</tt> 时会创建 \c Instr。
/// 当符号被反汇编时，Luthier 会在内部创建此类的实例来保存反汇编的指令，并缓存它们直到支持符号的
/// \c hsa_executable_t 被 HSA 运行时销毁。
/// \brief represents an instruction that was disassembled by inspecting the
/// contents of a \c LoadedCodeObjectSymbol of type \c SK_KERNEL or
/// \c SK_DEVICE_FUNCTION loaded on device memory
/// \details \c Instr is created when calling \c luthier::disassemble or
/// <tt>luthier::lift</tt> on a function symbol. When a symbol is disassembled,
/// Luthier internally creates instances of this class to hold the disassembled
/// instructions and caches them until the \c hsa_executable_t backing the
/// symbol is destroyed by the HSA runtime.
class Instr {
private:
  /// 指令的 MC 表示
  /// The MC representation of the instruction
  const llvm::MCInst Inst;
  /// 此指令加载到的 GPU Agent 上的地址
  /// The address on the GPU Agent this instruction is loaded at
  const address_t LoadedDeviceAddress;
  /// 此指令所属的符号
  /// The symbol this instruction belongs to
  const LoadedCodeObjectSymbol &Symbol;
  /// 指令大小
  /// Size of the instruction
  const size_t Size;
  // TODO: 当代码提升器中实现 DWARF 解析时，添加 DWARF 信息
  // TODO: add DWARF information when DWARF parsing is implemented in the
  // code lifter

public:
  /// 删除默认构造函数
  /// Deleted default constructor
  Instr() = delete;

  /// 构造函数
  /// \param Inst 指令的 \c MCInst
  /// \param Kernel 此指令所属的内核
  /// \param Address 指令加载到的设备地址
  /// \param Size 指令的字节大小
  /// Constructor
  /// \param Inst \c MCInst of the instruction
  /// \param Kernel the kernel this instruction belongs to
  /// \param Address the device address this instruction is loaded on
  /// \param Size size of the instruction in bytes
  Instr(llvm::MCInst Inst, const LoadedCodeObjectKernel &Kernel,
        address_t Address, size_t Size);

  /// 构造函数
  /// \param Inst 指令的 \c MCInst
  /// \param DeviceFunction 此指令所属的设备函数
  /// \param Address 指令加载到的设备地址
  /// \param Size 指令的字节大小
  /// Constructor
  /// \param Inst \c MCInst of the instruction
  /// \param DeviceFunction the device function this instruction belongs to
  /// \param Address the device address this instruction is loaded on
  /// \param Size size of the instruction in bytes
  Instr(llvm::MCInst Inst, const LoadedCodeObjectDeviceFunction &DeviceFunction,
        address_t Address, size_t Size);

  /// \return 此指令所属的设备函数/内核
  /// \return the device function/kernel that this instruction belongs to
  [[nodiscard]] const LoadedCodeObjectSymbol &getLoadedCodeObjectSymbol() const;

  /// \return 指令的 MC 表示
  /// \return the MC representation of the instruction
  [[nodiscard]] llvm::MCInst getMCInst() const;

  /// \return 指令在设备上的加载地址
  /// \note 可以从该指令的支持符号查询指令的 \c hsa_agent_t
  /// \return the loaded address of this instruction on the device
  /// \note the \c hsa_agent_t of the instruction can be queried from the
  /// this instruction's backing symbol
  [[nodiscard]] address_t getLoadedDeviceAddress() const;

  /// \return 指令的字节大小
  /// \return the size of the instruction in bytes
  [[nodiscard]] size_t getSize() const;
};

} // namespace luthier::hsa

#endif
