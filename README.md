# SRSP — 空间机器人可视化仿真平台

面向在轨服务、目标捕获、舱外装配与遥操作训练的空间机器人可视化仿真软件。
采用 **平台 + 插件** 架构：机器人模型、动力学、空间环境、传感器、渲染与控制器以**版本化契约**协作，可独立分发与替换。

> **当前状态（2026-11-09）**：架构基线 v0.1，`M0` 完成；`ctest` **67/67**（Windows UCRT64 已验证）。
> 已实现：C ABI 与宿主 C API、内核（插件宿主/时钟/调度/诊断/订阅）、`srsp-cli`（双传输）、`srsp-studio`（Qt 与 RHI 双后端）、浮基多体动力学（CRBA + RK4/半隐式）、SRM/URDF/MJCF 导入、MCAP 录制与 lock/replay 校验、gRPC 边车、可视化一键检查。
> 目标形态与实施路线见 [12 组件标准](docs/12-component-standard.md) 与 [01 §10](docs/01-architecture.md)。

---

## 1. 需求与方案对照

| 需求 | 方案落点 | 详见 |
|---|---|---|
| 平台 + 插件，模型/动力学/环境/可视化可替换 | 内核 + 能力注册表；场景只依赖能力不依赖插件 ID | [01](docs/01-architecture.md)、[02](docs/02-plugin-contract.md) |
| 标准数据与调用接口 | C ABI、Protobuf 控制面、canonical raw-f64 状态面、glTF 场景、SRM 模型、MCAP 录制 | [02](docs/02-plugin-contract.md)、[03](docs/03-data-contracts.md) |
| 组件标准化与可扩展 | 组件五要素、11 类清单、七条扩展规则 | [12](docs/12-component-standard.md) |
| 多实体与装配 | `MountSpec` 有向图 → 规范 SRM 树；零质量传感器不改 `nv` | [12 §4](docs/12-component-standard.md)、[01 §10](docs/01-architecture.md) |
| 统一数据面 | Snapshot / Command / Subscription / Record 四契约 | [03](docs/03-data-contracts.md)、[01 §10](docs/01-architecture.md) |
| 跨语言插件 | 进程内 C ABI；Python/C#/Java/MATLAB/Julia 走 gRPC 边车 | [02](docs/02-plugin-contract.md)、[04 §11](docs/04-subsystems-and-stack.md) |
| MSYS2 + VSCode 开发 | UCRT64 + CMake Presets + Ninja；任务、快捷键与可视化检查 | [05](docs/05-msys2-dev-environment.md)、[10](docs/10-vscode-workflow.md) |
| 可打包、Windows/Linux | 可重定位 runtime、`.srpkg`、安装器与冒烟 | [06](docs/06-packaging.md) |

---

## 2. 关键决策

| # | 决策 | 理由 |
|---|---|---|
| 1 | 内核 C++20，插件契约为**纯 C ABI** | 跨语言 FFI 成本最低，ABI 可控可版本化 |
| 2 | 控制面 Protobuf，1.x 状态面 **canonical raw-f64**（Arrow 为 2.0 预留） | 小消息用 schema，大数组走零拷贝预留 |
| 3 | **进程内 + 边车** 双传输，语义一致 | 兼顾性能与语言自由/故障隔离 |
| 4 | 自定义模型 IR **SRM**，URDF/SDF/MJCF 仅作导入 | 空间机器人需要浮基、轮/CMG、推力器 |
| 5 | 渲染后端为策略：Qt（软件）+ QRhi/D3D11；Null 用于 headless | 已验证的双后端；其它后端可插拔 |
| 6 | Python **不内嵌**，作为边车 | 规避 MinGW/MSVC ABI 与 GIL 风险 |
| 7 | 录制用 **MCAP** | 自描述、可索引、跨语言 |
| 8 | Windows 用 **MSYS2 UCRT64** 原生构建 | 满足约束、产物为原生 Win64 |
| 9 | v1 单进程，接口按可跨进程设计 | 先交付，保留拆分能力 |
| 10 | 对外暴露宿主侧 C API（`core_api.h`） | 非 C++ 应用可嵌入内核 |
| 11 | 新增能力默认关闭，legacy 路径逐位不变 | 保护既有 golden 与回放 |

---

## 3. 架构总览

```
┌───────────────────────────────────────────────────────────────┐
│ 应用：srsp-studio (Qt/RHI) · srsp-cli · Python SDK · 边车客户端 │
├───────────────────────────────────────────────────────────────┤
│ 内核 libsrsp：调度 · 注册表 · 快照/命令 · 时间 · 插件宿主 · 诊断 │
├──────────────────────────┬────────────────────────────────────┤
│ 进程内插件（C ABI）       │ 边车插件（gRPC + 共享内存）         │
│ 动力学 / 控制器 / 记录     │ Python 控制器 / 外部可视化 / JVM    │
├──────────────────────────┴────────────────────────────────────┤
│ 契约层：C ABI · Protobuf · JSON Schema · SRM · glTF · MCAP      │
└───────────────────────────────────────────────────────────────┘
```

