# 05 MSYS2 开发环境与构建体系

> 目标：在 Windows 上用 **MSYS2 UCRT64** 获得与 Linux 一致的原生工具链体验；VSCode 作为统一编辑器；一条命令完成配置、构建、测试、打包。
> 版本：0.1 ｜ 状态：评审草案 ｜ 适用：SRSP 1.x

---

## 1. 环境策略

| 平台 | 工具链 | 说明 |
|---|---|---|
| Windows 10/11 x64 | **MSYS2 UCRT64**（GCC 13–16 / Clang 17+） | 产物为原生 Win64，仅依赖 UCRT 与 MinGW 运行库；**不使用** MSYS 运行时构建发布物 |
| Linux（Ubuntu 22.04/24.04） | 系统 GCC/Clang + CMake/Ninja | 与 Windows 共享 CMakePresets 与源码 |
| Python 边车 | 独立 venv（3.11+） | 不内嵌解释器；推荐独立 CPython（python.org / 系统 Python），MSYS2 Python 仅用于工具脚本 |
| 文档/工具 | MSYS2 内的 mkdocs、doxygen、graphviz | 也可在 Linux 执行 |

**禁止事项**：

- 不把 `C:\msys64\usr\bin` 加入系统 PATH（会遮蔽 Windows 的 `find`、`sort`、`link` 等，导致构建脚本行为异常）。
- 不混用 MSVC 编译的库与 MinGW 编译的目标（C++ ABI 不兼容，表现为链接错误或运行时崩溃）。
- 不在 `msys` 子环境中构建发布物（会引入 `msys-2.0.dll` 依赖，无法脱离 MSYS2 运行）。

### 1.1 为什么选 MSYS2 UCRT64（与其他"虚拟环境"对比）

| 方案 | 优点 | 局限 | 结论 |
|---|---|---|---|
| **MSYS2 UCRT64** | 原生 Win64 产物、pacman 包生态、GCC/Clang 双工具链、无需虚拟机 | 与 MSVC 二进制 ABI 不兼容；部分库无包 | **Windows 主环境** |
| MSYS2 CLANG64 | LLVM 工具链、静态分析/消毒器更完整 | 包覆盖略少、与 UCRT64 二进制不通用 | 实验/CI 辅助 |
| WSL2 + Ubuntu | 与 Linux 完全一致、脚本简单 | 跨文件系统 I/O 慢、GUI/GPU 透传复杂、不是原生 Windows 测试 | 仅用于 Linux 侧验证 |
| Docker / devcontainer | 可复现、CI 一致 | Windows 桌面 GUI 与 GPU 支持弱 | **Linux 构建与 CI** |
| vcpkg + MSVC | MSVC 生态、调试器成熟 | 违反 MSYS2 约束；与 MinGW 混用会灾难 | 不选 |
| Conda / pixi | Python 依赖隔离好 | 只解决 Python，不提供 C++ 工具链 | 仅 Python 边车 |
| Nix | 可复现性最强 | 学习曲线陡、Windows 支持弱 | 不选 |

> MSYS2 本身不是容器：可复现性靠"包清单 + 固定版本 + superbuild 锁定 + CI 缓存"，而不是文件系统隔离（见 §5）。

### 1.2 UCRT64 与 MSVCRT 的区别

- UCRT64 使用 Windows 10 起的 Universal C Runtime，C11/C17 兼容性更好，是 MSYS2 当前默认环境；
- `mingw64`（MSVCRT）为兼容旧系统保留，新项目不应选择；
- 同一项目的所有插件必须使用同一环境（UCRT64）构建，否则运行库/ABI 可能冲突。

---

## 2. 一次性安装

### 2.1 安装 MSYS2

从 <https://www.msys2.org/> 下载安装器，安装到默认路径 `C:\msys64`（路径不要含空格/中文）。

### 2.2 更新基础系统

打开 **MSYS2 UCRT64** 终端（开始菜单中的 "MSYS2 UCRT64"），执行：

```bash
pacman -Syu
# 若提示关闭终端，关闭后重新打开，再次执行
pacman -Su
```

### 2.3 安装工具链与依赖

使用仓库内脚本（推荐，自动跳过不存在的包并汇总）：

```bash
cd /c/UserProjects/SpaceRobotSimPlatform
./tools/env/bootstrap-msys2.sh
```

等价的手工命令：

```bash
pacman -S --needed --noconfirm \
  base-devel git \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-ccache \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-gdb \
  mingw-w64-ucrt-x86_64-clang \
  mingw-w64-ucrt-x86_64-clang-tools-extra \
  mingw-w64-ucrt-x86_64-compiler-rt
```

> `clang` / `clang-tools-extra` / `compiler-rt` 提供第二编译器、clangd/clang-tidy 与 ASan；
> 另有 CLANG64 环境（`mingw-w64-clang-x86_64-*`）仅作 libc++ 验证层，见 §7 预设表。

