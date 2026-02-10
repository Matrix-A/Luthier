//===-- LoadedCodeObjectSymbol.h - Loaded Code Object Symbol ----*- C++ -*-===//
// Copyright 2022-2025 @ Northeastern University Computer Architecture Lab
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===----------------------------------------------------------------------===//
//
/// \file
/// \brief 本文件定义了 \c hsa::LoadedCodeObjectSymbol 类。
/// 它表示 \c hsa_loaded_code_object_t 内部的所有感兴趣的符号，无论其绑定类型如何，与仅包含 \c STB_GLOBAL 绑定的符号的 <tt>hsa_executable_symbol_t</tt> 不同。
/// This file defines the \c hsa::LoadedCodeObjectSymbol class.
/// It represents all symbols of interest inside an \c hsa_loaded_code_object_t
/// regardless of their binding type, unlike <tt>hsa_executable_symbol_t</tt>
/// which only include symbols with a \c STB_GLOBAL binding.
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_HSA_LOADED_CODE_OBJECT_SYMBOL_H
#define LUTHIER_HSA_LOADED_CODE_OBJECT_SYMBOL_H
#include "luthier/HSA/ApiTable.h"
#include "luthier/HSA/LoadedCodeObject.h"
#include "luthier/Object/AMDGCNObjectFile.h"
#include <hsa/hsa.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/Object/ELFObjectFile.h>
#include <luthier/types.h>
#include <optional>
#include <string>

namespace luthier::hsa {

/// \brief 表示 \c hsa_loaded_code_object_t 的 ELF 内部的符号
/// \details 与 <tt>hsa_executable_symbol_t</tt>（其中只有全局面向的符号由支持的 <tt>hsa_executable_t</tt> 枚举）不同，此类封装的对象同时具有 <tt>STB_GLOBAL</tt> 和 <tt>STB_LOCAL</tt> 绑定。这允许表示感兴趣的符号，包括具有本地绑定的设备函数和变量（例如主机调用打印操作中使用的字符串）。
/// \brief Represents a symbol inside the ELF of an \c hsa_loaded_code_object_t
/// \details Unlike <tt>hsa_executable_symbol_t</tt> where only global facing
/// symbols are enumerated by the backing <tt>hsa_executable_t</tt>, objects
/// encapsulated by this class have both <tt>STB_GLOBAL</tt> and
/// <tt>STB_LOCAL</tt> bindings. This allows for representation of symbols of
/// interest, including device functions and variables with local bindings (e.g.
/// strings used in host call print operations).
class LoadedCodeObjectSymbol {
public:
  /// 符号类型枚举
  /// 符号类型：内核、设备函数、变量、外部符号
  enum SymbolKind { SK_KERNEL, SK_DEVICE_FUNCTION, SK_VARIABLE, SK_EXTERNAL };

protected:
  /// 此符号所属的 HSA 加载代码对象
  /// The HSA Loaded Code Object this symbol belongs to
  hsa_loaded_code_object_t BackingLCO{};
  /// 已解析的 LCO 存储 ELF，以确保 \c Symbol 保持有效
  /// Parsed storage ELF of the LCO, to ensure \c Symbol stays valid
  luthier::object::AMDGCNObjectFile &StorageELF;
  /// 此 LCO 符号的 LLVM 对象 ELF 符号；
  /// 通过解析 LCO 的存储 ELF 支持
  /// The LLVM Object ELF symbol of this LCO symbol;
  /// Backed by parsing the storage ELF of the LCO
  llvm::object::ELFSymbolRef Symbol;
  /// LLVM RTTI
  /// LLVM RTTI
  SymbolKind Kind;
  /// HSA 可执行文件符号的等效项（如果存在）
  /// The HSA executable symbol equivalent, if exists
  std::optional<hsa_executable_symbol_t> ExecutableSymbol;

