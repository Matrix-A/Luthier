//===-- GenericLuthierError.h -----------------------------------*- C++ -*-===//
// GenericLuthierError 通用错误类头文件
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
/// Describes <tt>GenericLuthierError</tt>, which represents generic Luthier
/// errors not related third-party libraries used by Luthier.
/// 描述 <tt>GenericLuthierError</tt>，它表示与 Luthier 使用的第三方库无关的通用 Luthier 错误
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_COMMON_GENERIC_LUTHIER_ERROR_H
#define LUTHIER_COMMON_GENERIC_LUTHIER_ERROR_H
#include "luthier/Common/LuthierError.h"
#include <llvm/Support/FormatVariadic.h>

namespace luthier {

/// \brief Error used to indicate generic issues encountered in Luthier code not
/// related to any other library
/// 用于指示 Luthier 代码中遇到的与任何其他库无关的通用问题的错误
class GenericLuthierError final : public LuthierError {

public:
  static char ID;

  explicit GenericLuthierError(
      std::string ErrorMsg,
      const std::source_location ErrorLocation =
          std::source_location::current(),
      StackTraceType StackTrace = StackTraceInitializer())
      : LuthierError(std::move(ErrorMsg), ErrorLocation,
                     std::move(StackTrace)) {};

  explicit GenericLuthierError(
      const llvm::formatv_object_base &FormatObject,
      const std::source_location ErrorLocation =
          std::source_location::current(),
      StackTraceType StackTrace = StackTraceInitializer())
      : LuthierError(std::move(FormatObject.str()), ErrorLocation,
                     std::move(StackTrace)) {};

  void log(llvm::raw_ostream &OS) const override;
};

#define LUTHIER_MAKE_GENERIC_ERROR(ErrorMsg)                                   \
  llvm::make_error<luthier::GenericLuthierError>(                              \
      ErrorMsg, std::source_location::current(),                               \
      luthier::GenericLuthierError::StackTraceInitializer())

#define LUTHIER_GENERIC_ERROR_CHECK(Expr, ErrorMsg)                            \
  (Expr) ? llvm::Error::success() : LUTHIER_MAKE_GENERIC_ERROR(ErrorMsg)

} // namespace luthier

#endif