核心库（按需分批安装）：

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-eigen3 \
  mingw-w64-ucrt-x86_64-boost \
  mingw-w64-ucrt-x86_64-protobuf \
  mingw-w64-ucrt-x86_64-grpc \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-qt6-declarative \
  mingw-w64-ucrt-x86_64-qt6-svg \
  mingw-w64-ucrt-x86_64-qt6-tools \
  mingw-w64-ucrt-x86_64-assimp \
  mingw-w64-ucrt-x86_64-spdlog \
  mingw-w64-ucrt-x86_64-fmt \
  mingw-w64-ucrt-x86_64-nlohmann-json \
  mingw-w64-ucrt-x86_64-tomlplusplus \
  mingw-w64-ucrt-x86_64-yaml-cpp \
  mingw-w64-ucrt-x86_64-catch \
  mingw-w64-ucrt-x86_64-benchmark \
  mingw-w64-ucrt-x86_64-arrow \
  mingw-w64-ucrt-x86_64-zstd \
  mingw-w64-ucrt-x86_64-lz4
```

渲染与传感器（可选）：

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-vulkan-headers \
  mingw-w64-ucrt-x86_64-vulkan-loader \
  mingw-w64-ucrt-x86_64-shaderc \
  mingw-w64-ucrt-x86_64-glslang \
  mingw-w64-ucrt-x86_64-sdl2 \
  mingw-w64-ucrt-x86_64-freetype \
  mingw-w64-ucrt-x86_64-libpng \
  mingw-w64-ucrt-x86_64-libjpeg-turbo \
  mingw-w64-ucrt-x86_64-opencv
```

> 包名会随仓库演进。`tools/env/check-msys2-packages.sh` 会逐个检查并输出"已安装 / 可安装 / 仓库不存在"三态清单，缺失项按 §5 的自建策略处理。

### 2.4 校验

```bash
gcc --version && g++ --version
cmake --version && ninja --version
pkg-config --modversion eigen3
pacman -Q | wc -l
```

本机基线（2026-10-22 实测）：GCC 16.1.0、Clang 22.1.8（UCRT64，含 clangd/clang-tidy/compiler-rt）、Python 3.14.6 + pip 26.1.2、CMake 4.4.0、Ninja 1.13.2、ccache 4.13.6、Catch2 3.15.2、protobuf 35.1、gRPC 1.82.0（含 `python-grpcio` 1.82.0）、Qt 6.11.1、Eigen3、benchmark 均已安装；CLANG64 保留 clang 22 + libc++ + lld + lldb（仅验证层，1.87 GB）；Visual Studio Community 2026（MSVC 19.50）可用，`win-msvc-*` 预设已验证。已清理的重复项：clang-21 家族（约 7.1 GiB）、MSYS 层重复 cmake 4.3.4 及其孤儿依赖；清理后 `ucrt64` 3.55 GB、`clang64` 1.87 GB。

### 2.5 能力矩阵（2026-10-24 实测）

四套工具链**不是能力等价**的，差异来自第三方包的可获得性与运行时 ABI：

| 能力 | UCRT64 GCC | UCRT64 Clang | CLANG64（libc++） | MSVC |
|---|---|---|---|---|
| 构建（core / apps / tests） | ✅ | ✅ | ✅ | ✅ |
| 单元测试（`<model>/ModuleTest/unit`，Catch2） | ✅ 30 用例 | ✅ 30 用例 | ⛔ 跳过 | ⛔ 跳过 |
| 集成测试（`<model>/ModuleTest/integration`） | ✅ 7 | ✅ 7 | ✅ 4 | ✅ 3 |
| gRPC 边车桥接（`srsp_bridge`） | ✅ | ✅ | ⛔ 跳过 | ⛔ 跳过 |
| Python 边车（grpcio） | ✅ | ✅ | ⛔ 跳过 | ⛔ 跳过 |
| `ctest` 合计 | **20** | **20** | **10** | **9** |

跳过原因（不是缺陷，是当前环境的硬约束）：

- **Catch2 与单元测试**：Catch2 只在 UCRT64 前缀提供；CLANG64/MSVC 链接 UCRT64 的 libstdc++ 目标会失败，故模块单测仅在 UCRT64 启用；
- **gRPC 桥接**：`find_package(gRPC)` 在 CLANG64 下会找到 UCRT64 的包并带进 `-isystem .../ucrt64/include`，与 libc++ 标准库头冲突；MSVC 侧无 gRPC 包。桥接因此限定 UCRT64；
- 集成测试不依赖 Catch2，因此在四端都跑。

若后续要补齐 CLANG64/MSVC 的单元测试能力，需为这两个运行时各自提供 Catch2 构建（源码编译或 vcpkg），属于 M1 的 CI 工作项。