  /// 子类使用的构造函数
  /// \param LCO 符号所属的 \c hsa_loaded_code_object_t
  /// \param StorageELF \p Symbol 的 \c luthier::AMDGCNObjectFile
  /// \param Symbol 从 \p LCO 的存储 ELF 解析并缓存的 \c llvm::object::ELFSymbolRef 引用
  /// \param Kind 被构造符号的类型
  /// \param ExecutableSymbol <tt>LoadedCodeObjectSymbol</tt> 的 \c hsa_executable_symbol_t 等效项（如果存在）
  /// Constructor used by subclasses
  /// \param LCO the \c hsa_loaded_code_object_t which the symbol belongs to
  /// \param StorageELF the \c luthier::AMDGCNObjectFile of \p Symbol
  /// \param Symbol a reference to the \c llvm::object::ELFSymbolRef
  /// that was obtained from parsing the storage ELF of the \p LCO and cached
  /// \param Kind the type of the symbol being constructed
  /// \param ExecutableSymbol the \c hsa_executable_symbol_t equivalent of
  /// the <tt>LoadedCodeObjectSymbol</tt> if exists
  LoadedCodeObjectSymbol(
      hsa_loaded_code_object_t LCO,
      luthier::object::AMDGCNObjectFile &StorageELF,
      llvm::object::ELFSymbolRef Symbol, SymbolKind Kind,
      std::optional<hsa_executable_symbol_t> ExecutableSymbol);

public:
  /// 禁止复制构造
  /// Disallowed copy construction
  LoadedCodeObjectSymbol(const LoadedCodeObjectSymbol &) = delete;

  /// 禁止赋值操作
  /// Disallowed assignment operation
  LoadedCodeObjectSymbol &operator=(const LoadedCodeObjectSymbol &) = delete;

  virtual ~LoadedCodeObjectSymbol() = default;

  /// \return 深拷贝副本
  /// \return a deep clone copy of the
  [[nodiscard]] virtual std::unique_ptr<LoadedCodeObjectSymbol> clone() const {
    return std::unique_ptr<LoadedCodeObjectSymbol>(new LoadedCodeObjectSymbol(
        this->BackingLCO, this->StorageELF, this->Symbol, this->Kind,
        this->ExecutableSymbol));
  }

  /// 相等运算符
  /// Equality operator
  bool operator==(const LoadedCodeObjectSymbol &Other) const {
    bool Out = Symbol == Other.Symbol &&
               BackingLCO.handle == Other.BackingLCO.handle &&
               Kind == Other.Kind;
    if (ExecutableSymbol.has_value()) {
      return Out &&
             (Other.ExecutableSymbol.has_value() &&
              ExecutableSymbol->handle == Other.ExecutableSymbol->handle);
    } else
      return Out && !Other.ExecutableSymbol.has_value();
  }

  /// 工厂方法，根据 \c hsa_executable_symbol_t 返回 \c LoadedCodeObjectSymbol
  /// \param Symbol 被查询的 \c hsa_executable_symbol_t
  /// \return 成功时返回 HSA 可执行文件符号的缓存 \c LoadedCodeObjectSymbol 的常量引用，失败时返回 \c llvm::Error
  /// Factory method which returns the \c LoadedCodeObjectSymbol given its
  /// \c hsa_executable_symbol_t
  /// \param Symbol the \c hsa_executable_symbol_t being queried
  /// \return on success, a const reference to a cached
  /// \c LoadedCodeObjectSymbol of the HSA executable symbol, or an
  /// \c llvm::Error on failure
  static llvm::Expected<std::unique_ptr<hsa::LoadedCodeObjectSymbol>>
  fromExecutableSymbol(const ApiTableContainer<::CoreApiTable> &CoreApi,
                       const hsa_ven_amd_loader_1_03_pfn_t &LoaderApi,
                       hsa_executable_symbol_t Symbol);

  /// 查询 \c hsa::LoadedCodeObjectSymbol 是否在 \p LoadedAddress 的设备内存中加载
  /// \param LoadedAddress 被查询的设备加载地址
  /// \return 如果给定地址没有加载符号则返回 \c nullptr，否则返回加载在给定地址的符号的 \c const 指针
  /// Queries if a \c hsa::LoadedCodeObjectSymbol is
  /// loaded on device memory at \p LoadedAddress
  /// \param LoadedAddress the device loaded address being queried
  /// \return \c nullptr if no symbol is loaded at the given address, or
  /// a \c const pointer to the symbol loaded at the given address
  static llvm::Expected<std::unique_ptr<hsa::LoadedCodeObjectSymbol>>
  fromLoadedAddress(const ApiTableContainer<::CoreApiTable> &CoreApi,
                    const hsa_ven_amd_loader_1_03_pfn_t &LoaderApi,
                    luthier::address_t LoadedAddress);

  /// \return 此符号的 \c SymbolKind
  /// \return the \c SymbolKind of this symbol
  [[nodiscard]] SymbolKind getType() const { return Kind; }

