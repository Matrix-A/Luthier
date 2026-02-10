# 运行基于 Luthier 的工具

要使用 Luthier 开发的工具对基于 HIP 的应用程序进行插桩和加载，请运行以下命令：

```shell
LD_PRELOAD=${LUTHIER_PATH}:${TOOL_PATH} ${APPLICATION_CMD}
```

- `LD_PRELOAD` 在加载任何其他库之前加载指定的共享对象
- `${LUTHIER_PATH}` 是 Luthier 共享库的路径
- `${TOOL_PATH}` 是使用 Luthier 构建为共享对象的工具的路径
- `${APPLICATION_CMD}` 是用于启动目标应用程序的命令

如果目标应用程序不是基于 HIP 的，则必须设置环境变量 `HIP_ENABLE_DEFERRED_LOADING` 为 0，以确保 HIP 运行时加载插桩设备函数。