同一实测中**仍未安装**的项：Boost、Arrow、OpenCV、FFmpeg、Assimp、SDL2/Vulkan 等可选依赖；MSYS 层 `base-devel`/`git`；`VCPKG_ROOT` 未配置（`win-vcpkg-*` 预设待启用）。

**Python 归属**：schema 样例校验由 MSYS2 UCRT64 的 Python 驱动（隐藏预设 `win-python` 显式设置 `Python3_EXECUTABLE=C:/msys64/ucrt64/bin/python.exe`，`jsonschema` 由 pacman 包 `python-jsonschema` 提供），不再依赖系统安装的 Windows Python；Linux 预设仍走系统 `python3`，缺少 `jsonschema` 时该测试自动跳过。

---

## 3. 环境变量与启动方式

### 3.1 推荐：始终从 UCRT64 环境启动

```bat
C:\msys64\msys2_shell.cmd -ucrt64 -here -no-start
```

或在 Windows Terminal 中添加配置：

```json
{
  "name": "MSYS2 UCRT64",
  "commandline": "C:\\msys64\\msys2_shell.cmd -ucrt64 -here -no-start",
  "startingDirectory": "%USERPROFILE%"
}
```

关键环境变量（登录 shell 会自动设置，手工启动时需确认）：

| 变量 | 值 | 作用 |
|---|---|---|
| `MSYSTEM` | `UCRT64` | 选择 `/ucrt64` 前缀与工具链 |
| `CHERE_INVOKING` | `1` | 保持当前目录，不跳到 `$HOME` |
| `MSYS2_PATH_TYPE` | `inherit`（可选） | 继承 Windows PATH 中的额外工具 |
| `MSYS2_ARG_CONV_EXCL` | 按需 | 关闭特定参数的路径转换 |
| `SRSP_DATA_DIR` | 数据目录 | 星历/重力场/星表等只读数据 |

### 3.2 路径转换陷阱

从 MSYS2 shell 启动**原生 Windows 程序**时，形如 `/c/foo`、`/usr/local` 的参数会被自动转换为 Windows 路径。对 CMake 尤其危险：

```bash
# 错误：/usr/local 可能被转换为 C:/msys64/usr/local
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..

# 正确：显式使用 Windows 风格路径
cmake -DCMAKE_INSTALL_PREFIX=C:/opt/srsp ..

# 或临时禁用转换
MSYS2_ARG_CONV_EXCL='-DCMAKE_INSTALL_PREFIX' cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
```

在 CMakePresets 中一律使用 `C:/...` 风格，避免脚本行为随环境变化。

---

## 4. VSCode 配置

### 4.1 推荐启动方式

**方式 A（推荐）**：从 UCRT64 终端启动 VSCode，使 CMake Tools 与调试器继承完整环境：

```bash
cd /c/UserProjects/SpaceRobotSimPlatform
code .
```

**方式 B**：双击 `tools/env/launch-vscode.ps1`（或创建快捷方式），脚本会把 `C:\msys64\ucrt64\bin` 前置到 PATH 后再启动 VSCode。

> 仅靠 VSCode 内置终端配置不够：CMake Tools、clangd、调试器运行在扩展宿主进程中，必须让**扩展宿主**继承 PATH。

### 4.2 扩展

| 扩展 | 用途 | 备注 |
|---|---|---|
| `ms-vscode.cmake-tools` | 配置/构建/测试/预设集成 | 必需 |
| `llvm-vs-code-extensions.vscode-clangd` | 代码导航、补全、clang-tidy | 必需；IntelliSense 的唯一来源 |
| `ms-vscode.cpptools` | 仅用于调试（cppdbg） | 必需；`C_Cpp.intelliSenseEngine: disabled` |
| `xaver.clang-format` | 保存时格式化 C/C++ | 必需；`clang-format.executable` 指向 UCRT64 |
| `cheshirekow.cmake-format` | CMake 格式化 | 必需 |
| `ms-python.python` + `ms-python.vscode-pylance` | Python 边车与脚本 | 必需 |
| `drblury.protobuf-vsc` | `.proto` 语法/跳转 | 必需；**统一用它**，不再推荐 `zxh404.vscode-proto3` |
| `redhat.vscode-yaml` | YAML 校验 | 必需 |
| `github.vscode-github-actions` | CI 工作流 schema | 必需 |
| `tamasfe.even-better-toml` | TOML 校验 | 推荐 |
| `bierner.markdown-mermaid` | 文档 Mermaid 预览 | 推荐 |
| `davidanson.vscode-markdownlint` | Markdown lint | 推荐；行宽规则在 `.markdownlint.json` 关闭 |
| `editorconfig.editorconfig` | 尊重 `.editorconfig` | 推荐 |
| `matepek.vscode-catch2-test-adapter` | Test Explorer 跑 Catch2 用例 | 推荐（非强制，CI 才是门禁） |
| `cschlosser.doxdocgen` | 生成 Doxygen 注释骨架 | 推荐（与 `module-build` 技能一致） |
| `streetsidesoftware.code-spell-checker` | 拼写 | 可选；已配置忽略中文文档与标识符 |
| `vadimcn.vscode-lldb` | 更快的调试体验 | 可选 |
| `rust-lang.rust-analyzer` | Rust CLI（规划中） | 可选；待 `sdk/rust/` 落地 |

