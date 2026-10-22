# 优化方案：多语言混合实现与多工具链开发环境

- 日期：2026-10-22
- 状态：实施中（D-7～D-12 已于 2026-10-22 按建议倾向关闭，用户指令"按照规划进行执行"）
- 提出人：用户
- 关联文档：[README](../../../README.md)、[01 总体架构](../../../docs/01-architecture.md)、[02 插件契约](../../../docs/02-plugin-contract.md)、[04 技术栈](../../../docs/04-subsystems-and-stack.md)、[05 MSYS2 环境](../../../docs/05-msys2-dev-environment.md)、[06 打包](../../../docs/06-packaging.md)、[07 路线图](../../../docs/07-roadmap-risks.md)、[设计文档评审](2026-10-22-design-review.md)、[M0 执行计划](../../../deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md)

## 1. 背景与目标

当前基线的两条约束：

- **A01**：内核 C++20，插件契约为纯 C ABI（进程内）；非 C/C++ 只能走边车；
- **A09**：Windows 开发环境 = MSYS2 UCRT64 单工具链。

用户提出的两项新需求：

1. **实现语言不只局限于 C++**：平台自身组件也允许多语言混合实现（不只是第三方插件）；
2. **开发环境不只局限于 MSYS2**：允许联合其他开发环境/工具链共同构建。

目标：把「语言自由」从插件层扩展为平台级实现策略，把「单工具链」升级为有主次之分的工具链矩阵，**且不破坏现有契约先行与确定性原则**。本方案给出政策、契约增补、工具链矩阵、排期与决策项。

### 关键事实（方案的技术支点）

- 纯 C ABI 在 **MSVC 与 MinGW 之间可混载**（x64 调用约定一致），前提是：只用纯 C 类型、禁止 bitfield、结构体显式对齐、枚举固定底层类型——现有 02 §4.1 约束基本覆盖，契约冻结时补「跨编译器布局 static_assert」即可闭环；
- `host->alloc/free` 内存所有权规则（02 §4.1）**已经解决**不同 CRT 之间堆不兼容的问题——这是现有设计被低估的一个支点，多工具链方案直接受益；
- 边车（gRPC + 共享内存）天然跨 ABI/跨工具链/跨语言——工具链越多，边车路线的权重应该越高；
- 当前内核只有「插件被调用」方向的 ABI，**没有「内核被调用」方向的 API**——多语言混合实现（Rust CLI、Python SDK、Web 服务）首先需要补这块。

## 2. 方案要点

### 2.1 多语言混合实现政策

**原则**

1. **边界不变**：跨语言只发生在三个既定边界——进程内 C ABI、gRPC/边车、（新增）宿主侧 C API；组件内部语言自选，边界契约不随语言变化；
2. **一个交付单元一种主语言**：混合只发生在边界两侧，禁止组件内部多语言交织（避免构建与调试复杂度失控）；
3. **官方实现语言上限三种**：C++（性能/ABI）、Python（工具/SDK/测试驱动）、Rust（CLI/基础设施插件，单文件分发）；其余语言仅以边车形式接入（MATLAB/C#/Java/Julia/TS）；
4. **内核核心保持 C++**：调度、总线、FrameGraph、PluginHost 不换语言（ABI 与性能确定性是命脉）。

**官方组件语言矩阵（建议）**

| 组件 | 语言 | 理由 |
|---|---|---|
| libsrsp 内核（runtime/bus/registry/plugin_host） | C++20 | ABI 稳定、性能、与 C ABI 天然贴合 |
| renderer.bgfx / dynamics.rigid / env.spice 等性能插件 | C++（Rust 可做交叉验证副本） | GPU/数值生态 |
| `srsp-cli` | **Rust**（clap + serde） | 单文件分发、无 CRT 分发负担、跨平台一致、JSON/Schema 生态好 |
| `tools/codegen`、`tools/packaging`、校验脚本 | **Python**（替代现有部分 bash/ps1） | 跨平台统一，消除 Windows/bash 双份脚本 |
| `srsp-bridge` + Python SDK | Python | 生态即价值 |
| `srsp-studio` | Qt6/C++（不变）；Web 查看器 TypeScript | 桌面成熟栈 |
| 测试驱动（conformance 场景回放、数值对比） | Python + pyarrow | 分析生态 |
| MATLAB/C#/Java 插件 | 边车（04 §11 矩阵不变） | 许可/运行时隔离 |

**需要新增的契约（本方案唯一的契约级改动）**

