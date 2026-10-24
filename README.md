# SRSP — 空间机器人可视化仿真平台

面向在轨服务、目标捕获、舱外装配与遥操作训练的空间机器人可视化仿真软件。
采用 **平台 + 插件** 架构：机器人模型、动力学、空间环境、传感器、渲染引擎、控制器均为可独立分发、可动态替换的插件，通过**版本化的标准数据与调用接口**协作。

> 当前状态：**M0 基本完成**（契约 6/6、内核骨架、双传输、CLI、CI）。
> 已实现：C ABI 与宿主侧 C API、内核（插件宿主/注册表/时钟/调度/诊断）、`srsp-cli`（`inproc` 与 `sidecar` 双传输）、gRPC 边车与 Python 边车、能力消息 schema、CI（Windows UCRT64 + Linux GCC + 文档检查）。
> 待办：M1（可视化、录制回放、空间环境）。详见 [07 路线图](docs/07-roadmap-risks.md)。

---

## 1. 需求与方案对照

| 原始需求 | 方案落点 | 详见 |
|---|---|---|
| 平台 + 插件，模型/动力学/环境/可视化均为插件 | 内核 + 能力（Capability）注册表；场景只依赖能力不依赖插件 ID | [01 架构](docs/01-architecture.md)、[02 插件契约](docs/02-plugin-contract.md) |
| 通过标准数据和调用接口动态调用 | 控制面 Protobuf、数据面 Arrow、场景 glTF、模型 SRM；进程内 C ABI 与边车 gRPC 同语义 | [02](docs/02-plugin-contract.md)、[03 数据契约](docs/03-data-contracts.md) |
| 开发环境采用 MSYS2，VSCode 编辑 | MSYS2 UCRT64 + CMake Presets + Ninja；VSCode 工作区与任务已配置 | [05 MSYS2 环境](docs/05-msys2-dev-environment.md) |
| 语言不限制，不同组件可用不同语言 | C/C++/Rust 进程内（C ABI）；Python/C#/Java/MATLAB/Julia 边车（gRPC + 共享内存） | [02](docs/02-plugin-contract.md)、[04 §11](docs/04-subsystems-and-stack.md) |
| 可打包，运行于 Windows 与 Linux | 可重定位 runtime、`.srpkg` 插件包、Inno Setup/AppImage/deb、依赖收集与冒烟测试 | [06 打包交付](docs/06-packaging.md) |

---

## 2. 关键决策

| # | 决策 | 一句话理由 |
|---|---|---|
| 1 | 内核 C++20，插件契约为**纯 C ABI** | 跨语言 FFI 成本最低，ABI 可控、可版本化 |
| 2 | 控制面 **Protobuf**，数据面 **Arrow C Data Interface** | 小消息用 schema，大数组零拷贝，生态成熟 |
| 3 | **进程内 + 边车** 双传输，语义一致 | 兼顾性能与语言自由/故障隔离 |
| 4 | 自定义模型 IR **SRM**，URDF/SDF/MJCF 仅作导入 | 空间机器人需要浮基、轮/CMG、推力器、柔性扩展 |
| 5 | 渲染器为插件，默认 **bgfx**，Qt Quick 3D 兜底 | 多后端、自定义管线、BSD 许可；Wayland 有退路 |
| 6 | Python **不内嵌**，作为边车 | 规避 MinGW/MSVC ABI、GIL 与崩溃传染 |
| 7 | 录制用 **MCAP** | 自描述、可索引、跨语言，Foxglove 可直接打开 |
| 8 | Windows 用 **MSYS2 UCRT64** 原生构建 | 满足约束、包生态完整、产物为原生 Win64 |
| 9 | v1 单进程，但接口按可跨进程设计 | 先交付，保留拆分为 `srspd` 服务的能力 |
| 10 | 内核对外暴露宿主侧 C API（`core_api.h`） | Rust CLI、Python SDK、Web 服务等非 C++ 应用可嵌入内核 |
| 11 | Windows 双工具链：UCRT64 为主、MSVC 为辅 | 插件按 `os-arch-toolchain` 分发；跨工具链只允许 C ABI 加载或边车 |

---

## 3. 架构总览