**不建议**：`ms-vscode.cpptools-extension-pack`（与 clangd 路线重复，只带来主题与 make 辅助）、`zxh404.vscode-proto3`（与 drblury 冲突）。

### 4.3 仓库内已提供

- `.vscode/settings.json`：预设、clangd、clang-format 绑定、终端、编码、拼写忽略；
- `.vscode/extensions.json`：推荐与不推荐扩展；
- `.vscode/tasks.json`：configure / build / test / docs_check / hello demo / cli inproc / cli sidecar / clang-format；
- `.vscode/launch.json`：hello host、`srsp-cli`（inproc、sidecar）、`test_plugin_host`（单测）、Linux hello host；
- `.clangd`：命令行 clangd 的默认参数（不含数据库路径，路径由扩展参数注入）；
- `.markdownlint.json`：关闭行宽等不适合中文文档的规则；
- 个人覆盖写 `.vscode/*.local.json`（已被 `.gitignore` 忽略）。

### 4.4 clangd 与 compile_commands.json

CMake 配置时导出 `compile_commands.json`，并由 CMake Tools 复制到当前模型的缓存目录 `<model>/Temp/`（示例为 `deepseek`）：

```json
{
  "cmake.copyCompileCommands": "${workspaceFolder}/${env:SRSP_AI_MODEL}/Temp/compile_commands.json",
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}/${env:SRSP_AI_MODEL}/Temp",
    "--background-index",
    "--clang-tidy",
    "--header-insertion=never"
  ],
  "C_Cpp.intelliSenseEngine": "disabled"
}
```

> 让 clangd 与 GCC 编译器版本匹配即可；clangd 只做语义分析，不参与构建。

**多工具链约定（重要）**：`compile_commands.json` 只有一份，后配置的预设会覆盖它。为避免索引与真实构建不一致：

1. clangd **只跟 `win-ucrt64-debug`**；
2. 切到 `win-ucrt64-clang-debug` / `win-clang64-debug` / `win-msvc-debug` 后，clangd 索引可能报错——这属于预期，用命令行 `ctest` 验证该工具链即可；
3. 改回 UCRT64 后执行一次 configure，再 `clangd: Restart language server`。

命令行使用 clangd 时显式指定数据库：

```bash
clangd --compile-commands-dir=<model>/Temp
```

---

## 5. 依赖管理策略

### 5.1 优先级

1. **MSYS2 pacman**（Windows）/ **apt**（Linux）：优先使用，版本与 ABI 由发行方保证一致。
2. **superbuild（FetchContent/ExternalProject）**：仅用于仓库缺失或版本不满足的库，固定 tag + 源码哈希，统一安装到 `<model>/Temp/build/_deps/install`。
3. **系统自带**：Linux 上的 OpenGL/Vulkan 驱动、X11 库。
4. **不使用** vcpkg 默认 triplet（MSVC 预编译为主）；Conan 2 仅作为团队统一后的备选。

### 5.2 自建依赖清单（预计）

| 库 | 原因 | 方案 |
|---|---|---|
| bgfx / bx / bimg | 仓库可能无包或版本旧 | FetchContent，固定 tag，`BGFX_CONFIG_RENDERER_*` 按平台裁剪 |
| CSPICE | NAIF 仅提供源码 | 下载源码包，CMake 包装为静态库 |
| MCAP | 仓库可能无包 | FetchContent（C++ 实现），依赖 zstd/lz4 用系统包 |
| Pinocchio（可选） | 仓库无包 | superbuild：pinocchio + hpp-fcl/coal + urdfdom + jrl-cmakemodules |
| MuJoCo（可选） | 仓库可能无包 | 官方预编译包（MinGW 兼容性需验证）或源码构建 |
| NRLMSISE-00 / IGRF | 无包 | 源码内置 `third_party/`，保留许可证与出处 |

### 5.3 版本锁定与可复现

- 提交 `pacman -Q` 导出清单作为环境快照（`tools/env/packages.txt` 是所需包集合，不是版本快照）；
- superbuild 依赖必须固定 `GIT_TAG` 与 `URL_HASH`，禁止使用分支名；
- CI 缓存 `C:\msys64` 安装目录，键为包清单哈希；
- 发布构建记录：编译器版本、CMake 版本、全部依赖版本、git commit、构建时间。

### 5.4 离线/内网

- `tools/env/offline/` 存放 `.pkg.tar.zst` 离线包与安装脚本；
- superbuild 依赖预先下载到 `third_party/_cache/`，CMake 配置 `FETCHCONTENT_SOURCE_DIR_*` 指向本地；
- 文档提供"离线构建清单"，包含每个依赖的下载地址与 SHA-256。

