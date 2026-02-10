//===-- HsaError.h ----------------------------------------------*- C++ -*-===//
// HsaError.h HSA 错误头文件
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
/// Defines a \c llvm::ErrorInfo for holding information regarding
/// issues encountered with using HSA APIs.
/// 定义 \c llvm::ErrorInfo 以保存使用 HSA API 时遇到的问题信息
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_HSA_HSA_ERROR_H
#define LUTHIER_HSA_HSA_ERROR_H
#include "luthier/Common/ROCmLibraryError.h"
#include <hsa/hsa.h>

namespace luthier::hsa {
class HsaError final : public RocmLibraryError {
  const std::optional<hsa_status_t> Error;

public:
  explicit HsaError(std::string ErrorMsg,
                    const std::optional<hsa_status_t> Error = std::nullopt,
                    const std::source_location ErrorLocation =
                        std::source_location::current(),
                    StackTraceType StackTrace = StackTraceInitializer())
      : RocmLibraryError(std::move(ErrorMsg), ErrorLocation,
                         std::move(StackTrace)),
        Error(Error) {};

  explicit HsaError(const llvm::formatv_object_base &ErrorMsg,
                    const std::optional<hsa_status_t> Error = std::nullopt,
                    const std::source_location ErrorLocation =
                        std::source_location::current(),
                    StackTraceType StackTrace = StackTraceInitializer())
      : RocmLibraryError(ErrorMsg.str(), ErrorLocation, std::move(StackTrace)),
        Error(Error) {};

  static char ID;

  void log(llvm::raw_ostream &OS) const override;
};

#define LUTHIER_HSA_CALL_ERROR_CHECK(Expr, ErrorMsg)                           \
  [&]() -> llvm::Error {                                                       \
    if (const hsa_status_t Status = Expr; Status != HSA_STATUS_SUCCESS) {      \
      return llvm::make_error<luthier::hsa::HsaError>(                         \
          ErrorMsg, Status, std::source_location::current(),                   \
          luthier::hsa::HsaError::StackTraceInitializer());                    \
    }                                                                          \
    return llvm::Error::success();                                             \
  }()

#define LUTHIER_HSA_ERROR_CHECK(Expr, ErrorMsg)                                \
  [&]() -> llvm::Error {                                                       \
    if (!(Expr)) {                                                             \
      return llvm::make_error<luthier::hsa::HsaError>(                         \
          ErrorMsg, std::nullopt, std::source_location::current(),             \
          luthier::hsa::HsaError::StackTraceInitializer());                    \
    }                                                                          \
    return llvm::Error::success();                                             \
  }()

} // namespace luthier::hsa

#endif