```
┌──────────────────────────────────────────────────────────────┐
│ 应用：srsp-studio (Qt) · srsp-cli · srsp-web · Python SDK     │
├──────────────────────────────────────────────────────────────┤
│ 内核 libsrsp：调度 · 注册表 · 数据总线 · 时间 · 坐标系 · 插件宿主 │
├───────────────────────────┬──────────────────────────────────┤
│ 进程内插件（C ABI）        │ 边车插件（gRPC + 共享内存）        │
│ 动力学 / 环境 / 传感器 /    │ Python 控制器 / Unity 渲染 /       │
│ 渲染 / 控制器 / 记录        │ MATLAB / JVM                      │
├───────────────────────────┴──────────────────────────────────┤
│ 契约层：C ABI · Protobuf · JSON Schema · Arrow · glTF · MCAP   │
└──────────────────────────────────────────────────────────────┘
```

- 一次宏步：采样传感器 → 控制器 → 力模型累加 → 动力学步进 → 发布不可变快照 → 渲染/录制异步消费。
- 浮基多体 + 轮/CMG + 推力器 + 空间环境（星历/重力/大气/SRP/光照）+ 相机/星敏/LiDAR 传感器。
- 轨道尺度渲染采用**双原点 + 反转 Z**，解决 float32 在 10⁷ m 量级的抖动。

---

## 4. 文档地图

| 文档 | 内容 |
|---|---|
| [01 总体架构](docs/01-architecture.md) | 目标与边界、分层、进程/线程模型、调度循环、确定性、故障处理、质量指标 |
| [02 插件体系与接口契约](docs/02-plugin-contract.md) | 清单格式、生命周期、C ABI 规则、能力方法表、版本兼容、边车协议、隔离与热重载 |
| [03 标准数据与领域模型](docs/03-data-contracts.md) | 单位/坐标系/时间约定、SRM 模型、Arrow 状态表、话题、场景/渲染协议、MCAP 录制 |
| [04 子系统与技术选型](docs/04-subsystems-and-stack.md) | 动力学、力/接触、空间环境、传感器、渲染、UI、脚本；技术栈总表与备选对比 |
| [05 MSYS2 开发环境](docs/05-msys2-dev-environment.md) | 安装清单、环境变量、VSCode、CMake Presets、依赖策略、CI、排错手册 |
| [06 打包与跨平台交付](docs/06-packaging.md) | 产物矩阵、目录布局、DLL 收集、安装器、AppImage/deb、`.srpkg`、签名、冒烟 |
| [07 路线图与风险](docs/07-roadmap-risks.md) | M0–M4 里程碑、出口标准、团队、风险登记、质量门禁、前两周清单 |

---

## 5. 仓库结构

```
SpaceRobotSimPlatform/
├─ AGENTS.md                         # 模型无关的 Agent 规则（DSH/Codex 等自动加载）
├─ ai-models.json                    # 模型命名空间注册表
├─ CMakeLists.txt / CMakePresets.json
├─ .github/workflows/ci.yml          # CI：Windows UCRT64 / Linux GCC / 文档检查
├─ api/
│  ├─ include/srsp/plugin_api.h      # 稳定 C ABI（v1.0）
│  ├─ include/srsp/core_api.h        # 宿主侧 C API（draft 0.1，CLI/SDK 嵌入内核）
│  ├─ proto/srsp/v1/plugin.proto     # 控制面消息与边车服务
│  └─ schema/                        # manifest 与能力消息 schema + 正反例
├─ src/
│  ├─ core/                          # 内核：srsp_kernel（内部 C++）+ srsp_core（C API 共享库）
│  └─ bridge/                        # gRPC 边车桥接：Server / Host / Process
├─ apps/
│  ├─ cli/                           # srsp-cli（version / demo）
│  └─ sidecar/                       # srsp-sidecar（把插件暴露为 gRPC 服务）
├─ deepseek/                         # DeepSeek 命名空间（Kimi 为同级 kimi/）
│  ├─ Temp/                          # 缓存（不纳入版本管理）
│  ├─ ModuleTest/                    # 模型工作区的开发期测试（unit / integration）
│  └─ Logs/                          # Plan/ 与 Log/（CHANGELOG、ISSUES）
├─ tools/
│  ├─ ai/                            # 工作流、技能、脚本、模板
│  ├─ env/                           # MSYS2 引导、包检查、VSCode 启动
│  ├─ sidecar/                       # Python 边车与生成的 gRPC 桩
│  └─ packaging/                     # 运行库收集
├─ examples/
│  ├─ hello_plugin/                  # C++ 插件 + 宿主 + 清单
│  └─ python_plugin/                 # Python 边车插件（SDK 目标形态）
├─ tests/
│  ├─ abi/                           # ABI 布局 static_assert
│  ├─ unit/                          # 版本协商等契约测试
│  ├─ schema/                        # manifest 与能力消息样例校验
│  └─ docs/                          # JSON/YAML 解析与 Markdown 链接检查
├─ vcpkg.json                        # MSVC 工具链依赖清单
├─ docs/
└─ .vscode/
```

