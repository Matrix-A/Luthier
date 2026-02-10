//===-- LoadedCodeObjectExternSymbol.h - LCO External Symbol ----*- C++ -*-===//
// LoadedCodeObjectExternSymbol.h LCO 外部符号头文件
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
/// This file defines the \c LoadedCodeObjectExternSymbol under the
/// \c luthier::hsa namespace, which represents all symbols declared
/// inside a \c hsa::LoadedCodeObject but not defined.
/// 此文件在 \c luthier::hsa 命名空间下定义 \c LoadedCodeObjectExternSymbol，
/// 它表示 \c hsa::LoadedCodeObject 内部声明但未定义的所有符号
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_LOADED_CODE_OBJECT_EXTERN_SYMBOL_H
#define LUTHIER_LOADED_CODE_OBJECT_EXTERN_SYMBOL_H
#include "luthier/HSA/Agent.h"
#include "luthier/HSA/Executable.h"
#include "luthier/HSA/ExecutableSymbol.h"
#include "luthier/HSA/LoadedCodeObject.h"
#include "luthier/HSA/LoadedCodeObjectExternSymbol.h"
#include "luthier/HSA/LoadedCodeObjectSymbol.h"

namespace luthier::hsa {

/// \brief a \c LoadedCodeObjectSymbol of type
/// \c LoadedCodeObjectSymbol::SK_EXTERNAL
/// 类型为 \c LoadedCodeObjectSymbol::SK_EXTERNAL 的 \c LoadedCodeObjectSymbol
class LoadedCodeObjectExternSymbol final : public LoadedCodeObjectSymbol {

private:
  /// Constructor
  /// \param LCO the \c hsa_loaded_code_object_t this symbol belongs to
  /// \param ExternSymbol the external symbol,
  /// cached internally by Luthier
  /// \param ExecutableSymbol the \c hsa_executable_symbol_t equivalent of
  /// the extern symbol
  /// 构造函数
  /// \param LCO 符号所属的 \c hsa_loaded_code_object_t
  /// \param ExternSymbol 外部符号，由 Luthier 内部缓存
  /// \param ExecutableSymbol 外部符号的 \c hsa_executable_symbol_t 等效项
  LoadedCodeObjectExternSymbol(hsa_loaded_code_object_t LCO,
                               luthier::object::AMDGCNObjectFile &StorageElf,
                               llvm::object::ELFSymbolRef ExternSymbol,
                               hsa_executable_symbol_t ExecutableSymbol)
      : LoadedCodeObjectSymbol(LCO, StorageElf, ExternSymbol,
                               SymbolKind::SK_EXTERNAL, ExecutableSymbol) {}

public:
  static llvm::Expected<std::unique_ptr<LoadedCodeObjectExternSymbol>>
  create(const ApiTableContainer<::CoreApiTable> &CoreApiTable,
         const hsa_ven_amd_loader_1_03_pfn_t &VenLoaderApi,
         hsa_loaded_code_object_t LCO,
         luthier::object::AMDGCNObjectFile &StorageElf,
         llvm::object::ELFSymbolRef ExternSymbol) {
    // Get the executable symbol associated with this external symbol
    llvm::Expected<hsa_executable_t> ExecOrErr =
        hsa::loadedCodeObjectGetExecutable(VenLoaderApi, LCO);
    LUTHIER_RETURN_ON_ERROR(ExecOrErr.takeError());

    llvm::Expected<hsa_agent_t> AgentOrErr =
        hsa::loadedCodeObjectGetAgent(VenLoaderApi, LCO);
    LUTHIER_RETURN_ON_ERROR(AgentOrErr.takeError());

    auto Name = ExternSymbol.getName();
    LUTHIER_RETURN_ON_ERROR(Name.takeError());

    auto ExecSymbol = hsa::executableGetSymbolByName(CoreApiTable, *ExecOrErr,
                                                     *Name, *AgentOrErr);
    LUTHIER_RETURN_ON_ERROR(ExecSymbol.takeError());
    LUTHIER_RETURN_ON_ERROR(LUTHIER_GENERIC_ERROR_CHECK(
        ExecSymbol->has_value(),
        llvm::formatv("Failed to locate the external symbol {0} in its "
                      "executable using its name",
                      *Name)));

    return std::unique_ptr<LoadedCodeObjectExternSymbol>(
        new LoadedCodeObjectExternSymbol(LCO, StorageElf, ExternSymbol,
                                         **ExecSymbol));
  }

  /// method for providing LLVM RTTI
  /// 提供 LLVM RTTI 的方法
  [[nodiscard]] static bool classof(const LoadedCodeObjectSymbol *S) {
    return S->getType() == SK_EXTERNAL;
  }
};

} // namespace luthier::hsa

#endif