  /// \return 此符号的 GPU Agent，失败返回 \c llvm::Error
  /// \return GPU Agent of this symbol on success, an \c llvm::Error
  /// on failure
  template <typename LoaderTableType = hsa_ven_amd_loader_1_01_pfn_t>
  [[nodiscard]] llvm::Expected<hsa_agent_t>
  getAgent(const LoaderTableType &VenLoaderTable) const {
    return hsa::loadedCodeObjectGetAgent(VenLoaderTable, BackingLCO);
  }

  /// \return 此符号的加载代码对象
  /// \return Loaded Code Object of this symbol
  [[nodiscard]] hsa_loaded_code_object_t getLoadedCodeObject() const {
    return BackingLCO;
  }

  /// \return 符号加载到的可执行文件
  /// \return the executable this symbol was loaded into
  template <typename LoaderTableType = hsa_ven_amd_loader_1_01_pfn_t>
  [[nodiscard]] llvm::Expected<hsa_executable_t>
  getExecutable(const LoaderTableType &VenLoaderTable) const {
    return hsa::loadedCodeObjectGetExecutable(VenLoaderTable, BackingLCO);
  }

  /// \return 符号的名称，失败返回 \c llvm::Error
  /// \return the name of the symbol on success, or an \c llvm::Error on
  /// failure
  [[nodiscard]] llvm::Expected<llvm::StringRef> getName() const;

  /// \return 符号的大小
  /// \return the size of the symbol
  [[nodiscard]] size_t getSize() const;

  /// \return 符号的绑定
  /// \return the binding of the symbol
  [[nodiscard]] uint8_t getBinding() const;

  /// \return \c llvm::ArrayRef<uint8_t>，封装此符号在加载到的 \c GpuAgent 上的内容
  /// \return an \c llvm::ArrayRef<uint8_t> encapsulating the contents of
  /// this symbol on the \c GpuAgent it was loaded onto
  [[nodiscard]] llvm::Expected<llvm::ArrayRef<uint8_t>> getLoadedSymbolContents(
      const hsa_ven_amd_loader_1_03_pfn_t &VenLoaderTable) const;

  [[nodiscard]] llvm::Expected<luthier::address_t> getLoadedSymbolAddress(
      const hsa_ven_amd_loader_1_03_pfn_t &VenLoaderTable) const;

  /// \return 与此 LCO 符号关联的 \c hsa_executable_symbol_t（如果存在，即符号具有 \c llvm::ELF::STB_GLOBAL 绑定），否则返回 \c std::nullopt
  /// \return the \c hsa_executable_symbol_t associated with
  /// this LCO Symbol if exists (i.e the symbol has a \c llvm::ELF::STB_GLOBAL
  /// binding), or an \c std::nullopt otherwise
  [[nodiscard]] std::optional<hsa_executable_symbol_t>
  getExecutableSymbol() const;

  /// 以人类可读的形式打印符号。
  /// Print the symbol in human-readable form.
  void print(llvm::raw_ostream &OS) const;

  void dump() const;

  [[nodiscard]] inline size_t hash() const {
    llvm::object::DataRefImpl Raw = Symbol.getRawDataRefImpl();
    return llvm::hash_combine(
        BackingLCO.handle, Raw.p, Raw.d.a, Raw.d.b, Kind,
        ExecutableSymbol.has_value() ? ExecutableSymbol->handle : 0);
  }
};

/// 用于在 STL 容器中方便查找符号的相等性结构体
/// Equal-to struct used to allow convenient look-ups of symbols inside
/// STL containers
template <
    typename SymbolType,
    std::enable_if_t<std::is_base_of_v<LoadedCodeObjectSymbol, SymbolType>,
                     bool> = true>
struct LoadedCodeObjectSymbolEqualTo {
  using is_transparent = void;

  template <typename Dt, typename Dt2>
  bool operator()(const std::unique_ptr<SymbolType, Dt> &Lhs,
                  const std::unique_ptr<SymbolType, Dt2> &Rhs) const {
    if (Lhs && Rhs)
      return *Lhs == *Rhs;
    else
      return Lhs.get() == Rhs.get();
  }

  template <typename Dt>
  bool operator()(const std::unique_ptr<SymbolType, Dt> &Lhs,
                  const std::shared_ptr<SymbolType> &Rhs) const {
    if (Lhs && Rhs)
      return *Lhs == *Rhs;
    else
      return Lhs.get() == Rhs.get();
  }

  template <typename Dt>
  bool operator()(const std::unique_ptr<SymbolType, Dt> &Lhs,
                  const SymbolType *Rhs) const {
    if (Lhs && Rhs)
      return *Lhs == *Rhs;
    else
      return Lhs.get() == Rhs;
  }

