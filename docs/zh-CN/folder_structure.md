# Luthier 的高级文件夹结构

Luthier 项目由以下文件夹组成：

- **`.devcontainer/`**：包含 `devcontainer.json` 文件。它可以与 CLion 或 VSCode 等 IDE 一起使用，以在容器内运行和开发 Luthier。默认情况下，它使用我们提供的开发容器，但您可以在 `dockerfiles` 文件夹中自行构建容器。

- **`.vscode`**：包含编译命令，可与 VSCode 一起启用 Intellisense 代码补全。

- **`compiler-plugins`**：包含 Luthier 的 LLVM 编译器插件。目前由 Luthier 工具使用；将来，它将包含用于静态插桩的编译器插件。

- **`dockerfiles`**：包含 Luthier 项目的 Docker 文件，用于运行和开发基于 Luthier 的工具。

- **`docs`**：包含 Luthier 的文档。

- **`examples`**：包含示例 Luthier 工具。

- **`include`**：包含 Luthier 的面向公众的 API。

- **`scripts`**：包含用于生成 Luthier 的 HIP 和 HSA 回调的脚本。

- **`src`**：包含 Luthier 的源代码。

- **`tests`**：包含集成测试。

---

（以下内容已被注释掉，记录了 Luthier 的旧架构设计，供参考）

Luthier 由以下组件组成：

1. **控制器**：在 `luthier` 命名空间下，位于 `src/luthier.cpp`。它为工具编写者实现高级接口，并根据需要初始化和最终确定所有基本组件。

2. **HSA 拦截层**：在 `src/hsa_intercept.hpp` 和 `src/hsa_intercept.cpp` 中实现。它拦截目标应用程序调用的所有 HSA 函数，并且（如果启用）向用户和工具执行回调。

3. **HIP 拦截层**：在 `src/hip_intercept.hpp` 和 `src/hip_intercept.cpp` 中实现。它拦截 HIP 运行时调用的所有 HIP 函数，并且（如果启用）向用户和工具执行回调。

4. **目标管理器**：在 `src/target_manager.hpp` 和 `src/target_manager.cpp` 中实现。它记录附加到系统的代理的 HSA ISA，并为运行时每个唯一的 ISA 创建 LLVM 相关的数据结构并缓存它们。任何其他组件都可以查询此信息。

5. **HSA 抽象层**：在 `luthier::hsa` 命名空间下实现。为 HSA 库的 C-API 提供有用、面向对象的抽象，为 Luthier 提供不那么冗长的接口，并实现 HSA 当前未实现的任何所需功能（如间接函数支持）。此层调用的 API 不会被拦截。其他组件不应直接使用 HSA 库。

6. **代码提升器**：在 `src/disassembler.hpp` 和 `src/disassembler.cpp` 中实现。它使用 LLVM 反汇编每个 `hsa::ExecutableSymbol` 并缓存结果。它是唯一允许创建 `hsa::Instr` 对象的组件。

7. **代码生成器**：在 `src/code_generator.hpp` 和 `src/code_generator.cpp` 中实现。在用户描述插桩任务后，代码生成器通过分析目标代码对象的 LLVM 反汇编指令来创建插桩后的代码对象。

其他说明：

- HIP/HSA API 表通过 Python 脚本 `hip_intercept_gen.py` 和 `hsa_intercept_gen.py` 自动生成。

- 目前，HIP 使用 `dlsym` 来捕获必要的 HIP API。我们正在迁移到 Gotcha，以提供动态开启/关闭不必要的 API 捕获的方法。HIP API 表承诺在 ROCm 6.0+ 中提供。

- HSA 使用 `libroctool.so` 来捕获 HSA API 表。动态开启/关闭捕获目前正在研究中。