---

## 6. Agent 工作区约定（多模型）

本仓库的 agent 规则与工具是**模型无关**的：DeepSeek、Claude、GPT、Gemini、Qwen、GLM、Llama、Mistral、Grok、Kimi、Doubao、ERNIE 等均可使用，各自拥有独立命名空间。

- **命名空间**：`<model>/`，唯一真源 [`ai-models.json`](ai-models.json)；当前仓库内实际存在 `deepseek/` 与 `kimi/` 两个工作区。
- **缓存**：`<model>/Temp/`；构建目录 `<model>/Temp/build/<preset>`。
- **计划**：`<model>/Logs/Plan/YYYY-MM-DD-<slug>.md`。
- **日志**：`<model>/Logs/Log/CHANGELOG.md`，按日期分组、同日 `V1/V2/...`，只记日期不记时间。
- **共享脚本**：[`tools/ai/`](tools/ai/README.md)，全部通过 `-Model <slug>` 或 `SRSP_AI_MODEL` 解析命名空间。
- **入口文件**：只保留 [`AGENTS.md`](AGENTS.md)（DSH/Codex 等直接读取；无专用入口的工具同样读取本文件）。

```powershell
# Windows PowerShell 默认策略可能拦截本地脚本，按需仅对当前进程放开
Set-ExecutionPolicy -Scope Process Bypass -Force
. .\tools\ai\Enter-AgentEnv.ps1 -Model deepseek
```

> CMake 通过 `SRSP_AI_MODEL` 决定构建目录；未设置时配置会直接失败，避免把缓存写到仓库根。

---

## 7. 快速开始

### 7.1 Windows（MSYS2 UCRT64）

