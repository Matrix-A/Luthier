//===-- LoadedCodeObjectDeviceFunction.h - LCO Device Function --*- C++ -*-===//
// LoadedCodeObjectDeviceFunction.h LCO 设备函数头文件
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
/// This file defines the \c LoadedCodeObjectDeviceFunction under the
/// \c luthier::hsa namespace, which represents all device (non-kernel)
/// functions inside a \c hsa::LoadedCodeObject.
/// 此文件在 \c luthier::hsa 命名空间下定义 \c LoadedCodeObjectDeviceFunction，
/// 它表示 \c hsa::LoadedCodeObject 内部的所有设备（非内核）函数
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_LOADED_CODE_OBJECT_DEVICE_FUNCTION_H
#define LUTHIER_LOADED_CODE_OBJECT_DEVICE_FUNCTION_H
#include "luthier/HSA/LoadedCodeObjectSymbol.h"
#include "luthier/Object/AMDGCNObjectFile.h"

namespace luthier::hsa {

/// \brief a \c LoadedCodeObjectSymbol of type
/// \c LoadedCodeObjectSymbol::ST_DEVICE_FUNCTION
/// 类型为 \c LoadedCodeObjectSymbol::SK_DEVICE_FUNCTION 的 \c LoadedCodeObjectSymbol
class LoadedCodeObjectDeviceFunction final : public LoadedCodeObjectSymbol {

private:
  /// Constructor
  /// \param LCO the \c hsa_loaded_code_object_t this symbol belongs to
  /// \param FuncSymbol the function symbol of the device function,
  /// cached internally by Luthier
  /// 构造函数
  /// \param LCO 符号所属的 \c hsa_loaded_code_object_t
  /// \param FuncSymbol 设备函数的函数符号，由 Luthier 内部缓存
  LoadedCodeObjectDeviceFunction(hsa_loaded_code_object_t LCO,
                                 luthier::object::AMDGCNObjectFile &StorageElf,
                                 llvm::object::ELFSymbolRef FuncSymbol)
      : LoadedCodeObjectSymbol(LCO, StorageElf, FuncSymbol,
                               SymbolKind::SK_DEVICE_FUNCTION, std::nullopt) {}

public:
  /// Factory method used internally by Luthier
  /// Symbols created using this method will be cached, and a reference to them
  /// will be returned to the tool writer when queried
  /// \param LCO the \c hsa_loaded_code_object_t this symbol belongs to
  /// \param FuncSymbol the function symbol of the device function,
  /// cached internally by Luthier
  /// Luthier 内部使用的工厂方法
  /// 使用此方法创建的符号将被缓存，当查询时将返回给工具编写者的引用
  static llvm::Expected<std::unique_ptr<LoadedCodeObjectDeviceFunction>>
  create(hsa_loaded_code_object_t LCO,
         luthier::object::AMDGCNObjectFile &StorageElf,
         llvm::object::ELFSymbolRef FuncSymbol);

  /// method for providing LLVM RTTI
  /// 提供 LLVM RTTI 的方法
  __attribute__((used)) static bool classof(const LoadedCodeObjectSymbol *S) {
    return S->getType() == SK_DEVICE_FUNCTION;
  }
};

} // namespace luthier::hsa

#endif