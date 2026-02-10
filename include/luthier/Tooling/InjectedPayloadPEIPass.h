//===-- InjectedPayloadPEIPass.h --------------------------------*- C++ -*-===//
// InjectedPayloadPEIPass.h 注入负载PEI传递头文件
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
/// This file describes Luthier's Injected Payload Prologue and Epilogue
/// insertion pass, which replaces the normal prologues and epilogues insertion
/// by the CodeGen pipeline.
/// 此文件描述 Luthier 的注入负载序言和尾声插入传递，它替换了 CodeGen 流水线的
/// 正常序言和尾声插入
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_TOOLING_INJECTED_PAYLOAD_PEI_PASS_H
#define LUTHIER_TOOLING_INJECTED_PAYLOAD_PEI_PASS_H
#include "luthier/Intrinsic/IntrinsicProcessor.h"
#include "luthier/Tooling/AMDGPURegisterLiveness.h"
#include "luthier/Tooling/LRCallgraph.h"
#include "luthier/Tooling/LiftedRepresentation.h"
#include "luthier/Tooling/PhysicalRegAccessVirtualizationPass.h"
#include "luthier/Tooling/PrePostAmbleEmitter.h"
#include "luthier/Tooling/SVStorageAndLoadLocations.h"
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/IR/PassManager.h>

namespace luthier {

class PhysicalRegAccessVirtualizationPass;

class InjectedPayloadPEIPass : public llvm::MachineFunctionPass {
private:
  PhysicalRegAccessVirtualizationPass &PhysRegVirtAccessPass;

public:
  static char ID;

  explicit InjectedPayloadPEIPass(
      PhysicalRegAccessVirtualizationPass &PhysRegVirtAccessPass)
      : llvm::MachineFunctionPass(ID),
        PhysRegVirtAccessPass(PhysRegVirtAccessPass) {};

  [[nodiscard]] llvm::StringRef getPassName() const override {
    return "Luthier Injected Payload Prologue Epilogue Insertion Pass";
  }

  bool runOnMachineFunction(llvm::MachineFunction &MF) override;

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
};

} // namespace luthier

#endif