1. 安装 [MSYS2](https://www.msys2.org/)，打开开始菜单中的 **MSYS2 UCRT64**。
2. 引导依赖：

```bash
cd /c/UserProjects/SpaceRobotSimPlatform
./tools/env/bootstrap-msys2.sh
```

3. 从 PowerShell 启动带工具链环境的 VSCode：

```powershell
./tools/env/launch-vscode.ps1 -Model deepseek
```

4. 构建并运行最小示例（以 DeepSeek 命名空间为例）：

```bash
export SRSP_AI_MODEL=deepseek
cmake --preset win-ucrt64-debug
cmake --build --preset win-ucrt64-debug
./deepseek/Temp/build/win-ucrt64-debug/bin/srsp_hello_host.exe \
  ./deepseek/Temp/build/win-ucrt64-debug/plugins/org.srsp.examples.hello/bin/srsp_hello.dll
```

预期输出（已验证）：

```
loaded org.srsp.examples.hello 0.1.0 (ABI 1.0)
[INFO] hello plugin initialised
== core.describe ==
{"id":"org.srsp.examples.hello", ...}
checkpoint: 16 bytes at step 3
after 2 more steps: {"ok":true,"steps":5}
after restore:      {"ok":true,"steps":3}
after one update:   {"commands":[],"steps":4}
[INFO] hello plugin shut down
log lines: 2
```

> 2026-10-24 实测：以上输出逐行一致（含最后三行）。`ctest` 应显示 20/20 通过。

### 7.2 Linux（Ubuntu 22.04+）

```bash
sudo apt install build-essential cmake ninja-build gdb
export SRSP_AI_MODEL=deepseek
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
./deepseek/Temp/build/linux-gcc-debug/bin/srsp_hello_host \
  ./deepseek/Temp/build/linux-gcc-debug/plugins/org.srsp.examples.hello/bin/srsp_hello.so
```

> CMake 需 ≥ 3.27；Ubuntu 22.04 自带版本较旧，可从 Kitware APT 源安装。

### 7.3 多工具链构建（可选）

同一份源码可在四套工具链上构建与测试（预设见 [05 §7](docs/05-msys2-dev-environment.md)）：

| 预设 | 工具链 | 用途 |
|---|---|---|
| `win-ucrt64-debug` | GCC 16（UCRT64） | 主工具链，发布用它 |
| `win-ucrt64-clang-debug` | Clang 22（UCRT64，libstdc++） | 第二编译器、clang-tidy、ASan |
| `win-clang64-debug` | Clang 22（CLANG64，libc++） | 仅验证层，产物不分发 |
| `win-msvc-debug` | MSVC 19.50 | 生态层（Qt 官方包、vcpkg、调试器） |

规则：`PATH` 一次只放一个 MSYS2 环境；MSVC 预设需在 `vcvars64.bat` 环境里设置 `CC=cl`、`CXX=cl`；跨工具链只允许纯 C ABI 插件加载，不做 C++ 混链（详见 [02 §4.1](docs/02-plugin-contract.md)）。

---

## 8. 环境验证状态

| 项 | 状态 |
|---|---|
| GCC 16.1.0（MSYS2 UCRT64） | ✅ 构建 + `ctest` **20/20** |
| Clang 22.1.8（MSYS2 UCRT64，libstdc++） | ✅ 构建 + `ctest` **20/20**；与 GCC 插件双向互载 |
| Clang 22.1.8（MSYS2 CLANG64，libc++） | ✅ 仅验证层：构建 + `ctest` **10/10**（单元测试与边车桥接按设计跳过）；产物不分发 |
| MSVC 19.50（VS Community 2026） | ✅ 构建 + `ctest` **9/9**（同上跳过项） |
| CMake 4.4.0 / Ninja 1.13.2 / ccache 4.13.6 / Catch2 3.15.2 | ✅ 已安装（`pacman`） |
| Qt 6.11.1 / gRPC 1.82.0 / protobuf 35.1 / Eigen3 / benchmark | ✅ 已安装并完成集成 smoke（find_package + 链接 + 运行） |
| Python（UCRT64 3.14.6，含 `jsonschema`/`grpcio`/`yaml`） | ✅ schema/能力/文档测试与 Python 边车均用它 |
| vcpkg | ⚠️ 未安装（`VCPKG_ROOT` 未设置）；`win-vcpkg-*` 预设待启用 |
| bgfx / CSPICE / Pinocchio / MCAP | ⚠️ MSYS2 无现成包，按 superbuild 自建（见 05 §5） |
| Linux 构建 | ⏳ 由 CI `linux-gcc` job 验证（本地无 WSL/容器）；边车桥接在 Linux 默认关闭（需带 CMake config 的 gRPC） |
| CI | ✅ 三个 job 已落地；Windows 与文档 job 已通过，Linux 修复中 |
| Agent 配置（AGENTS.md + workflows/skills） | ✅ 已启用（两条工作流 + 三个技能） |

---

## 9. 约定

- 文档与设计讨论用中文；代码、标识符、提交信息用英文。
- 契约（`api/`）是唯一真源；CI 校验 JSON/YAML 可解析、manifest 与能力消息样例符合 schema、Markdown 链接可达（`ctest docs_check`）。
- 所有插件必须声明能力版本与 ABI 版本；破坏性变更提升主版本。
- 仿真线程内禁用墙钟与无序并行；确定性缺陷按 P0 处理。
- 提交前：`clang-format`、`ctest`、`srsp-conformance`（有插件改动时）。

---

## 10. 下一步

M0 已基本收尾（契约、内核骨架、双传输、CLI、CI）。接下来按 [07 路线图](docs/07-roadmap-risks.md) 进入 **M1**：

1. `renderer.bgfx` 最小渲染 + 双原点；
2. `srsp-studio`（场景树/属性/时间轴/视口）；
3. MCAP 录制与回放（`state_hash` 一致）；
4. 空间环境（星历/光照/eclipse）与 `dynamics.rigid` 对照 Pinocchio；
5. 补审计遗留项 D3（selection pin/score）与 D4（`Diagnostic.remedy`）。
