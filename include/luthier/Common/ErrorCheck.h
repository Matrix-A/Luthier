//===-- ErrorCheck.h - Error Checking Macros  -------------------*- C++ -*-===//
// 错误检查宏定义头文件
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
/// Defines useful macros to check <tt>llvm::Error</tt>s.
/// 定义用于检查 llvm::Error 的实用宏
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_COMMON_ERROR_CHECK_H
#define LUTHIER_COMMON_ERROR_CHECK_H

/// \brief Reports a fatal error if the passed \p llvm::Error argument is not
/// equal to \c llvm::ErrorSuccess
/// 如果传入的 \p llvm::Error 参数不等于 \c llvm::ErrorSuccess，则报告致命错误
#define LUTHIER_REPORT_FATAL_ON_ERROR(Error)                                   \
  do {                                                                         \
    if (auto ___E = std::move(Error)) {                                        \
      llvm::report_fatal_error(std::move(___E), true);                         \
    }                                                                          \
  } while (false)

/// \brief returns from the function if the passed \p llvm::Error argument is
/// not equal to \c llvm::ErrorSuccess
/// 如果传入的 \p llvm::Error 参数不等于 \c llvm::ErrorSuccess，则从函数返回
#define LUTHIER_RETURN_ON_ERROR(Error)                                         \
  do {                                                                         \
    if (auto ___E = std::move(Error)) {                                        \
      return std::move(___E);                                                  \
    }                                                                          \
  } while (false)

/// \brief Emits an error into the LLVM Context if \p Error is not equal to
/// \c llvm::ErrorSuccess
/// 如果 \p Error 不等于 \c llvm::ErrorSuccess，则在 LLVM 上下文中发出错误
#define LUTHIER_EMIT_ERROR_IN_CONTEXT(Ctx, Error)                              \
  do {                                                                         \
    if (auto ___E = std::move(Error)) {                                        \
      (Ctx).emitError(llvm::toString(std::move(___E)));                        \
    }                                                                          \
  } while (false)

#endif