//===-- GenericLuthierError.cpp -------------------------------------------===//
// GenericLuthierError 通用错误类实现文件
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
/// Implements the GenericLuthierError class.
/// 实现 GenericLuthierError 类
//===----------------------------------------------------------------------===//
#include "luthier/Common/GenericLuthierError.h"

namespace luthier {

char GenericLuthierError::ID = 0;

void GenericLuthierError::log(llvm::raw_ostream &OS) const {
  OS << "Error encountered in file " << ErrorLocation.file_name()
     << ", function " << ErrorLocation.function_name() << ", at "
     << ErrorLocation.line() << ": " << ErrorMsg << ".\n";
  OS << "Stack trace: \n";
#ifdef __cpp_lib_stacktrace
  OS << std::to_string(StackTrace);
#else
  OS << StackTrace;
#endif
  OS << "\n";
}

} // namespace luthier