| 新增 | 内容 | 动机 |
|---|---|---|
| `api/include/srsp/core_api.h`（宿主侧 C API v0） | `srsp_context_create/destroy`、`load_project`、`start/pause/step`、`get_snapshot`、`subscribe`、`plugin_call`；规则与插件 ABI 同构（`struct_size`、UTF-8 长度前缀、alloc/free 配对） | Rust CLI、Python SDK、Web 服务、C# 工具都通过它调内核；没有它，多语言只能写在插件侧 |
| `api/schema/plugin-manifest.schema.json` 扩展 | `transport.library` 的平台键从 `windows-x86_64` 扩展为 `windows-x86_64-ucrt64` / `windows-x86_64-msvc` 形式（`os-arch-toolchain`），旧键别名兼容 | MSVC 与 MinGW 的 C++ 依赖库不兼容，插件二进制必须按工具链区分，否则会装错 DLL |
| ABI 补充条款 | 禁止 bitfield；枚举固定底层类型（`int32_t`）；新增「跨编译器布局一致性」static_assert 组（MSVC/GCC/Clang 三端 CI 跑） | 支撑混工具链进程内加载 |

**与既有评审项的合流**：O3（`time_tai_ns`）、O6（订阅）、O7（checkpoint 统一）、O8（错误所有权）在设计 `core_api.h` 与 ABI 增补时一并定稿，避免两次改契约。

### 2.2 多工具链开发环境矩阵

**工具链矩阵（主次分明）**

| 平台 | 主工具链（CI 必过、发布用它） | 备选工具链（开发者自选，CI 逐步纳入） | 理由 |
|---|---|---|---|
| Windows | MSYS2 UCRT64 GCC（现状保留） | **MSVC 2022 + vcpkg**；Clang-CL | MSYS2 包生态/离线友好已验证；MSVC 有 Qt 官方包、vcpkg、MuJoCo 预编译、更好的调试器与 PIX/RenderDoc 集成 |
| Linux | Ubuntu 22.04 GCC（容器基线） | Clang（ASan/UBSan 已在预设里） | 与 CI/发布容器一致 |
| Windows 上的 Linux 开发 | — | **WSL2 Ubuntu** | 一台机器双平台验证，免双系统 |
| 复现/发布 | — | **Docker / Dev Container** | 本地 = CI；新人一键；发布构建环境固定 |
| macOS（预留，非 v1 承诺） | Apple Clang | — | bgfx Metal 后端理论可用，1.x 再评估 |

**依赖管理分层（每个工具链一份清单，同一来源生成）**

| 层 | 机制 | 适用 |
|---|---|---|
| 系统包 | pacman（MSYS2）/ apt（Linux）/ **vcpkg manifest 模式（`vcpkg.json`）** | 各自主场 |
| 统一兜底 | superbuild（FetchContent 固定 tag + URL_HASH）：bgfx、CSPICE、MCAP、Pinocchio | 所有工具链 |
| 引导脚本 | `tools/env/bootstrap-<toolchain>.ps1/.sh` 每工具链一份，由同一 `packages` 清单派生 | 新人/CI 共用 |

**CMake 预设扩展（保留现有 5 组，新增）**

| 预设 | 说明 |
|---|---|
| `win-msvc-debug` / `win-msvc-release` | MSVC 2022 + Ninja + vcpkg toolchain 文件 |
| `win-clang-cl-debug` | Clang-CL（复用 MSVC 标准库，接近 MSVC ABI） |
| `wsl-gcc-debug` | WSL2 内的 Linux 构建（与 linux-gcc 同配置，文档专用名） |
| `docker-gcc-release` | 与发布容器一致的构建 |

> 预设继续遵守 AGENTS 规则：构建目录一律 `<model>/Temp/build/<preset>`，不写全局环境。

**CI 矩阵扩展**

| Job | 门禁级别 |
|---|---|
| windows-msys2、linux-gcc | 必修（现状计划） |
| windows-msvc | 先「允许失败」观察 2 个里程碑，M2 起转必修 |
| linux-clang-asan | 必修（内存/UB 防线） |
| docker-release-build | 发布流水线专用 |
| 文档/契约校验（schema、链接、示例可解析） | 必修 |

**关键工程规则**

