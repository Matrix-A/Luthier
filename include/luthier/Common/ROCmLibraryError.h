//===-- RocmLibraryError.h --------------------------------------*- C++ -*-===//
// RocmLibraryError ROCm 库错误类头文件
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
/// This file describes the \c RocmLibraryError class,
/// used as a base class for creating specialized \c llvm::ErrorInfo for
/// ROCm libraries.
/// 此文件描述 \c RocmLibraryError 类，用于为 ROCm 库创建专门的 \c llvm::ErrorInfo 基类
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_COMMON_ROCM_LIBRARY_ERROR_H
#define LUTHIER_COMMON_ROCM_LIBRARY_ERROR_H
#include "luthier/Common/LuthierError.h"

namespace luthier {

class RocmLibraryError : public LuthierError {
protected:
  explicit RocmLibraryError(std::string ErrorMsg,
                            const std::source_location ErrorLocation =
                                std::source_location::current(),
                            StackTraceType StackTrace = StackTraceInitializer())
      : LuthierError(std::move(ErrorMsg), ErrorLocation,
                     std::move(StackTrace)) {};

  explicit RocmLibraryError(const llvm::formatv_object_base &FormatObject,
                            const std::source_location ErrorLocation =
                                std::source_location::current(),
                            StackTraceType StackTrace = StackTraceInitializer())
      : LuthierError(std::move(FormatObject.str()), ErrorLocation,
                     std::move(StackTrace)) {};

public:
  static char ID;
};

} // namespace luthier

#endif