---

## 6. 仓库结构与构建目标

```
SpaceRobotSimPlatform/
├─ AGENTS.md                # 模型无关 Agent 规则（缓存/计划/日志）
├─ ai-models.json           # 模型命名空间注册表
├─ CMakeLists.txt
├─ CMakePresets.json
├─ api/                     # 契约：C 头文件、proto、schema（可独立安装）
│  ├─ include/srsp/plugin_api.h
│  ├─ proto/srsp/v1/*.proto
│  └─ schema/*.json
├─ core/                    # libsrsp：内核
│  ├─ runtime/  bus/  time/  frames/  registry/  plugin_host/
├─ plugins/                 # 官方插件（每个子目录一个 CMake 目标）
│  ├─ model_urdf/  dynamics_rigid/  env_spice/  renderer_bgfx/ ...
├─ apps/
│  ├─ studio/               # srsp-studio（Qt）
│  ├─ cli/                  # srsp-cli
│  └─ bridge/               # srsp-bridge（Python 边车宿主/示例）
├─ sdk/
│  ├─ cpp/  python/  matlab/
├─ tests/
│  ├─ unit/  integration/  conformance/  bench/
├─ deepseek/                # 模型命名空间：Temp / Logs/Plan / Logs/Log（其他模型为 claude/、gpt/…）
├─ third_party/             # 自建依赖与补丁
├─ tools/
│  ├─ ai/                   # 模型解析、环境注入、计划/日志脚本与模板
│  ├─ env/  packaging/  codegen/  schemas/
├─ examples/
│  ├─ hello_plugin/  python_plugin/
└─ docs/
```

CMake 目标命名：

| 目标 | 类型 | 说明 |
|---|---|---|
| `srsp_api` | INTERFACE | 只含头文件与 proto 生成规则 |
| `srsp_core` | SHARED | 内核；导出稳定符号 |
| `srsp_plugin_<name>` | MODULE | 插件动态库，输出到 `plugins/<id>/` |
| `srsp-studio` | EXECUTABLE | Qt 应用（WIN32 / MACOSX_BUNDLE） |
| `srsp-cli` | EXECUTABLE | 控制台 |
| `srsp_tests` | EXECUTABLE | Catch2 测试聚合 |

---

## 7. CMakePresets 设计

已提供根目录 `CMakePresets.json`，核心约定：

| Preset | 用途 |
|---|---|
| `win-ucrt64-debug` | Windows 调试（Ninja + GCC，`-O0 -g`） |
| `win-ucrt64-release` | Windows 发布（`-O2 -DNDEBUG`，`-ffp-contract=off`） |
| `win-msvc-debug` / `win-msvc-release` | Windows MSVC（VS 2022+ vcvars 环境，无第三方依赖；用于跨编译器验证） |
| `win-vcpkg-debug` / `win-vcpkg-release` | Windows MSVC + vcpkg（需要 `VCPKG_ROOT`，解析根目录 `vcpkg.json`） |
| `win-ucrt64-clang-debug` / `-release` | Windows UCRT64 + Clang（同一份 libstdc++，可与 GCC 产物互链；用于 ABI 断言、clang-tidy、ASan） |
| `win-clang64-debug` | Windows CLANG64 + libc++（**仅验证层**，产物不分发；cmake/ninja 复用 UCRT64；test 预设自带 `clang64/bin` 到 PATH 以解析 libc++ 运行库） |
| `linux-gcc-debug` / `linux-gcc-release` | Linux 构建 |
| `linux-clang-asan` | Linux ASan + UBSan |

> Windows 下的 MinGW 消毒器支持有限，暂不提供；内存错误优先在 Linux ASan 作业中发现。

MSVC 预设（`win-msvc-*` / `win-vcpkg-*`）需要 VS 2022+ 的开发环境（`vcvars64.bat`），编译器通过 `CC=cl`、`CXX=cl` 指定——注意：当 MSYS2 的 `bin` 在 `PATH` 中靠前时，CMake 会优先选到 `cc/c++` 而非 `cl`，务必在 vcvars 环境中显式设置这两个变量。跨工具链混载见 02 文档 §4.1「插件自包含」。

**MSYS2 多环境隔离规则（强制）**：

1. `PATH` 中一次只放一个 MSYS2 环境的 `bin`（UCRT64 / CLANG64 / MSYS 互斥）——同名 DLL（`zlib1.dll`、`libpng16.dll` 等）会按 `PATH` 顺序被错误加载；
2. 预设一律写死编译器绝对路径，不依赖 `PATH` 决定；
3. UCRT64 与 CLANG64 的 C++ 运行时不同（libstdc++ vs libc++），**禁止互链 C++ 目标**，只允许纯 C ABI 插件加载或边车；
4. MSYS 子系统只跑脚本（bash/pacman/make），不产出可交付二进制。