  template <typename Dt>
  bool operator()(const std::unique_ptr<SymbolType, Dt> &Lhs,
                  const SymbolType &Rhs) const {
    return Lhs && *Lhs == Rhs;
  }

  bool operator()(const std::shared_ptr<SymbolType> &Lhs,
                  const std::shared_ptr<SymbolType> &Rhs) const {
    if (Lhs && Rhs)
      return *Lhs == *Rhs;
    else
      return Lhs.get() == Rhs.get();
  }

  template <typename Dt>
  bool operator()(const std::shared_ptr<SymbolType> &Lhs,
                  const std::unique_ptr<SymbolType, Dt> &Rhs) const {
    if (Lhs && Rhs)
      return *Lhs == *Rhs;
    else
      return Lhs.get() == Rhs.get();
  }

  bool operator()(const std::shared_ptr<SymbolType> &Lhs,
                  const SymbolType *Rhs) const {
    if (Lhs && Rhs)
      return *Lhs == *Rhs;
    else
      return Lhs.get() == Rhs;
  }

  bool operator()(const std::shared_ptr<SymbolType> &Lhs,
                  const SymbolType &Rhs) const {
    return Lhs && *Lhs == Rhs;
  }

  bool operator()(const SymbolType *Lhs, const SymbolType *Rhs) const {
    if (Lhs && Rhs)
      return *Lhs == *Rhs;
    else
      return Lhs == Rhs;
  }

  bool operator()(const SymbolType *Lhs,
                  const std::unique_ptr<SymbolType> &Rhs) const {
    if (Lhs && Rhs)
      return *Lhs == *Rhs;
    else
      return Lhs == Rhs.get();
  }

  bool operator()(const SymbolType *Lhs,
                  const std::shared_ptr<SymbolType> &Rhs) const {
    if (Lhs && Rhs)
      return *Lhs == *Rhs;
    else
      return Lhs == Rhs.get();
  }

  bool operator()(const SymbolType *Lhs, const SymbolType &Rhs) const {
    return Lhs && *Lhs == Rhs;
  }

  bool operator()(const SymbolType &Lhs, const SymbolType &Rhs) const {
    return Lhs == Rhs;
  }

  template <typename Dt>
  bool operator()(const SymbolType &Lhs,
                  const std::unique_ptr<SymbolType, Dt> &Rhs) const {
    return Rhs && Lhs == *Rhs;
  }

  bool operator()(const SymbolType &Lhs,
                  const std::shared_ptr<SymbolType> &Rhs) const {
    return Rhs && Lhs == *Rhs;
  }

  bool operator()(const SymbolType &Lhs, const SymbolType *Rhs) const {
    return Rhs && Lhs == *Rhs;
  }
};

/// \brief 用于在 STL 容器中方便查找符号的哈希结构体
/// \brief Hash struct to allow convenient look-up of symbols inside STL
/// containers
template <
    typename SymbolType,
    std::enable_if_t<std::is_base_of_v<LoadedCodeObjectSymbol, SymbolType>,
                     bool> = true>
struct LoadedCodeObjectSymbolHash {
  using is_transparent = void;

  using transparent_key_equal = LoadedCodeObjectSymbolEqualTo<SymbolType>;

  std::size_t operator()(const std::unique_ptr<SymbolType> &Symbol) const {
    if (Symbol)
      return Symbol->hash();
    else
      return llvm::hash_value(static_cast<SymbolType *>(nullptr));
  }

  std::size_t
  operator()(const std::unique_ptr<const SymbolType> &Symbol) const {
    if (Symbol)
      return Symbol->hash();
    else
      return llvm::hash_value(static_cast<const SymbolType *>(nullptr));
  }

  std::size_t operator()(const std::shared_ptr<SymbolType> &Symbol) const {
    if (Symbol)
      return Symbol->hash();
    else
      return llvm::hash_value(static_cast<SymbolType *>(nullptr));
  }

  std::size_t
  operator()(const std::shared_ptr<const SymbolType> &Symbol) const {
    if (Symbol)
      return Symbol->hash();
    else
      return llvm::hash_value(static_cast<const SymbolType *>(nullptr));
  }

  std::size_t operator()(const SymbolType *Symbol) const {
    if (Symbol)
      return Symbol->hash();
    else
      return llvm::hash_value(static_cast<SymbolType *>(nullptr));
  }

  std::size_t operator()(const SymbolType &Symbol) const {
    return Symbol.hash();
  }
};

} // namespace luthier::hsa

#endif
