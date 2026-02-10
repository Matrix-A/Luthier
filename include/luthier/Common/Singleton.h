//===-- Singleton.h - Luthier Singleton Interface ---------------*- C++ -*-===//
// Singleton 单例模式接口头文件
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
/// Defines the interface inherited by all Singleton objects in
/// Luthier.
/// It was inspired by OGRE's Singleton implementation here:
/// https://github.com/OGRECave/ogre/blob/master/OgreMain/include/OgreSingleton.h
/// 定义 Luthier 中所有单例对象继承的接口
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_COMMON_SINGLETON_H
#define LUTHIER_COMMON_SINGLETON_H
#include "luthier/Common/ErrorCheck.h"
#include "luthier/Common/GenericLuthierError.h"

namespace luthier {

/// \brief Interface inherited by all Singleton objects in Luthier
/// \tparam T The concrete Singleton object itself
/// Luthier 中所有单例对象继承的接口
template <typename T> class Singleton {
private:
  static T *Instance;

public:
  /// Constructor for explicit initialization of the Singleton instance \n
  /// Instead of hiding initialization away in the \c instance() method,
  /// this design allows passing additional arguments to the constructor
  /// of Singleton if required
  /// The constructor is \b not thread-safe, and is meant to be allocated
  /// on the heap with the \c new operator for better control over its lifetime
  /// 单例实例显式初始化的构造函数
  /// 不在 \c instance() 方法中隐藏初始化，此设计允许根据需要向 Singleton 构造函数传递额外参数
  /// 构造函数不是线程安全的，旨在使用 \c new 运算符在堆上分配，以便更好地控制其生命周期
  Singleton() {
    LUTHIER_REPORT_FATAL_ON_ERROR(LUTHIER_GENERIC_ERROR_CHECK(
        Instance == nullptr, "Called the Singleton constructor twice."));
    Instance = static_cast<T *>(this);
  }

  /// Destructor for explicit initialization of the Singleton Instance \n
  /// The destructor is \b not thread-safe, and is meant to be used directly
  /// with the \p delete operator for better control over its lifetime
  /// 单例实例显式初始化的析构函数
  /// 析构函数不是线程安全的，旨在直接使用 \p delete 运算符，以便更好地控制其生命周期
  virtual ~Singleton() { Instance = nullptr; }

  /// Disallowed copy construction
  /// 禁止复制构造
  Singleton(const Singleton &) = delete;

  /// Disallowed assignment operation
  /// 禁止赋值操作
  Singleton &operator=(const Singleton &) = delete;

  /// \return a reference to the Singleton instance
  /// 返回单例实例的引用
  static T &instance() {
    LUTHIER_REPORT_FATAL_ON_ERROR(LUTHIER_GENERIC_ERROR_CHECK(
        Instance != nullptr, "Singleton is not initialized"));
    return *Instance;
  }

  static bool isInitialized() { return Instance != nullptr; }
};

#ifdef __clang__
// Template definition of the Instance pointer to suppress clang warnings
// regarding translation units
/// Instance 指针的模板定义，用于抑制 clang 关于翻译单元的警告
template <typename T> T *luthier::Singleton<T>::Instance{nullptr};
#endif

} // namespace luthier

#endif