要点：

- 编译器用绝对路径（`C:/msys64/ucrt64/bin/gcc.exe`），避免 CMake 探测到 MSVC；
- `CMAKE_MAKE_PROGRAM` 显式指向 `ninja.exe`；
- `CMAKE_EXPORT_COMPILE_COMMANDS=ON`；
- `CMAKE_CXX_COMPILER_LAUNCHER=ccache`（可选开关）；
- 构建目录统一 `<model>/Temp/build/<preset>`（由环境变量 `SRSP_AI_MODEL` 解析，见根目录 `AGENTS.md` 与 `ai-models.json`）；
- 未设置 `SRSP_AI_MODEL` 时，顶层 `CMakeLists.txt` 直接报错，避免把构建树写到仓库根；
- 任何预设都不修改全局环境变量。

常用命令：

```bash
# 选择当前模型命名空间（示例：deepseek；bash）
export SRSP_AI_MODEL=deepseek

cmake --preset win-ucrt64-debug
cmake --build --preset win-ucrt64-debug -j
ctest --preset win-ucrt64-debug --output-on-failure
cmake --install deepseek/Temp/build/win-ucrt64-release --prefix dist/srsp-1.0.0-win64
```

---

## 8. 测试与质量

| 类型 | 工具 | 入口 |
|---|---|---|
| 单元测试 | Catch2 v3 + CTest | `ctest --preset ...` |
| 契约一致性 | `srsp-conformance` | `ctest -R conformance` |
| 数值回归 | 黄金轨迹 + `state_hash` | `tests/integration/golden/` |
| 性能基准 | Google Benchmark | `srsp_bench` |
| 静态检查 | clang-tidy、cppcheck | `tools/ci/static-analysis.sh`（随 M1 提供） |
| 格式 | clang-format（`.clang-format`） | **门禁**：`ctest -R format_check`（`tests/style/check_format.py`）；修复：`clang-format -i` |
| 文档与数据 | JSON/YAML 解析 + Markdown 链接 | **门禁**：`ctest -R docs_check`（`tests/docs/check_docs.py`） |
| 内存/线程 | Linux ASan/UBSan、TSan；Windows 用 Dr. Memory | `linux-clang-asan` 预设 |

格式门禁说明：`format_check` 遍历 `src/ apps/ tests/ examples/` 下的 C/C++ 源文件，用 `clang-format --dry-run --Werror` 校验；机器上没有 clang-format 时脚本打印 SKIP 并以 0 退出（CI 的 Linux job 通过 apt 安装 `clang-format`，因此该门禁在 CI 生效）。

覆盖率：Linux + gcov/lcov（`SRSP_COVERAGE=ON`），目标核心模块 ≥ 70%。

---

## 9. 调试与剖析

- **进程内插件**：VSCode `cppdbg` + gdb，断点可直接下在插件源码；
- **边车**：`--debug-wait` 挂起等待附加；Python 用 `debugpy`，C# 用 `vsdbg`；
- **崩溃**：Windows 生成 minidump（`MiniDumpWriteDump`），Linux 生成 core；符号文件单独归档；
- **性能**：Tracy（帧/步/插件调用区间）、`perf`/VTune（Linux）、GPU 用 RenderDoc/Nsight；
- **日志**：`SRSP_LOG_LEVEL=debug`，`SRSP_TRACE_PLUGIN=<id>` 打印每次调用。

---

## 10. CI

工作流文件：`.github/workflows/ci.yml`（2026-10-24 落地，替代本文档早期的草案）。

| Job | Runner | 内容 |
|---|---|---|
| `windows-ucrt64` | `windows-latest` + `msys2/setup-msys2` (UCRT64) | 安装 GCC/CMake/Ninja/Catch2/nlohmann-json/protobuf/gRPC/python(jsonschema,yaml) → `win-ucrt64-debug` 配置、构建、`ctest` |
| `linux-gcc` | `ubuntu-24.04` | apt 安装 cmake/ninja/g++/python3(+jsonschema,yaml)/libprotobuf/libgrpc++/nlohmann/catch2 → `linux-gcc-debug` 配置、构建、`ctest` |
| `docs` | `ubuntu-24.04` | `python3 tests/docs/check_docs.py`：JSON/YAML 解析 + Markdown 相对链接 |

约定：

- 三个 job 都设置 `SRSP_AI_MODEL=ci`，构建目录落在 `ci/Temp/build/<preset>`（已被 `.gitignore` 忽略）；
- 门禁：构建 + 单测 + 契约测试必须通过；性能基准仅记录趋势，回归超过 15% 时告警（基准尚未进 CI）；
- **本地等价命令**（Windows）：`cmake --preset win-ucrt64-debug && cmake --build --preset win-ucrt64-debug && ctest --preset win-ucrt64-debug --output-on-failure`；
- 文档检查也可单独跑：`python tests/docs/check_docs.py`（缺少 PyYAML 时只跳过 YAML 部分）。
- **Linux 的边车桥接默认不启用**：桥接层需要 protobuf/gRPC 的 CMake config，而 Ubuntu 只提供 pkg-config 文件；Linux 端因此覆盖内核、CLI（inproc）、全部契约与文档测试，边车路径由 Windows 端覆盖（要启用需用 vcpkg/conan 的 gRPC 构建）。