- 一次宏步：收集命令 → 控制器 → 力模型累加 → 动力学推进 → 原子提交快照 → 订阅/录制/渲染消费。
- 浮基多体（`nv = 6 + joints`）、CRBA 质量矩阵、RK4（默认）/半隐式；轮、推力器、燃料与空间环境为规划能力。
- 轨道尺度渲染采用**双原点 + 反转 Z**，解决 float32 在 10⁷ m 量级的抖动。
- 渲染 capture 只含 3D 场景，UI 叠加由窗口合成（`capture.hpp` 契约，`unit.capture_scope` 守护）。

### 3.1 组件模型（摘要）

> 组件 = 清单 + 类型化参数 schema + 生命周期 + 数据契约 + 能力实现。

| 维度 | 内容 |
|---|---|
| 身份 | `ComponentKind` / `ComponentId` / `entity/<name>` / `Capability@major` |
| 清单 | `component.json`：kind、id、version、capabilities、params、mount、data |
| 生命周期 | declare → validate → resolve → instantiate → init → tick/sample → checkpoint → shutdown |
| 扩展 | 新类型/实例/后端/力模型/参数/契约均不碰内核；保留档必须显式失败 |

完整规范见 [12 组件标准](docs/12-component-standard.md)。

---

## 4. 文档地图

| 文档 | 内容 |
|---|---|
| [01 总体架构](docs/01-architecture.md) | 边界、分层、进程/线程、调度、确定性、质量指标、§10 组件化目标架构 |
| [02 插件体系](docs/02-plugin-contract.md) | 清单、生命周期、C ABI 规则、能力方法、版本兼容、边车、组件/能力/插件分层 |
| [03 数据契约](docs/03-data-contracts.md) | 单位/坐标/时间、SRM、状态面、话题、场景协议、MCAP、数据面四契约 |
| [04 子系统与选型](docs/04-subsystems-and-stack.md) | 动力学、力/接触、环境、传感器、渲染、脚本、技术栈对比 |
| [05 MSYS2 环境](docs/05-msys2-dev-environment.md) | 安装、环境变量、Presets、CI、排错 |
| [06 打包交付](docs/06-packaging.md) | 产物矩阵、DLL 收集、安装器、`.srpkg`、冒烟 |
| [07 路线图与风险](docs/07-roadmap-risks.md) | 里程碑、出口标准、风险登记、质量门禁、B 系列待办 |
| [08 插件数据接口](docs/08-plugin-data-interface.md) | 通道、线格式、BlobRef、错误约定 |
| [09 代码规范](docs/09-code-standards.md) | 命名、注释、格式、测试、提交 |
| [10 VSCode 工作流](docs/10-vscode-workflow.md) | 任务、快捷键、可视化检查、快照冒烟 |
| [11 模块总览](docs/11-module-overview.md) | 分层、模块职责、数据流、测试覆盖、能力矩阵、组件清单 |
| [12 组件标准](docs/12-component-standard.md) | **组件五要素、11 类清单、清单文件、生命周期、扩展规则、目录约定** |

---

## 5. 仓库结构

```
SpaceRobotSimPlatform/
├─ AGENTS.md                     # 模型无关的 Agent 契约
├─ ai-models.json                # 模型命名空间注册表
├─ VisualCheck.cmd               # 双击式可视化检查
├─ CMakeLists.txt / CMakePresets.json
├─ api/
│  ├─ include/srsp/plugin_api.h  # 稳定 C ABI（v1.1：host 表追加订阅）
│  ├─ include/srsp/core_api.h    # 宿主侧 C API
│  ├─ proto/srsp/v1/plugin.proto
│  └─ schema/                    # manifest 与能力消息 schema
├─ src/
│  ├─ core/                      # srsp_kernel + srsp_core（C API 共享库）
│  ├─ model/                     # SRM + URDF/MJCF 导入
│  ├─ dynamics/                  # 浮基多体、CRBA、积分器、力模型函数
│  ├─ render/                    # Scene、mesh、文字图集、Qt/RHI 后端
│  ├─ record/                    # MCAP、lock、replay 校验
│  └─ bridge/                    # gRPC 边车
├─ apps/
│  ├─ cli/                       # srsp-cli（soak/lock/inspect/replay-check）
│  ├─ studio/                    # srsp-studio（Qt + RHI，--snapshot）
│  └─ sidecar/                   # srsp-sidecar
├─ tools/
│  ├─ ai/                        # 工作流、技能、脚本、模板
│  ├─ visual/                    # VisualCheck.ps1
│  ├─ env/  sidecar/  packaging/
├─ examples/{hello_plugin,python_plugin,models}
├─ tests/{abi,unit,integration,docs}
├─ bench/                        # 动力学、插件调用、RHI
├─ docs/                         # 01–12
└─ .vscode/                      # 任务、快捷键
```