1. **ABI 不可混链**：MSVC 与 MinGW 产物禁止互相 `target_link_libraries`；跨工具链只许走「纯 C ABI 插件加载」或「边车」；CI 用 `objdump`/`dumpbin` 检查导入表防止混入；
2. **进程内插件的双工具链构建**：官方插件在 Windows 出 `ucrt64` 与 `msvc` 两份二进制（manifest 按新标签选载）；第三方只交一份时平台按其标签拒绝错误宿主并给出解释；
3. **分发差异**：MSVC 产物随包带 `vcruntime140*.dll`/`msvcp140*.dll`（或 vc_redist）；MinGW 产物带 `libstdc++-6.dll` 等（06 文档已有，需按工具链分节）；
4. **边车优先原则强化**：任何「只在一种工具链上能编出来」的组件（如 MuJoCo 预编译 = MSVC），默认做成边车插件而不是进程内插件——这是多工具链时代降低矩阵爆炸的核心手段。

### 2.3 对现有文档的影响清单（采纳后要改的位置）

| 文档 | 改动 |
|---|---|
| 01 | §1.2 G2/G3 措辞升级为「实现多语言 + 工具链矩阵」；ADR 追加 A11（宿主侧 core_api）、A12（Windows 双工具链） |
| 02 | §2 manifest `library` 键扩展工具链标签；§4 ABI 增补（禁 bitfield、枚举底层类型、混载 static_assert）；§6 版本表补「工具链标签不参与语义版本」 |
| 03 | §2.1 对齐/字节序约定补「三编译器 static_assert 验证」；无其他改动 |
| 04 | §10 技术栈总表补 MSVC/vcpkg/Rust；§11 矩阵补「官方实现语言上限三种」 |
| 05 | 保持 MSYS2 专篇，新增「工具链矩阵总览」一节并链接新预设；§6 仓库结构补 `sdk/rust/`、`api/include/srsp/core_api.h` |
| 06 | §2 布局、§3 依赖收集按工具链分节（MSVC redist vs MinGW runtime）；§5 `.srpkg` 的 `bin/` 目录按 `os-arch-toolchain` 分层 |
| 07 | CI 矩阵更新；风险新增 R13（多工具链维护成本）与 R14（ABI 混淆混入）；M0/M1 范围相应微调 |
| README | §2 关键决策追加 2 条；§7 快速开始按工具链分节 |
| M0 执行计划 | 新增工作流：T1.6（core_api.h v0 + 混载 static_assert）、T6.4（win-msvc 预设与 vcpkg 清单）；O13 排期基线重估 |

## 3. 交付物与验收

| 交付物 | 验收 |
|---|---|
| 本方案（评审通过） | 决策项 D-7～D-12 全部关闭 |
| `core_api.h` v0 + 增补条款 | 三编译器（GCC/MSVC/Clang）布局 static_assert 全过；混载冒烟：MSVC 编译的插件被 MinGW 宿主加载（或反之）成功调用 `core.describe` |
| manifest schema 扩展 | 新旧清单均校验通过；错误工具链插件被拒并给出可读解释 |
| 预设矩阵 | `win-ucrt64-*`、`win-msvc-*`、`linux-gcc-*` 在本机/容器配置并构建 hello 成功 |
| CI 矩阵 | msys2/linux-gcc 必修绿；msvc job 跑通并允许失败转必修有记录 |
| 文档更新 | 上表 2.3 全部落笔；CI 校验链接/schema 通过 |

## 4. 执行步骤（与 M0 对齐，分三步走）

**阶段 A（M0 内，与契约冻结同期）**
1. D-7～D-12 评审关闭；
2. ABI 增补条款 + 三编译器 static_assert 组（归入 M0 T1.3）；
3. manifest schema 工具链标签扩展 + 样例（归入 M0 T1.2）；
4. `core_api.h` v0 设计（只定义上下文/加载/步进/快照/订阅，CLI 够用即可）；
5. `win-msvc-debug` 预设 + `vcpkg.json` 清单（只声明 qt6-base、protobuf、grpc、catch2 等 M0 需要项）。

**阶段 B（M1）**
6. `srsp-cli` Rust 实现试点（与 C++ 版并行一个里程碑，对比后只保留一个）；
7. Dev Container（与 CI linux-gcc 共用 Dockerfile）；
8. CI 增加 windows-msvc job（允许失败）；
9. 官方插件双工具链构建与 `.srpkg` 分层打包验证。

**阶段 C（M2+）**
10. Python SDK 切到 `core_api`（替代直连边车以外的路径）；Rust 插件模板 `srsp-plugin-template-rust`；macOS 评估记录。

## 5. 风险与待确认

