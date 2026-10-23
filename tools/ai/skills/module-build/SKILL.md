# Skill：模块构建（module-build）

> 职责：新增或修改一个软件模块，保证「能编译、接口有文档、注释合规」。
> 语言规则：**代码注释（含 Doxygen）一律英文**；本文档为中文说明。

## 1. 触发

- 新增模块/组件（`src/`、`plugins/`、`apps/`、`api/`）；
- 修改公共接口（头文件、C ABI、proto、schema）。

## 2. 输入

| 项 | 示例 |
|---|---|
| 模块名与语言 | `plugin_host` / C++20 |
| 目标目录 | `src/core/src/`、`api/include/srsp/` |
| 接口契约 | 能力 ID（`srsp.dynamics/solver@1`）或 C 函数签名 |
| 构建接入点 | 父级 `CMakeLists.txt` |

## 3. 步骤

1. **目录与构建接入**：新建目录 → 父 `CMakeLists.txt` 加 `add_subdirectory` → 目标命名 `srsp_<name>`；
2. **写声明**（头文件/接口），同步写 Doxygen；
3. **写实现**，同步写 Doxygen（不留 TODO 注释给后人）；
4. **构建**：
   ```powershell
   cmake --preset win-ucrt64-debug
   cmake --build --preset win-ucrt64-debug
   ```
5. **静态检查**：编译告警不新增；`clang-tidy`（可用时）无 error；
6. **注释合规检查**：源码不含 CJK（见 §5）；
7. **产出证据**：构建命令、退出码、检查结果。

## 4. Doxygen 规范（强制，英文）

| 对象 | 要求 |
|---|---|
| 文件 | `@file` + `@brief` |
| 结构体/类 | `@brief`；每个成员 `///<` 说明 |
| 函数 | `@brief`、`@param[in/out]`、`@return` / `@retval`、`@note` / `@warning` |
| 枚举 | `@brief`；每个枚举值 `///<` |
| 宏 | `@brief`；必要时 `@warning` |

示例：

```cpp
/** @file plugin_host.cpp @brief Plugin loading, lifecycle and unloading. */

/**
 * @brief Loads a plugin shared library and resolves its entry point.
 * @param[in]  path  Absolute path to the library file.
 * @param[out] out   Receives the plugin vtable on success.
 * @return SRSP_OK on success, SRSP_E_NOT_FOUND if the file is missing,
 *         SRSP_E_VERSION_MISMATCH if the plugin rejects the host ABI.
 * @note The caller owns nothing here; the table lives until process exit.
 */
srsp_status load_plugin(const char* path, const srsp_plugin_v1** out);
```

## 5. 验收命令

```powershell
# 1) build (pick the preset for the toolchain under change)
cmake --build --preset win-ucrt64-debug

# 2) documentation coverage (doxygen config must set WARN_IF_UNDOCUMENTED=YES)
doxygen Doxyfile 2>&1 | Select-String "warning"

# 3) comments must be English: no CJK in source files
Get-ChildItem api,src,plugins,apps,tests -Recurse -Include *.h,*.hpp,*.cpp,*.c |
  Select-String -Pattern '[\u4e00-\u9fff]'
```

三条都必须干净（第 3 条无输出）。

## 6. 常见错误

| 现象 | 原因 | 处理 |
|---|---|---|
| `CMake Error: SRSP_AI_MODEL is not set` | 未注入环境 | `. .\tools\ai\Enter-AgentEnv.ps1 -Model <slug>` |
| 注释里出现中文 | 习惯性写中文 | 改为英文；中文只写在 Markdown 文档 |
| 公共函数缺 `@param` | 只写了 `@brief` | 补齐参数与返回值 |
| 新增告警 | 未加 `-Wall -Wextra` 检查 | 清理后再提交 |

## 7. 产物

- 源码 + 构建接入；
- 构建/检查证据（贴进日志「验证」字段）；
- 需要时同步 `docs/`（契约变更必须更新对应章节）。
