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

## 4. Doxygen 规范（强制，英文 + 中文对照）

### 4.0 适用范围（2026-11-01 明确）

仅以下代码需要双语注释，其余目录保持英文单语：

| 需要双语 | 不需要 |
|---|---|
| `src/core`、`src/model`、`src/dynamics`、`src/render`、`src/record`、`src/bridge` | `apps/`、`bench/`、`tests/`、`tools/` |
| `api/include`（C ABI 表面） | `deepseek/ModuleTest/`（测试） |
| `plugins/`、`examples/`（示例插件） | 脚本、生成物 |
| 上述目录下的公共头文件 | 上述目录下的 `.cpp` 私有实现（英文即可） |


自 2026-11-01 起，公共符号的文档注释采用**英文主注释 + 中文对照**：

| 对象 | 要求 |
|---|---|
| 文件 | `@file` + `@brief`，下接 `@brief_zh` |
| 结构体/类 | `@brief` / `@brief_zh`；每个成员 `///< English / 中文` |
| 函数 | `@brief`、`@param[in/out]`、`@return` / `@retval`，随后以 `@zh` 段落给出中文对照 |
| 枚举 | `@brief` / `@brief_zh`；每个枚举值 `///< English / 中文` |
| 宏 | `@brief` / `@brief_zh`；必要时 `@warning` / `@warning_zh` |

### 4.1 格式定义

```cpp
/**
 * @file mcap.cpp
 * @brief Minimal MCAP writer and reader for the recording subset.
 * @brief_zh 录制子集的最小 MCAP 读写实现。
 */

/**
 * @brief Writes a binary attachment used for BlobRef payloads.
 * @param[in] name Attachment name, conventionally "blob/<id>".
 * @param[in] log_time_ns Timestamp on the same clock as the messages.
 * @return true on success, false when the writer is closed or the write fails.
 * @zh 写出用于 BlobRef 载荷的二进制附件。
 * @zh_param[in] name 附件名，约定为 "blob/<id>"。
 * @zh_param[in] log_time_ns 与消息同一时钟的时间戳。
 * @zh 返回：成功为 true；写入器已关闭或写失败为 false。
 */
bool add_attachment(const std::string& name, std::uint64_t log_time_ns);
```

### 4.2 规则

1. **英文在前、中文在后**，一一对应；中文不得替代英文；
2. 中文行前缀统一为 `@brief_zh`、`@zh`、`@zh_param`、`@warning_zh`；
3. 行内成员注释：`///< English text / 中文文本`；
4. 英文段与中文段都遵守「解释为什么，不复述代码」；
5. 私有实现可只写英文；**公共头文件必须双语**；
6. 中文标点用全角，英文用半角，不在同一行混排（成员行注释除外）；
7. Doxygen 若启用，需在 `Doxyfile` 中加别名，避免未知命令告警：
   ```text
   ALIASES = "brief_zh=@brief" "zh=@details" "zh_param=@param" "warning_zh=@warning"
   ```

### 4.3 变量命名与注释

| 变量类型 | 注释要求 |
|---|---|
| 结构体成员 | 行尾 `///<` 双语，含单位（如 `m`, `rad/s`, `TAI ns`） |
| 函数参数 | `@param[in/out]` 英文 + `@zh_param` 中文，含取值范围与单位 |
| 局部变量 | 仅当含义不直观时注释，单行英文即可 |
| 常量 | `///<` 说明取值理由（如预算、上限） |


## 5. 验收命令

```powershell
# 1) build (pick the preset for the toolchain under change)
cmake --build --preset win-ucrt64-debug

# 2) documentation coverage (doxygen config must set WARN_IF_UNDOCUMENTED=YES)
doxygen Doxyfile 2>&1 | Select-String "warning"

# 3) Chinese must appear only in the paired zh lines (brief_zh / zh / zh_param)
Get-ChildItem api,src,plugins,apps,tests -Recurse -Include *.h,*.hpp,*.cpp,*.c |
  Select-String -Pattern '[\u4e00-\u9fff]' |
  Where-Object { $_.Line -notmatch '_zh|///<.*/' }
```

三条都必须干净（第 3 条无输出）。

## 6. 常见错误

| 现象 | 原因 | 处理 |
|---|---|---|
| `CMake Error: SRSP_AI_MODEL is not set` | 未注入环境 | `. .\tools\ai\Enter-AgentEnv.ps1 -Model <slug>` |
| 中文写在纯英文行 | 习惯性写中文 | 中文只放在 `_zh` 行或 `///< ... /` 后半段 |
| 批量替换整批未生效 | 注释含英文撇号（`Qt's`、`child's`）破坏 PowerShell 单引号哈希表 | 含撇号的行用双引号键；每批 ≤ 4 文件并立即编译 |
| 公共头只有英文 | 漏写中文对照 | 补 `@brief_zh` / `@zh` / `@zh_param` |
| 公共函数缺 `@param` | 只写了 `@brief` | 补齐参数与返回值（中英各一份） |
| 新增告警 | 未加 `-Wall -Wextra` 检查 | 清理后再提交 |

## 7. 产物

- 源码 + 构建接入；
- 构建/检查证据（贴进日志「验证」字段）；
- 需要时同步 `docs/`（契约变更必须更新对应章节）。