| 项 | 说明 | 应对 |
|---|---|---|
| R13 多工具链维护成本 | 每加一个工具链，CI/打包/排错都要加一份 | 主/备分级；备选工具链转必修有门槛；边车优先原则 |
| R14 ABI 混淆 | 开发者误把 MSVC 库链给 MinGW 目标 | CI 导入表检查；CMake 预设隔离输出目录；文档红字 |
| Rust 引入成本 | 团队 Rust 熟练度、cargo 与 CMake 集成（corrosion） | 只在 CLI/工具试点；不复用内核代码；失败则退回 C++ CLI |
| MuJoCo 等 MSVC-only 依赖 | 与 MSYS2 主工具链冲突 | 做成边车（gRPC 跨 ABI），不硬刚进程内 |
| 与 O 系列评审的关系 | O3/O6/O7/O8 需与 core_api 同步设计 | 一次契约修订完成，不二次冻结 |

## 6. 决策项（需评审关闭）

| 编号 | 决策 | 建议倾向 | 影响 |
|---|---|---|---|
| D-7 | 内核是否新增宿主侧 `core_api.h` | 新增（多语言的前置条件） | api/、CLI、SDK、Web |
| D-8 | `srsp-cli` 语言 | Rust（失败回退 C++） | 构建链 +cargo/corrosion |
| D-9 | Windows 第二工具链 | MSVC 2022 + vcpkg manifest | CI、打包、依赖清单 |
| D-10 | 是否引入 Dev Container | M1 引入，与 CI 共用 Dockerfile | 新人上手、发布复现 |
| D-11 | 插件包平台标签 | `os-arch-toolchain`（旧键别名兼容） | 02/06 契约与打包 |
| D-12 | 官方实现语言上限 | C++/Python/Rust 三种，其余仅边车 | 团队技能与维护面 |

## 7. 执行记录

| 日期 | 动作 | 结果 |
|---|---|---|
| 2026-10-22 | 依据用户两条新需求（多语言实现、多工具链环境）制定方案 | 待评审 |
| 2026-10-22 | 用户指令"按照规划进行执行"，D-7～D-12 按建议倾向关闭，阶段 A 开工 | 环境闭环：pacman 安装 CMake 4.4.0 / Ninja 1.13.2 / ccache 4.13.6 / Catch2 3.15.2；`win-ucrt64-debug` 构建 + ctest 通过 |
| 2026-10-22 | 阶段 A 第 2 项：ABI 增补条款 + 布局 static_assert | `plugin_api.h` 增补（禁位域/枚举 32 位/跨编译器布局 + `time_tai_ns`）；`tests/abi/layout_static_assert.cpp` 建立，GCC 16.1 与 MSVC 19.50 双端通过 |
| 2026-10-22 | 阶段 A 第 3 项：manifest schema 扩展 | `plugin-manifest.schema.json` 支持 `os-arch-toolchain` 标签（旧键别名兼容）；`api/schema/examples/` 10 样例（6 正 4 反）经 jsonschema 校验全过并接入 ctest |
| 2026-10-22 | 阶段 A 第 4 项：`core_api.h` v0 | 新增 `api/include/srsp/core_api.h`（draft 0.1：context/项目/时钟/快照/订阅/插件透传），布局断言并入 ABI 测试 |
| 2026-10-22 | 阶段 A 第 5 项：MSVC 预设 + vcpkg 清单 | `CMakePresets.json` 新增 `win-msvc-*`（无依赖，VS 2026 实测构建/测试通过）与 `win-vcpkg-*`（待 VCPKG_ROOT）；`vcpkg.json` 建立 |
| 2026-10-22 | 跨工具链混载冒烟 | 插件静态 CRT 后：GCC 宿主 ↔ MSVC 插件、MSVC 宿主 ↔ GCC 插件双向加载调用成功（exit 0） |
| 2026-10-22 | 文档同步 | 02（manifest 标签/ABI 增补/冻结清单 2 项勾销）、01（ADR A11/A12）、04（备选工具链行）、05（本机基线/预设表/MSVC 使用注意）、06（工具链分层打包）、07（R13/R14）、README（决策 10/11、结构、环境状态） |

## 8. 修订记录

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-10-22 | V1 | 初稿：语言矩阵、core_api 增补、工具链矩阵、CI 扩展、6 项决策 |
| 2026-10-22 | V2 | 记录阶段 A 执行结果：5 项任务全部完成并构建验证；D-7～D-12 按建议关闭；补跨工具链混载冒烟结果
