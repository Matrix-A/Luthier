//===-- MIRConvenience.h ----------------------------------------*- C++ -*-===//
// MIRConvenience.h MIR 便捷函数头文件
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
/// This file contains a set of high-level convenience functions used to write
/// MIR instructions.
/// 此文件包含一组高级便捷函数，用于编写 MIR 指令
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_TOOLING_MIR_CONVENIENCE_H
#define LUTHIER_TOOLING_MIR_CONVENIENCE_H
#include <llvm/CodeGen/MachineBasicBlock.h>

namespace llvm {

class MCRegister;

}

namespace luthier {

/// Swaps the value between \p ScrSGPR and \p DestSGPR by inserting 3
/// <tt>S_XOR_B32</tt>s before \p InsertionPoint
/// 通过在 \p InsertionPoint 之前插入 3 条 <tt>S_XOR_B32</tt> 指令来交换 \p ScrSGPR 和 \p DestSGPR 之间的值
void emitSGPRSwap(llvm::MachineBasicBlock::iterator InsertionPoint,
                  llvm::MCRegister SrcSGPR, llvm::MCRegister DestSGPR);

/// Swaps the value between \p ScrVGPR and \p DestVGPR by inserting 3
/// <tt>V_XOR_B32_e32</tt>s before \p InsertionPoint
/// 通过在 \p InsertionPoint 之前插入 3 条 <tt>V_XOR_B32_e32</tt> 指令来交换 \p ScrVGPR 和 \p DestVGPR 之间的值
void emitVGPRSwap(llvm::MachineBasicBlock::iterator InsertionPoint,
                  llvm::MCRegister SrcVGPR, llvm::MCRegister DestVGPR);

/// Emits an instruction that flips the exec mask before \p MI
/// Clobbers the SCC bit
/// 在 \p MI 之前发出翻转 exec 掩码的指令
/// 会破坏 SCC 位
void emitExecMaskFlip(llvm::MachineBasicBlock::iterator MI);

void emitMoveFromVGPRToVGPR(llvm::MachineBasicBlock::iterator MI,
                            llvm::MCRegister SrcVGPR, llvm::MCRegister DestVGPR,
                            bool KillSource);

void emitMoveFromSGPRToSGPR(llvm::MachineBasicBlock::iterator MI,
                            llvm::MCRegister SrcSGPR, llvm::MCRegister DestSGPR,
                            bool KillSource);

void emitMoveFromAGPRToVGPR(llvm::MachineBasicBlock::iterator MI,
                            llvm::MCRegister SrcAGPR, llvm::MCRegister DestVGPR,
                            bool KillSource);

void emitMoveFromVGPRToAGPR(llvm::MachineBasicBlock::iterator MI,
                            llvm::MCRegister SrcVGPR, llvm::MCRegister DestAGPR,
                            bool KillSource = true);

void emitMoveFromSGPRToVGPRLane(llvm::MachineBasicBlock::iterator MI,
                                llvm::MCRegister SrcSGPR,
                                llvm::MCRegister DestVGPR, unsigned int Lane,
                                bool KillSource);

void emitMoveFromVGPRLaneToSGPR(llvm::MachineBasicBlock::iterator MI,
                                llvm::MCRegister SrcVGPR,
                                llvm::MCRegister DestSGPR, unsigned int Lane,
                                bool KillSource);

/// Generates a set of MBBs that ensures the \c llvm::AMDGPU::SCC bit does not
/// get clobbered due to the sequence of instructions built by \p MIBuilder
/// before the insertion point \p MI
/// This is a common pattern used when loading and storing the state value
/// array that allows flipping the exec mask without clobbering the
/// \c SCC bit and not requiring temporary registers
/// \returns the iterator where all paths emitted converge together
/// 生成一组 MBB，确保 \c llvm::AMDGPU::SCC 位不会因为 \p MIBuilder 在插入点 \p MI
/// 之前构建的指令序列而被破坏
/// 这是在加载和存储状态值数组时使用的常见模式，允许翻转 exec 掩码而不破坏
/// \c SCC 位，也不需要临时寄存器
/// \returns 所有发出的路径汇聚在一起的迭代器
llvm::MachineBasicBlock::iterator createSCCSafeSequenceOfMIs(
    llvm::MachineBasicBlock::iterator MI,
    const std::function<void(llvm::MachineBasicBlock &,
                             const llvm::TargetInstrInfo &)> &MIBuilder);

void emitLoadFromEmergencyVGPRScratchSpillLocation(
    llvm::MachineBasicBlock::iterator MI, llvm::MCRegister StackPtr,
    llvm::MCRegister DestVGPR);

void emitStoreToEmergencyVGPRScratchSpillLocation(
    llvm::MachineBasicBlock::iterator MI, llvm::MCRegister StackPtr,
    llvm::MCRegister SrcVGPR, bool KillSource);

void emitLoadFromEmergencySVSScratchSpillLocation(
    llvm::MachineBasicBlock::iterator MI, llvm::MCRegister StackPtr,
    llvm::MCRegister DestVGPR);

void emitStoreToEmergencySVSScratchSpillLocation(
    llvm::MachineBasicBlock::iterator MI, llvm::MCRegister StackPtr,
    llvm::MCRegister SrcVGPR, bool KillSource);

void emitWaitCnt(llvm::MachineBasicBlock::iterator MI);

} // namespace luthier

#endif