已验证 / 未验证（如实说明）：

- ✅ 工作流 YAML 可解析（3 个 job、步骤数正确）；`check_docs.py` 本地通过；本地四端 ctest 全绿；
- ⚠️ **GitHub Actions 的实际运行未验证**（本地无 runner）。首次推送后需要看首轮 CI 结果再收紧。
---

## 11. 常见问题（排错手册）

| 现象 | 原因 | 处理 |
|---|---|---|
| `cmake` 找到 MSVC 或 "No CMAKE_CXX_COMPILER" | 未继承 MSYS2 PATH，或从普通 PowerShell 启动 | 用 UCRT64 终端启动 VSCode；或在预设中写死编译器路径 |
| 运行时提示缺少 `libstdc++-6.dll` / `libgcc_s_seh-1.dll` | 未随包分发 MinGW 运行库 | 打包时收集（见 06 文档），或对可执行文件静态链接运行库 |
| 运行时提示缺少 Qt 平台插件 `windows` | 未运行 `windeployqt` 或 `qt.conf` 路径错 | 部署 `platforms/qwindows.dll`，设置 `qt.conf` |
| CMake 参数中的 `/usr` 变成 `C:/msys64/usr` | MSYS2 路径自动转换 | 用 `C:/...` 或 `MSYS2_ARG_CONV_EXCL` |
| 构建产物在别的机器上崩溃 | 混入 MSVC 预编译库 / 依赖了 msys 运行时 | 统一用 UCRT64 构建；`objdump -p` 检查导入表 |
| `pip install` 找不到 wheel | MSYS2 Python 版本过新或平台标签不匹配 | Python 边车用独立 venv（官方 CPython），不要把 MSYS2 Python 当发布运行时 |
| clangd 报大量红波浪线 | 未生成/未找到 `compile_commands.json` | 重新配置；检查 `cmake.copyCompileCommands` |
| Ninja 构建偶发文件占用 | 杀毒软件实时扫描 | 将构建目录加入排除项 |
| Git 检出后 shell 脚本报 `\r` 错误 | `core.autocrlf=true` | `.gitattributes` 强制 `*.sh text eol=lf` |
| 路径含中文导致工具失败 | 部分工具不支持非 ASCII 路径 | 仓库与构建目录使用纯 ASCII 路径 |
| OpenGL 初始化失败（远程桌面） | RDP 无 GPU/驱动降级 | 使用 Vulkan/D3D11 后端，或软件渲染兜底 |
| Linux 下窗口创建失败 | Wayland 会话 | 使用 X11/XWayland 会话，或切换 Qt Quick 3D 渲染器 |

---

## 12. 环境验收清单（新成员第一天）

- [ ] `gcc --version` 显示 UCRT64 的 GCC 13+
- [ ] 已设置模型命名空间（`export SRSP_AI_MODEL=<slug>` 或 `. .\tools\ai\Enter-AgentEnv.ps1 -Model <slug>`）
- [ ] `cmake --preset win-ucrt64-debug` 配置成功
- [ ] `cmake --build --preset win-ucrt64-debug` 构建通过
- [ ] `ctest --preset win-ucrt64-debug` 全部通过
- [ ] VSCode 中 `F5` 可启动示例宿主并命中插件断点
- [ ] `srsp-cli --version` 可运行（M1 之后）
- [ ] 能打开并播放一个示例场景（M1 之后）

---
## 11. M1 实测性能（2026-10-26，Release）

`bench/` 下的基准可复现（`ctest --preset win-ucrt64-release -R bench -V`）。

| 指标 | 数值 | 说明 |
|---|---|---|
| 100 体链 · 质量矩阵装配 | 2.141 ms | `M = Σ Jᵀ I J`，J 为 6×306 |
| 100 体链 · 偏置力（RNEA） | 0.009 ms | 世界系 Newton-Euler 递推 |
| 100 体链 · 单步（RK4） | **9.058 ms** | 4 次 M 装配 + 4 次 306×306 LDLT |
| 预算 | 300 μs | docs/07 M1 判据 → **未达标（约 30×）** |
| MCAP 写 60 000 条（256 B） | 0.057 s（≈ 29.5 MB/s） | 10 分钟 @100 Hz 容量充足 |
| MCAP 读回 | 0.072 s（≈ 202.8 MB/s） | — |
| 2 000 步哈希重放 | 0 处不一致 | XXH3-128 |

