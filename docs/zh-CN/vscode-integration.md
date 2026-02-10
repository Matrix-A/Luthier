# VSCode 集成配置

以下 JSON 配置可用于 VSCode，以启用代码补全和 IntelliSense：

```json
{
    "configurations": [
        {
            "includePath": [
                "/opt/rocm/include",
                "/opt/rocm/llvm/include",
                "${workspaceRoot}/include"
            ],
            "defines": [
                "AMD_INTERNAL_BUILD",
                "LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1"
            ],
            "cppStandard": "c++20",
            "name": "Linux",
            "compileCommands": "${workspaceRoot}/.vscode/compile_commands.json",
            "configurationProvider": "ms-vscode.cmake-tools"
        }
    ],
    "version": 4
}
```

此配置设置了以下内容：

- **includePath**：包含搜索路径，指向 ROCm 头文件、LLVM 头文件和项目 include 目录
- **defines**：预处理器定义，用于正确的代码解析
- **cppStandard**：C++ 标准设置为 C++20
- **compileCommands**：指向编译命令文件的路径，用于准确的代码补全
- **configurationProvider**：CMake 工具扩展，用于项目配置解析
