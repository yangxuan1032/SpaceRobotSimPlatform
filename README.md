# SRSP — 空间机器人可视化仿真平台

面向在轨服务、目标捕获、舱外装配与遥操作训练的空间机器人可视化仿真软件。
采用 **平台 + 插件** 架构：机器人模型、动力学、空间环境、传感器、渲染与控制器以**版本化契约**协作，可独立分发与替换。

> **当前状态（2026-11-11）**：架构基线 v0.1；`ctest` **79/79**（Windows UCRT64 已验证）。
> 组件化实施 W0–W9 核心已完成：组件清单与装配、多实体 World（原子快照）、天体环境与轨道、摄动力与姿态、推进/质量时变、传感器调度与相机、发布门禁。
> 规划与追溯见 [12 组件标准](docs/12-component-standard.md) 与 [01 §10](docs/01-architecture.md)。

---

## 1. 需求与方案对照

| 需求 | 方案落点 | 详见 |
|---|---|---|
| 平台 + 插件，模型/动力学/环境/可视化可替换 | 内核 + 能力注册表；场景只依赖能力不依赖插件 ID | [01](docs/01-architecture.md)、[02](docs/02-plugin-contract.md) |
| 标准数据与调用接口 | C ABI、Protobuf 控制面、canonical raw-f64 状态面、glTF、SRM、MCAP | [02](docs/02-plugin-contract.md)、[03](docs/03-data-contracts.md) |
| 组件标准化与可扩展 | 组件五要素、11 类清单、七条扩展规则、CLI 校验 | [12](docs/12-component-standard.md) |
| 多实体与装配 | `MountSpec` 有向图 → 规范 SRM 树；零质量传感器不改 `nv` | [12 §8](docs/12-component-standard.md) |
| 统一数据面 | Snapshot / Command / Subscription / Record 四契约 | [03](docs/03-data-contracts.md) |
| 轨道、摄动与姿态 | 环境采样（引力/J2/三体/大气/SRP/遮挡）+ RK4 + 姿态与轮 | [03](docs/03-data-contracts.md) |
| 传感器与交互 | 相机成像、LiDAR 环扫、雷达检测、轨道相机与拾取 | [03](docs/03-data-contracts.md) |
| 跨语言插件 | 进程内 C ABI；Python/C#/Java/MATLAB 走 gRPC 边车 | [02](docs/02-plugin-contract.md)、[04 §11](docs/04-subsystems-and-stack.md) |
| MSYS2 + VSCode 开发 | UCRT64 + CMake Presets + Ninja；任务、快捷键与可视化检查 | [05](docs/05-msys2-dev-environment.md)、[10](docs/10-vscode-workflow.md) |
| 可打包、Windows/Linux | 可重定位 runtime、`.srpkg`、安装器与发布门禁 | [06](docs/06-packaging.md) |

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
│ 世界层：EntityStore · AssemblyGraph · Environment · Dispatcher  │
├───────────────────────────────────────────────────────────────┤
│ 领域层：dynamics · sensors · render · record · model            │
├───────────────────────────────────────────────────────────────┤
│ 契约层：C ABI · Protobuf · JSON Schema · SRM · glTF · MCAP      │
└───────────────────────────────────────────────────────────────┘
```

- 一次宏步：命令入队 → 控制器 → 力模型（引力/J2/三体/SRP/阻力/推力）→ RK4 位置与姿态 → 原子提交快照。
- 快照带 16 字节头（`@SRSP` + 版本 + 步长 + 实体数），每实体含位置/速度/质量/姿态/角速度。
- 渲染 capture 只含 3D 场景，UI 叠加由窗口合成（`capture.hpp` 契约，`unit.capture_scope` 守护）。

### 3.1 组件模型

> 组件 = 清单 + 类型化参数 schema + 生命周期 + 数据契约 + 能力实现。

| 维度 | 内容 |
|---|---|
| 身份 | `ComponentKind` / `ComponentId` / `entity/<name>` / `Capability@major` |
| 清单 | `component.json`：kind、id、version、capabilities、params、mount、data |
| 装配 | `MountSpec` 图校验（宿主/环/frame）→ 合并为规范 SRM 树，id 加实例前缀 |
| 扩展 | 新类型/实例/后端/力模型/参数均不碰内核；保留档必须显式失败 |

完整规范见 [12 组件标准](docs/12-component-standard.md)。

### 3.2 世界与数据面

| 契约 | 方向 | 实现 |
|---|---|---|
| Snapshot | 读 | `World::acquire_latest()`，原子提交、generation 单调 |
| Command | 写 | `WorldCommand`（速度增量 / 推力 / 轮力矩），步首统一应用 |
| Subscription | 推 | `World::on_commit` 回调，每步一次 |
| Record | 存 | `srsp-cli world --record`，可被 `inspect` 准入 |

---

## 4. 文档地图

| 文档 | 内容 |
|---|---|
| [01 总体架构](docs/01-architecture.md) | 边界、分层、进程/线程、调度、确定性、质量指标、组件化目标架构 |
| [02 插件体系](docs/02-plugin-contract.md) | 清单、生命周期、C ABI 规则、能力方法、版本兼容、边车、组件/能力/插件分层 |
| [03 数据契约](docs/03-data-contracts.md) | 单位/坐标/时间、SRM、状态面、话题、场景协议、MCAP、数据面四契约、环境/姿态/传感器实现 |
| [04 子系统与选型](docs/04-subsystems-and-stack.md) | 动力学、力/接触、环境、传感器、渲染、脚本、技术栈对比 |
| [05 MSYS2 环境](docs/05-msys2-dev-environment.md) | 安装、环境变量、Presets、CI、排错 |
| [06 打包交付](docs/06-packaging.md) | 产物矩阵、DLL 收集、安装器、`.srpkg`、**发布门禁** |
| [07 路线图与风险](docs/07-roadmap-risks.md) | 里程碑、出口标准、风险登记、质量门禁、B 系列待办 |
| [08 插件数据接口](docs/08-plugin-data-interface.md) | 通道、线格式、BlobRef、错误约定 |
| [09 代码规范](docs/09-code-standards.md) | 命名、注释、格式、测试、提交 |
| [10 VSCode 工作流](docs/10-vscode-workflow.md) | 任务、快捷键、可视化检查、快照冒烟 |
| [11 模块总览](docs/11-module-overview.md) | 分层、模块职责、数据流、测试覆盖、能力矩阵、组件清单 |
| [12 组件标准](docs/12-component-standard.md) | 组件五要素、11 类清单、清单文件、生命周期、扩展规则、装配 |

---

## 5. 仓库结构

```
SpaceRobotSimPlatform/
├─ AGENTS.md                     # 模型无关的 Agent 契约
├─ ai-models.json                # 模型命名空间注册表
├─ VisualCheck.cmd               # 双击式可视化检查
├─ CMakeLists.txt / CMakePresets.json
├─ api/
│  ├─ include/srsp/plugin_api.h  # 稳定 C ABI（host 表 v1.1）
│  ├─ include/srsp/core_api.h    # 宿主侧 C API
│  ├─ proto/srsp/v1/plugin.proto
│  └─ schema/                    # manifest 与能力消息 schema
├─ src/
│  ├─ core/                      # kernel + C API；world/environment/sensing
│  ├─ model/                     # SRM、URDF/MJCF 导入、组件清单、装配编译
│  ├─ dynamics/                  # 浮基多体、CRBA、积分器、力模型函数
│  ├─ render/                    # Scene、mesh、文字图集、相机成像、Qt/RHI
│  ├─ record/                    # MCAP、lock、replay 校验
│  └─ bridge/                    # gRPC 边车
├─ apps/
│  ├─ cli/                       # version / component validate / world / soak / lock / inspect
│  ├─ studio/                    # Qt + RHI，--snapshot，鼠标相机与拾取
│  └─ sidecar/
├─ tools/{ai,visual,env,sidecar,packaging}
├─ examples/{hello_plugin,python_plugin,components,models}
├─ tests/{abi,unit,integration,docs}
├─ bench/                        # 动力学、插件调用、录制、RHI、World
├─ tools/ci/release_gate.ps1           # 发布门禁
├─ docs/                         # 01–12
└─ .vscode/                      # 任务、快捷键
```

---

## 6. Agent 工作区约定（多模型）

- **命名空间**：`<model>/`，唯一真源 [`ai-models.json`](ai-models.json)。
- **全部模型目录不入库**：`<model>/`（`Temp/`、`Logs/`、README、设计文档）由 `.gitignore` 整体忽略；版本化资产放 `tools/ai/`、`docs/`、`api/`、`tests/`、`apps/`。
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

### 7.2 常用命令

| 命令 | 用途 |
|---|---|
| `srsp-cli component validate examples/components/earth.component.json` | 校验组件清单 |
| `srsp-cli world --steps 100 --record state.mcap` | 多实体世界步进 + 录制 |
| `srsp-cli inspect state.mcap` | 查看录制通道与元数据 |
| `srsp-cli soak --plugin <dll> --workload step --minutes 1` | 插件压力 |
| `srsp-studio --snapshot out.png --renderer rhi` | 离屏快照 |
| `srsp-studio examples/models/arm6.srm.json` | 交互式查看（鼠标旋转/缩放/拾取） |
| `ctest -L visual` | 可视化冒烟（Qt 必跑，RHI 无 GPU 跳过） |
| `pwsh -File tools/ci/release_gate.ps1` | 发布门禁（测试/文档/格式；性能门需 Release） |

### 7.3 可视化检查

```powershell
# 三种入口等效：双击 VisualCheck.cmd / VSCode 任务 / Ctrl+Alt+V
.\VisualCheck.cmd
```

### 7.4 Linux（Ubuntu 22.04+）

```bash
sudo apt install g++-14 cmake ninja-build ccache catch2 qt6-base-dev nlohmann-json3-dev
cmake --preset linux-gcc-debug && cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

