//===-- LuthierError.h ------------------------------------------*- C++ -*-===//
// LuthierError 基础错误类头文件
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
/// Defines <tt>LuthierError</tt>, containing the common part among all
/// \c llvm::ErrorInfo classes defined by Luthier, as well as RTTI mechanism
/// for checking whether a given \c llvm::Error originated from Luthier.
/// 定义 <tt>LuthierError</tt>，包含 Luthier 定义的所有 \c llvm::ErrorInfo 类的公共部分，
/// 以及用于检查给定 \c llvm::Error 是否来自 Luthier 的 RTTI 机制
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_COMMON_LUTHIER_ERROR_H
#define LUTHIER_COMMON_LUTHIER_ERROR_H
#include <llvm/Support/Error.h>
#include <llvm/Support/FormatVariadic.h>
#include <source_location>
/// Use the C++ stacktrace if it's supported by the compiler/standard;
/// 如果编译器/标准支持 C++ stacktrace，则使用它
/// Otherwise, use LLVM's stack trace printer
/// 否则使用 LLVM 的栈追踪打印器
#include <stacktrace>
#ifndef __cpp_lib_stacktrace
#include <llvm/Support/Signals.h>
#endif

namespace luthier {

class LuthierError : public llvm::ErrorInfo<LuthierError> {
public:
#ifdef __cpp_lib_stacktrace
  using StackTraceType = std::stacktrace;
#else
  using StackTraceType = std::string;
#endif


#ifdef __cpp_lib_stacktrace
  static auto constexpr StackTraceInitializer = []() {
    return std::stacktrace::current();
  };
#else
  static auto constexpr StackTraceInitializer = []() {
    std::string Out;
    llvm::raw_string_ostream OutStream(Out);
    llvm::sys::PrintStackTrace(OutStream);
    return Out;
  };
#endif

protected:
  /// Source location where the error occurred
  /// 错误发生的源位置
  const std::source_location ErrorLocation;
  /// Stack trace of where the error occurred
  /// 错误发生的栈追踪
  const StackTraceType StackTrace;
  /// Message of the error
  /// 错误消息
  const std::string ErrorMsg;

  explicit LuthierError(std::string ErrorMsg,
                        const std::source_location ErrorLocation =
                            std::source_location::current(),
                        StackTraceType StackTrace = StackTraceInitializer())
      : ErrorLocation(ErrorLocation), StackTrace(std::move(StackTrace)),
        ErrorMsg(std::move(ErrorMsg)) {};

  explicit LuthierError(const llvm::formatv_object_base &ErrorMsg,
                        const std::source_location ErrorLocation =
                            std::source_location::current(),
                        StackTraceType StackTrace = StackTraceInitializer())
      : ErrorLocation(ErrorLocation), StackTrace(std::move(StackTrace)),
        ErrorMsg(ErrorMsg.str()) {};

public:
  static char ID;

  [[nodiscard]] std::source_location getErrorLocation() const {
    return ErrorLocation;
  }

  [[nodiscard]] const StackTraceType &getStackTrace() const {
    return StackTrace;
  }

  [[nodiscard]] llvm::StringRef getErrorMsg() const { return ErrorMsg; }

  [[nodiscard]] std::error_code convertToErrorCode() const override {
    llvm_unreachable("Not implemented");
  }
};

} // namespace luthier

#endif