---

## 6. Agent 工作区约定（多模型）

- **命名空间**：`<model>/`，唯一真源 [`ai-models.json`](ai-models.json)。
- **全部模型目录不入库**：`<model>/`（含 `Temp/`、`Logs/`、README、设计文档）由 `.gitignore` 整体忽略；需要版本化的资产放 `tools/ai/`、`docs/`、`api/`、`tests/`、`apps/`。
- **缓存**：`<model>/Temp/`；构建目录 `<model>/Temp/build/<preset>`。
- **计划与会话记录**：`<model>/Logs/Plan/`、`<model>/Logs/Log/`（本地保存）。
- **提交与推送**：先执行后征询，未获许可不得 commit/push。

规则全文见 [AGENTS.md](AGENTS.md)。

---

## 7. 快速开始

### 7.1 Windows（MSYS2 UCRT64）

```powershell
# 依赖（首次）
C:\msys64\usr\bin\bash -lc "pacman -S --needed mingw-w64-ucrt-x86_64-toolchain cmake ninja ccache catch2 qt6-base nlohmann-json"

# 每个新 shell：设置模型命名空间与缓存目录
Set-ExecutionPolicy -Scope Process Bypass -Force
. .\tools\ai\Enter-AgentEnv.ps1 -Model deepseek

cmake --preset win-ucrt64-debug
cmake --build --preset win-ucrt64-debug
ctest --preset win-ucrt64-debug
```

### 7.2 可视化检查

```powershell
# 三种入口等效：双击 VisualCheck.cmd / VSCode 任务 / Ctrl+Alt+V
.\VisualCheck.cmd
```

生成 `<model>/Temp/visual/<时间戳>/{qt,rhi}.png` 并打开；`ctest -L visual` 在 CI 跑无 GPU 的冒烟。

### 7.3 Linux（Ubuntu 22.04+）

```bash
sudo apt install g++-14 cmake ninja-build ccache catch2 qt6-base-dev nlohmann-json3-dev
cmake --preset linux-gcc-debug && cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

### 7.4 常用命令

| 命令 | 用途 |
|---|---|
| `srsp-cli soak --plugin <dll> --workload step --minutes 1` | 插件压力/稳定性 |
| `srsp-cli soak --workload record --minutes 1` + `lock`/`inspect`/`replay-check` | 录制与回放校验 |
| `srsp-studio --snapshot out.png --renderer rhi` | 离屏快照 |
| `srsp-studio examples/models/arm6.srm.json` | 交互式查看 |
| `bench_dynamics` / `bench_plugin_call` | 性能基准 |

---

## 8. 环境验证状态

| 项 | 状态 |
|---|---|
| GCC（MSYS2 UCRT64，Debug） | ✅ **唯一已验证基线**：构建 + `ctest` **67/67** |
| 可视化后端 | ✅ Qt（软件）与 RHI（D3D11）均实测出图；无 GPU 时 RHI 快照 SKIP |
| Clang / CLANG64 / MSVC / Linux | ⚠️ 本轮未验证 —— 不宣称通过 |
| Qt 6 / nlohmann-json / Catch2 / gRPC | ✅ 已安装并集成 |
| 文档与格式门 | ✅ `docs_check`、`format_check` 通过 |

> 规则：只有实际跑过的组合才能标 ✅；未验证的写 ⚠️ 并说明原因。

---

## 9. 约定

- 文档与讨论用中文；代码、标识符、提交信息用英文。
- 契约（`api/`）是唯一真源；CI 校验 JSON/YAML 可解析、Markdown 链接可达（`ctest docs_check`）。
- 所有插件声明能力版本与 ABI 版本；破坏性变更提升主版本。
- 仿真线程内禁用墙钟与无序并行；确定性缺陷按 P0 处理。
- 提交前：`clang-format`、`ctest`、必要时 `docs_check`。

---

## 10. 下一步

当前按「组件化目标架构」推进，工作包 W0–W9：

1. **W0** 基线与追溯矩阵；
2. **W1** 组件身份与参数校验（`component.json`、`entity/<name>`、覆盖优先级）；
3. **W2** 装配编译（`MountSpec` → 规范 SRM 树）；
4. **W3** World 与四契约数据面；
5. W4–W8 依次为环境/轨道、摄动与光照、质量/执行器/控制、传感器、视图与交互；
6. **W9** 性能与交付。

里程碑与出口标准见 [07 路线图](docs/07-roadmap-risks.md)，组件规范见 [12](docs/12-component-standard.md)。

---

## 11. 许可证

本项目采用 **MIT License**，全文见 [`LICENSE`](LICENSE)。

```
Copyright (c) 2026 SRSP contributors
```

允许自由使用、修改、分发与再许可，需保留版权与许可声明；软件按「原样」提供，不附带任何明示或暗示担保。