优化方向已立项（M1.4d）：铰接体算法（ABA）或 CRBA + 稀疏/带状求解；ABA 首次尝试未通过等价性测试，已回退（P26）。

---

## 12. 环境安装分级（必须 / 按需 / 延后）

依据执行计划 v4（本地）整理，避免提前引入不必要的 ABI 与磁盘成本。

### 12.1 必须（当前已满足）

| 项 | 现状 |
|---|---|
| MSYS2 UCRT64 工具链（GCC 16.1、CMake 4.4、Ninja、ctest、ccache） | ✅ 实测 34/34 |
| Git + `credential.helper=manager` | ✅ 可推送 |
| Qt 6.11（base/tools） | ✅ 已装 |
| 构建期 Python（protoc 插件） | ✅ 系统 Python 3.14 + MSYS2 grpcio |

### 12.2 按需（M1 收口时才需要）

| 项 | 触发条件 | 成本 |
|---|---|---|
| Python 互操作环境（uv/venv + `numpy`/`mujoco`/`mcap`） | W7 MuJoCo 边车、MCAP 官方 SDK 交叉校验 | 2–4 GB |
| micromamba + `pinocchio` | Pinocchio 数值对照 | 1–2 GB |

### 12.3 延后（有明确触发点）

| 项 | 触发条件 |
|---|---|
| MSVC Build Tools / vcpkg | 需要 CUDA 或改用 bgfx / 原生重依赖（约 25–40 GB） |
| Vulkan SDK | 选择 Vulkan 渲染后端或 GPU compute（D3D11 路线不需要） |
| CUDA Toolkit | 2.0 的可选 NVIDIA 加速 |
| WSL2 + Ubuntu 24.04 | 1.2 跨平台阶段 |
| Doxygen / graphviz、clang-tidy / cppcheck | 文档生成、静态分析专项 |

### 12.4 安装路径约定

| 项 | 路径 |
|---|---|
| MSYS2 | `C:\msys64`（仅用 `ucrt64` 前缀） |
| Python venv | `deepseek\Temp\venv`（被 `.gitignore` 的 `/*/Temp/*` 覆盖） |
| micromamba 根 | `deepseek\Temp\mamba`（`MAMBA_ROOT_PREFIX`） |
| ccache | `deepseek\Temp\ccache`（`CCACHE_DIR`） |
| vcpkg（延后） | `C:\vcpkg`（`VCPKG_ROOT`） |
| Vulkan SDK（延后） | `C:\VulkanSDK\<version>`（`VULKAN_SDK`） |

规则：互操作脚本必须使用**绝对路径解释器**（如 `deepseek\Temp\venv\Scripts\python.exe`），禁止裸 `python`，避免 PATH 串台（P24/P30）。
### 12.5 CI 作业范围（2026-10-26 更新）

按「当前只支持 Windows 10 + UCRT64」的决策，CI 取消搁置平台的测试作业：

| 作业 | 状态 | 原因 |
|---|---|---|
| `windows-ucrt64` | ✅ 保留 | 主力构建与 34/34 测试 |
| `docs`（JSON/YAML/链接检查） | ✅ 保留 | 与平台无关 |
| `linux-gcc` | ❌ 已移除 | Linux 跨平台推迟到 1.2；本地 WSL2 亦未安装 |
| MSVC / CLANG64 | 未纳入 | MSVC 待 vcvars 环境（P27）；CLANG64 已移除（P28） |

恢复条件：1.2 跨平台里程碑启动时，重新加入 `linux-gcc` 作业（直接复用本次删除前的定义，见 git 历史）。

按 docs/01 §7.1 的支持等级规则，**Linux 当前为 planned**：没有 CI 作业即不算 supported；UCRT64 Clang 与 MSVC 为 **unverified**（本地可构建但无 CI 证据），CLANG64 为 **experimental**（预设已移除）。
### 11.1 300 µs 基准的固定工况（U6 已决，2026-10-28）

为避免不同人测出不同数字，`bench.dynamics` 的统计工况固定如下：

| 项 | 取值 |
|---|---|
| 拓扑 | 100 体**单链**，每体质量 10 kg，惯量 diag(0.05, 0.05, 0.01)，关节间距 0.2 m |
| 关节 | 99 个转动关节，轴 `+z`，限位 ±π |
| 积分器 | RK4，`dt = 1 ms` |
| 预热 | 1 步（不计入统计） |
| 统计窗口 | 50 步取平均（`mass_ms` / `bias_ms` / `step_ms` 各自独立） |
| 构建 | Release（`-O2`/`-O3`），单线程，`-ffp-contract=off` |
| 机器 | 记录 CPU 型号与频率；跨机器比较只看同型号 |
| 判据 | `step_us ≤ 300` 为达标；当前 9 058 µs（已豁免到 1.1） |

改动此工况必须同时更新本表与 `bench/bench_dynamics.cpp`，否则数字不可比。