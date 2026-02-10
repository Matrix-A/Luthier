//===-- InstrumentationTask.h - Instrumentation Task ------------*- C++ -*-===//
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
/// \file
/// \brief 本文件描述了插桩任务，这是一个工具接口，用于描述应该如何插桩提升的表示。
/// This file describes the instrumentation task, an interface for tools to
/// describe how a Lifted Representation should be instrumented.
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_TOOLING_INSTRUMENTATION_TASK_H
#define LUTHIER_TOOLING_INSTRUMENTATION_TASK_H
#include "luthier/types.h"
#include <functional>
#include <llvm/ADT/DenseMap.h>
#include <llvm/CodeGen/MachineInstr.h>
#include <utility>
#include <variant>

namespace luthier {

class LiftedRepresentation;

class InstrumentationModule;

/// \brief 跟踪要对 \c LiftedRepresentation执行的修改，以创建 HSA 执行原语（即 <tt>hsa_executable_symbol_t</tt> 或 <tt>hsa_executable_t</tt>）的插桩版本
/// \details 插桩任务由以下部分组成：\n
/// 1. 一个"预设"名称，用于标识插桩任务。同一个 HSA 原语不能使用相同的名称进行插桩。同一 HSA 原语的不同插桩版本必须具有不同的预设名称。\n
/// 2. 一个函数，允许用户描述如何对 <tt>LiftedRepresentation</tt> 进行插桩。此函数应用于 <tt>LiftedRepresentation</tt> 的克隆版本，它是唯一允许直接改变 <tt>LiftedRepresentation</tt> 的面向用户的地方（例如，使用机器指令构建器 API 添加 <tt>llvm::MachineInstr</tt>）。在此函数之外，修改 \c InstrumentationTask 的唯一方法是通过 \c InstrumentationTask::insertHookBefore 函数插入钩子。\n
/// 此类对象以及 HSA 原语的 <tt>LiftedRepresentation<tt> 被传递给 \c luthier::instrumentAndLoad 函数。
/// \brief keeps track of modifications to be performed on on a
/// \c LiftedRepresentation in order to create an instrumented version of an HSA
/// execution primitive (i.e. <tt>hsa_executable_symbol_t</tt>, or
/// <tt>hsa_executable_t</tt>)
/// \details instrumentation task consists of the following:\n
/// 1. A "preset" name, identifying the instrumentation task. An HSA primitive
/// cannot be instrumented under the same name. Different instrumented versions
/// of the same HSA primitive must have different preset names.\n
/// 2. A function allowing the user to describe how to instrument a
/// <tt>LiftedRepresentation</tt>. This function is applied to a cloned version
/// of a <tt>LiftedRepresentation</tt>, and it is designed to be the only
/// user-facing place which allows directly mutating the
/// <tt>LiftedRepresentation</tt> (e.g. adding <tt>llvm::MachineInstr</tt>'s
/// using the Machine Instruction builder API). Outside of this function, the
/// only way to modify the \c InstrumentationTask is to insert hooks via the
/// \c InstrumentationTask::insertHookBefore function.\n
/// Objects of this class as well as a <tt>LiftedRepresentation<tt> of an
/// HSA primitive are passed to the \c luthier::instrumentAndLoad function.
class InstrumentationTask {
public:
  typedef struct {
    /// 要插入的钩子名称
    /// Name of the hook to be inserted
    llvm::StringRef HookName;
    /// 传递给钩子的参数列表
    /// List of arguments passed to the hook
    llvm::SmallVector<std::variant<llvm::Constant *, llvm::MCRegister>, 1> Args;
  } hook_invocation_descriptor;

  /// \c llvm::MachineInstr 到要插入的钩子及其参数的映射
  /// A mapping of a \c llvm::MachineInstr to the hooks + their arguments
  /// to be inserted before it
  typedef llvm::DenseMap<llvm::MachineInstr *,
                         llvm::SmallVector<hook_invocation_descriptor, 1>>
      hook_insertion_tasks;

private:
  /// 被插桩的 \c LiftedRepresentation
  /// The \c LiftedRepresentation being instrumented
  LiftedRepresentation &LR;
  /// 用于对 <tt>LiftedRepresentation</tt> 进行插桩的插桩模块
  /// The instrumentation module used to instrument the
  /// <tt>LiftedRepresentation</tt>s
  const InstrumentationModule &IM;
  /// 要在 <tt>LiftedRepresentation</tt> 的每个 \c llvm::MachineInstr 处插入的钩子列表
  /// A list of hooks to be inserted at each \c llvm::MachineInstr of the
  /// <tt>LiftedRepresentation</tt>
  hook_insertion_tasks HookInsertionTasks{};

public:
  /// InstrumentationTask 构造函数
  /// InstrumentationTask constructor
  /// \param LR 被插桩的 \c LiftedRepresentation
  /// \param LR the \c LiftedRepresentation being instrumented
  explicit InstrumentationTask(LiftedRepresentation & LR);

  /// 排队一个钩子插入任务，该任务将在 \p MI 之前插入一个钩子\n
  /// 没有 "<tt>insertHookAfter</tt>" 变体，以防止在块的终止符指令之后插入指令
  /// \param MI 钩子将插入其前的 \c llvm::MachineInstr
  /// \param Hook 从 \c LUTHIER_GET_HOOK_HANDLE 获取的钩子句柄
  /// \param Args 要传递给钩子的参数列表；默认为空列表
  /// \return 指示操作成功或其失败的 \c llvm::Error
  /// Queues a hook insertion task, which will insert a hook before the
  /// \p MI \n
  /// There is no "<tt>insertHookAfter</tt>" variant to prevent insertion of
  /// instructions after the block's terminator instruction
  /// \param MI the \c llvm::MachineInstr the hook will be inserted before
  /// \param Hook handle of the hook obtained from \c LUTHIER_GET_HOOK_HANDLE
  /// \param Args A list of arguments to be passed to the hook; An empty list
  /// by default
  /// \returns an \c llvm::Error indicating the success of the operation or
  /// its failure
  llvm::Error insertHookBefore(
      llvm::MachineInstr &MI, const void *Hook,
      llvm::ArrayRef<std::variant<llvm::Constant *, llvm::MCRegister>> Args =
          {});

  /// \return 钩子插入任务的常量引用
  /// \return a const reference to the hook insertion tasks
  [[nodiscard]] const hook_insertion_tasks &getHookInsertionTasks() const {
    return HookInsertionTasks;
  }

  /// \return 此任务的插桩模块的常量引用
  /// \return a const reference to the instrumentation module of this task
  [[nodiscard]] const InstrumentationModule &getModule() const { return IM; }
};

} // namespace luthier

#endif
