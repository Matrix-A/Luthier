//===-- LoadedCodeObjectVariable.h - LCO Variable Symbol --------*- C++ -*-===//
// LoadedCodeObjectVariable.h LCO 变量符号头文件
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
/// This file defines the \c LoadedCodeObjectVariable under the
/// \c luthier::hsa namespace, which represents all device variable symbols
/// inside a <tt>hsa::LoadedCodeObject</tt>.
/// 此文件在 \c luthier::hsa 命名空间下定义 \c LoadedCodeObjectVariable，
/// 它表示 <tt>hsa::LoadedCodeObject</tt> 内部的所有设备变量符号
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_LOADED_CODE_OBJECT_VARIABLE_H
#define LUTHIER_LOADED_CODE_OBJECT_VARIABLE_H
#include "luthier/HSA/LoadedCodeObjectSymbol.h"

namespace luthier::hsa {

/// \brief a \c LoadedCodeObjectSymbol of type
/// \c LoadedCodeObjectSymbol::ST_DEVICE_FUNCTION
/// 类型为 \c LoadedCodeObjectSymbol::SK_VARIABLE 的 \c LoadedCodeObjectSymbol
class LoadedCodeObjectVariable final : public LoadedCodeObjectSymbol {

private:
  /// Constructor
  /// \param LCO the \c hsa_loaded_code_object_t this symbol belongs to
  /// \param VarSymbol the symbol of the variable,
  /// cached internally by Luthier
  /// \param ExecutableSymbol the \c hsa_executable_symbol_t equivalent of
  /// the variable symbol, if exists
  /// 构造函数
  /// \param LCO 符号所属的 \c hsa_loaded_code_object_t
  /// \param VarSymbol 变量的符号，由 Luthier 内部缓存
  /// \param ExecutableSymbol 变量符号的 \c hsa_executable_symbol_t 等效项（如果存在）
  LoadedCodeObjectVariable(
      hsa_loaded_code_object_t LCO,
      luthier::object::AMDGCNObjectFile &StorageElf,
      llvm::object::ELFSymbolRef VarSymbol,
      std::optional<hsa_executable_symbol_t> ExecutableSymbol)
      : LoadedCodeObjectSymbol(LCO, StorageElf, VarSymbol,
                               SymbolKind::SK_VARIABLE, ExecutableSymbol) {}

public:
  static llvm::Expected<std::unique_ptr<LoadedCodeObjectVariable>>
  create(const ApiTableContainer<::CoreApiTable> &CoreApiTable,
         const hsa_ven_amd_loader_1_03_pfn_t &VenLoaderApi,
         hsa_loaded_code_object_t LCO,
         luthier::object::AMDGCNObjectFile &StorageElf,
         llvm::object::ELFSymbolRef VarSymbol);

  /// method for providing LLVM RTTI
  /// 提供 LLVM RTTI 的方法
  [[nodiscard]] static bool classof(const LoadedCodeObjectSymbol *S) {
    return S->getType() == SK_VARIABLE;
  }
};

} // namespace luthier::hsa

#endif