---

## 8. 环境验证状态

| 项 | 状态 |
|---|---|
| GCC（MSYS2 UCRT64，Debug） | ✅ **唯一已验证基线**：构建 + `ctest` **79/79** |
| 可视化后端 | ✅ Qt（软件）与 RHI（D3D11）均实测出图；无 GPU 时 RHI 快照 SKIP |
| 性能基准 | ⚠️ 必须用 **Release** 预设；Debug 下 `bench_dynamics` 约 24.5 ms，不作判定 |
| Clang / CLANG64 / MSVC / Linux | ⚠️ 本轮未验证 —— 不宣称通过 |
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

组件化实施已过 W0–W9 核心，剩余工作：

1. **打包与发布**：`.srpkg` 流程、Inno Setup/AppImage、发布门禁接入 CI；
2. **W4.5 收尾**：天体作为运行时组件实例注册（当前为清单 + 解析函数）；
3. **Studio 接线**：把 World 接入交互主流程（当前 `srsp-cli world` 为主）；
4. **跨平台**：Clang / MSVC / Linux 验证与门禁；
5. **触发型后置**：B1 心跳、B6 ABA、B7 Data Bus、B8 签名（按 `docs/07 §18` 触发条件）。

---

## 11. 许可证

本项目采用 **MIT License**，全文见 [`LICENSE`](LICENSE)。

```
Copyright (c) 2026 SRSP contributors
```

允许自由使用、修改、分发与再许可，需保留版权与许可声明；软件按「原样」提供，不附带任何明示或暗示担保。
