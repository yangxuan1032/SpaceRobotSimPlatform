# M0 执行计划：契约冻结与骨架闭环

- 日期：2026-10-22
- 状态：实施中（环境与契约部分已完成，内核/CLI/双传输/CI 待建；2026-10-23 回填）
- 提出人：用户
- 关联文档：[README](../../../README.md)、[01 总体架构](../../../docs/01-architecture.md)、[02 插件契约](../../../docs/02-plugin-contract.md)、[03 数据契约](../../../docs/03-data-contracts.md)、[04 技术栈](../../../docs/04-subsystems-and-stack.md)、[05 MSYS2 环境](../../../docs/05-msys2-dev-environment.md)、[06 打包](../../../docs/06-packaging.md)、[07 路线图](../../../docs/07-roadmap-risks.md)、[一致性审计](2026-10-24-plan-vs-architecture-audit.md)（原「架构与技术栈规划」已按计划治理策略移出版本库，可在 git 历史中查看）

## 1. 背景与目标

`docs/07` §3 把 M0 定义为「契约与走通骨架」。截至 **2026-10-23**，环境与契约两层已经落地，差距集中在实现层：

- ✅ **环境**：CMake 4.4.0 / Ninja 1.13.2 / Catch2 / Clang 22.1.8 / Qt 6.11.1 / gRPC 1.82 / protobuf 35.1 / Eigen3 已装；四套工具链（GCC、Clang-UCRT64、Clang-CLANG64、MSVC）各自 `ctest` 3/3；
- ✅ **契约**：ABI 布局断言（`tests/abi`）、manifest 10 样例（`api/schema/examples` + jsonschema 校验）、宿主侧 `core_api.h` v0 已落地；
- ❌ **没有内核**（无 `src/`）：注册表、调度、插件宿主、诊断全部缺失；
- ❌ **没有 `srsp-cli`**：`demo` / `validate` 这些验收命令不存在；
- ❌ **双传输只完成了一半**：C++ 进程内 hello 有，Python 边车仍是「目标形态」示例，不参与构建；
- ❌ **没有 CI**：`.github/` 已随 Agent 入口清理移除。

**目标**：达成 `docs/07` §3 的 M0 出口判据——新成员按 README 从零构建成功（≤30 分钟）、CI 绿（Windows + Linux）、双传输 demo 可复现且输出一致。

## 2. 现状盘点（差距分析）

| M0 交付物 | 仓库现状 | 差距 |
|---|---|---|
| `api/`：C ABI 头、proto、manifest schema、SRM schema v0 | `plugin_api.h`（ABI 1.0，180 行）、`plugin.proto`（`PluginRuntime` 7 个 RPC）、`plugin-manifest.schema.json`（4.1 KB） | 缺 SRM schema v0；manifest schema 只有 1 个样例；缺错误码文档；缺 ABI 布局静态断言 |
| hello 插件（C++ 进程内）+ hello 边车（Python） | `examples/hello_plugin`（plugin.cpp / host_demo.cpp / plugin.json）可构建；`examples/python_plugin` 仅示例注释 | 缺边车真实实现、缺边车宿主、缺双传输一致性用例 |
| 内核骨架：注册表、调度循环、日志、诊断、插件宿主 | 无 `src/`、`core/` | 全部缺失 |
| CMakePresets + `bootstrap-msys2.sh` + CI（Win/Linux） | 5 组预设、`tools/env/bootstrap-msys2.sh`、`check-msys2-packages.sh`、`packages.txt` 齐备 | 本机缺 cmake/ninja；无 `.github/workflows` |
| Qt 空壳 + 原生窗口句柄传递 | 无 `apps/`；Qt6 未安装 | 全部缺失 |
| 文档 01–05 初稿 | 已完成 01–07 | 需随实现回填「实现状态」与待定项结论 |

工具链实测（2026-10-22，本机）：

| 组件 | 状态 |
|---|---|
| MSYS2 UCRT64 GCC | 16.1.0 可用 |
| Python（UCRT64） | 3.14.6 可用（文档支持矩阵为 3.11–3.13） |
| pacman / bash | 可用 |
| CMake / Ninja | **未安装**（`C:\msys64\ucrt64\bin` 下不存在） |
| Catch2 / Qt6 / gRPC | 未安装 |

## 3. 执行步骤

按「先环境、后契约、再骨架」排序；契约冻结是硬前置，否则内核与边车会反复返工。

### 阶段 0：环境闭环（0.5 天）

| 编号 | 任务 | 产出 | 完成判据 |
|---|---|---|---|
| T0.1 | `pacman -S --needed mingw-w64-ucrt-x86_64-{cmake,ninja,ccache,catch2,gdb}` | 工具链可用 | `cmake --version`、`ninja --version` 有输出 |
| T0.2 | `cmake --preset win-ucrt64-debug` → `cmake --build --preset ...` → `ctest --preset ...` | 现有 hello 用例跑绿 | `hello_plugin_abi` PASS |
| T0.3 | 记录配置/构建耗时，回填 `docs/05` §2.4 与本文执行记录 | 基线数字 | 冷构建时长有记录 |

> ⚠ 实测（2026-10-22）：`pacman`、`bash` 与 git 的凭据助手在 DSH 沙箱内都无法启动（`couldn't create signal pipe, Win32 error 5`），安装类动作必须在**完全权限会话**或用户自己的 MSYS2 UCRT64 终端执行；沙箱内只做构建/测试的读取与验证。

### 阶段 1：契约冻结（4 天，与阶段 2 前段并行）

对齐 [docs/02](../../../docs/02-plugin-contract.md) §12 的 6 条出口条件。

| 编号 | 任务 | 产出 | 完成判据 |
|---|---|---|---|
| T1.1 | SRM schema v0，覆盖 [docs/03](../../../docs/03-data-contracts.md) §3.3 主描述结构 | `api/schema/srm.schema.json` + 3 个最小模型样例 | 样例通过 schema 校验 |
| T1.2 | manifest schema 用 10 个真实清单校验（含反例） | `api/schema/examples/*.json` | 10/10 校验结果符合预期 |
| T1.3 | ABI 布局静态断言 | `tests/abi/layout_static_assert.cpp` | `offsetof`/`sizeof` 在 Win(GCC) 与 Linux(GCC/Clang) 一致；`struct_size` 校验用例 |
| T1.4 | 版本协商与拒绝路径 | 内核侧 `version` 模块 + 单测 | 主版本不匹配 → `srsp_plugin_query` 返回 NULL / `SRSP_E_VERSION_MISMATCH` |
| T1.5 | 错误码表与诊断文案 | `api/include/srsp/error_codes.md`（或 02 附录） | 12 个错误码都有触发条件与建议动作 |

### 阶段 2：内核骨架（7 天）

建议目录（与 [docs/05](../../../docs/05-msys2-dev-environment.md) §6 的仓库结构一致）：

```
src/core/include/srsp/core/*.hpp    # 公共头（仅内核内部使用）
src/core/src/{plugin_host,registry,scheduler,time_service,diagnostics}.cpp
apps/cli/main.cpp                   # srsp-cli
tests/unit/                         # Catch2
tests/conformance/                  # 双传输一致性
```

| 编号 | 任务 | 要点 | 完成判据 |
|---|---|---|---|
| T2.1 | PluginHost | `LoadLibrary`/`dlopen`、`srsp_plugin_query`、`struct_size` 校验、init/call/checkpoint/restore/shutdown、卸载顺序 | 加载/卸载/错误路径单测全绿 |
| T2.2 | Registry | 插件表（`plugin_id`）+ 能力表（`capability@major` → 实现列表/优先级） | 能力解析与多实现选择有用例 |
| T2.3 | Scheduler + TimeService | 固定步长宏步循环骨架（事件队列占位）、TAI 累计、暂停/单步/倍速 | 1000 步时间累计无漂移；步进顺序确定 |
| T2.4 | Diagnostics | `severity + code + message + context`，控制台 + JSON 行日志 | 结构化日志可被测试断言 |
| T2.5 | 内核 CMake 接入 | 顶层 `add_subdirectory(src/core)`、`apps`、`tests`，替换现有占位注释 | `SRSP_BUILD_EXAMPLES=OFF` 也能独立构建内核 |

> 进度（2026-10-24）：**阶段 1、2、3、6 全部完成**——契约 6/6、内核骨架（T2.1–T2.4）、双传输（T3.1–T3.4）、CI（`windows-ucrt64`＋`linux-gcc`＋`docs` 三个 job，`docs_check` 进 ctest）；**T4 起步**（`version` / `demo --transport inproc|sidecar`）；**审计 D1/D2/D7 已修复**。四端：UCRT64 GCC/Clang 20/20，CLANG64 10/10，MSVC 9/9。剩余：T4 其余命令（非出口必需）、D3/D4（M1）；CI 首轮真实运行待推送后确认。

### 阶段 3：双传输 hello（5 天）

| 编号 | 任务 | 要点 | 完成判据 |
|---|---|---|---|
| T3.1 | `srsp-cli demo --transport inproc\|sidecar` | 同一请求序列双路执行 | 两条路径退出码 0 |
| T3.2 | Python 边车实现 | 基于 `api/proto/srsp/v1/plugin.proto` 的 `PluginRuntime`：Handshake / Invoke / Ping / Shutdown，先实现 `core.describe` 与 `hello.echo` | 边车可独立启动并响应 Ping |
| T3.3 | 边车宿主 | 进程拉起、token 握手、超时、一次重启路径 | 杀死边车后能重启并恢复调用 |
| T3.4 | 一致性用例 | 相同输入两路输出逐字节一致 | `<model>/ModuleTest/integration` 用例通过（原写 `tests/conformance`，已按 [02 §10.3](../../../docs/02-plugin-contract.md) 的分层语义归位） |

M0 只走 gRPC + inline bytes；共享内存（`BlobRef`/`AttachShm`）按 [docs/02](../../../docs/02-plugin-contract.md) §7 推到 M2 再落地，避免 M0 被 Windows 共享内存细节拖住。

### 阶段 4：CLI 与测试工具（4 天，与阶段 3 并行）

| 编号 | 任务 | 完成判据 |
|---|---|---|
| T4.1 | `srsp-cli validate model\|project` | 非法 SRM/项目文件返回非 0 并给出定位信息 |
| T4.2 | `srsp-cli schema-hash` | 对 `api/schema/**` 生成稳定哈希，供清单比对 |
| T4.3 | 测试分组落地 | `ctest` 能按 unit / conformance / abi 分组执行 |

### 阶段 5：Qt 空壳（**已移出 M0 → M1**，2026-10-23 调整）

> 依 [规划优化方案](2026-10-23-roadmap-optimization-plan.md) 决策 D-13：M0 不产出 Qt 空壳，避免与 M1 的最终形态（窗口 + 原生句柄 + `renderer.null`）重复建设。以下任务保留在 M1 执行。

| 编号 | 任务 | 完成判据 |
|---|---|---|
| T5.1 | 安装 `qt6-base`（必要时 `qt6-declarative`） | CMake 能找到 Qt6（✅ 2026-10-22 已完成：Qt 6.11.1） |
| T5.2 | `apps/studio`：`QMainWindow` + 原生句柄（Windows `HWND` / Linux `xcb_window_t`）导出 | 句柄传递路径有日志可查 |
| T5.3 | 空壳内清屏（`renderer.null` 插件 + QOpenGLWidget 占位） | 窗口可启动并显示 |

bgfx 本阶段不引入：按 [docs/07](../../../docs/07-roadmap-risks.md) §5 R2，bgfx 先做 2 周 spike 再决定是否作为主渲染器，spike 放在 M1 入口。

### 阶段 6：CI（第 2 周起持续）

| 编号 | 任务 | 完成判据 |
|---|---|---|
| T6.1 | `.github/workflows/ci.yml`：`windows-msys2`（setup-msys2 + pacman 缓存 + ccache + preset + ctest）与 `linux-gcc` 两个 job | PR 上两端都绿 |
| T6.2 | 文档校验 job：JSON/YAML 解析、Markdown 相对链接 | 文档漂移能被 CI 拦住 |
| T6.3 | 分支保护（可选） | main 需要 CI 通过 |

### 依赖关系

```
T0 环境 ──▶ T2 内核 ──▶ T3 双传输 ──▶ M0 出口
   │            ▲            ▲
   └──▶ T1 契约 ┘            │
T4 CLI ────────┴─────────────┘
T6 CI（贯穿）          （T5 Qt 已移入 M1）
```

## 4. 排期（单人全职 4 周；多人可按工作流并行压缩到 3 周）

| 周 | 主要工作 | 周末应达成 |
|---|---|---|
| W1（D1–D5） | T0 环境、T1 契约、T2.1–T2.2 骨架、T6.1 CI 雏形 | 构建可复现；ABI 断言与 manifest/SRM schema 落地；插件宿主能加载 hello |
| W2（D6–D10） | T2.3–T2.5、T3.1–T3.2、T4.1 | `srsp-cli demo --transport inproc` 跑通；边车能握手 |
| W3（D11–D15） | T3.3–T3.4、T4.2–T4.3 | 双传输输出一致；CLI 校验可用（原 T5.1–T5.2 已移 M1） |
| W4（D16–D20） | T6.2、`tests/bench` 骨架、文档回填、出口验收 | M0 出口判据全部满足；演示脚本可复现 |

## 5. 交付物与验收

| 交付物 | 验收（对齐 docs/07 §3 与 docs/02 §12） |
|---|---|
| `api/` 完整契约 | ABI 布局断言 Win/Linux 一致；manifest 10 样例；SRM schema v0 + 样例校验通过 |
| 内核骨架 | 加载/卸载/配置/错误路径有单测；core 覆盖率 ≥ 70% |
| `srsp-cli demo` 双传输 | 同输入输出一致，退出码 0 |
| `srsp-cli validate` | 非法输入可定位报错 |
| CMake + CI | 干净机器按 README ≤30 分钟构建；PR 上 Windows/Linux 双绿 |
| Qt 空壳 | 窗口可启动，句柄传递可查 |
| 文档 | 01–05 回填实现状态；03 §12 与 07 §10 待定项关闭并记录结论 |

**出口判据（复述 docs/07）**：新成员按 README 从零构建成功；CI 绿；双传输 demo 可复现。

## 6. 首批可执行动作（72 小时内）

| # | 动作 | 状态 |
|---|---|---|
| 1 | `pacman` 安装 CMake / Ninja / ccache / Catch2 / gdb（`catch` 而非 `catch2`） | ✅ 2026-10-22 完成 |
| 2 | 用 `win-ucrt64-debug` 预设配置 + 构建 + `ctest`，记录耗时 | ✅ 完成（配置 ~4 s，构建 ~10 s，ctest 3/3） |
| 3 | 建 `src/core`、`apps/cli`、`tests/{unit,abi,conformance}` 目录与最小 CMake 接入 | ⏳ 部分：`tests/abi`、`tests/schema` 已建；`src/core`、`apps/cli`、`tests/unit` 未建 |
| 4 | 起草 `api/schema/srm.schema.json`（浮基 + 关节 + 传感器安装三类节点） | ❌ 未开始 |
| 5 | 起草 `.github/workflows/ci.yml` 的 windows-msys2 job | ❌ 未开始 |

## 7. 风险与待确认

| 项 | 说明 | 处理 |
|---|---|---|
| CMake/Ninja/Qt6/gRPC 未安装 | 已实测确认，直接阻塞 T0 | 阶段 0 安装；docs/05 §2.4 基线同步更新 |
| Python 3.14 与文档矩阵冲突 | 本机 3.14.6，docs 声明 3.11–3.13 | 见决策 D-4；M0 边车只用标准库+gRPC，风险可控 |
| 沙箱内 `sh.exe`/`pacman` 不可用 | 任何 MSYS 程序（pacman、bash、git 凭据助手）都会报 signal pipe 错误 | 安装与 git 凭据操作在完全权限会话执行；沙箱内只用 PowerShell 调 `cmake`/`git` 做构建与校验 |
| CI Windows 无 GPU | Qt 空壳只能在本地验证显示 | CI 只编译不运行 Qt；显示验证进手工清单 |
| 边车通信延迟 | docs/07 R4 | M0 记录 `Ping` 往返基线，M2 再优化 |
| 契约冻结被拖 | 冻结晚会导致内核返工 | T1 设硬截止（D5），未决项按默认值先行并在文档标注 |

## 8. 决策项（需评审关闭，建议 D5 前）

| 编号 | 决策 | 当前倾向 | 影响 |
|---|---|---|---|
| D-1 | 柔性附件是否进 v1（03 §12.1 / 07 §10.1） | 不进，1.1 引入 | 决定 SRM 1.0 与动力学接口范围 |
| D-2 | 关节坐标系与 URDF 对照表（03 §12.2） | M1 用黄金模型比对后定稿 | 影响导入器与验证用例 |
| D-3 | `quat_wb` 与角速度约定（03 §12.3） | 沿用「世界线速度 + 体系角速度」 | 影响状态表与传感器 |
| D-4 | Python 支持矩阵（07 §10.4） | 3.11–3.13；本机 3.14 仅作开发验证 | 影响边车 wheel 与打包 |
| D-5 | MCAP 中 Arrow schema 编码（03 §12.5） | 内嵌 IPC schema | 影响录制格式与回放工具 |
| D-6 | 是否首发 Linux 包（07 §10.2） | M3 提供 AppImage/deb | 影响排期与 CI 矩阵 |

## 9. 执行记录

| 日期 | 动作 | 结果 |
|---|---|---|
| 2026-10-22 | 依据 docs/01–07 与仓库实际状态建立计划 | 完成 |
| 2026-10-22 | 阶段 0：安装 CMake 4.4.0 / Ninja 1.13.2 / ccache / Catch2；`win-ucrt64-debug` 配置+构建+ctest | 3/3 通过 |
| 2026-10-22 | 阶段 1（部分）：ABI 增补 + 布局断言、manifest schema 工具链标签 + 10 样例、`core_api.h` v0 | GCC/MSVC/Clang×2 验证通过 |
| 2026-10-22 | 阶段 2/3 前置：Clang（UCRT64/CLANG64）与 MSVC 预设；四端 ctest 各 3/3；清理 7.1 GiB 重复工具链 | 完成 |
| 2026-10-23 | 清理 `deepseek/Temp/tmp` 三处探测残留（git-askpass / toolchain-probe / sentinel） | tmp 归零 |
| 2026-10-23 | 状态回填：本文档改「实施中」，同步 §1 现状、§6 首批动作状态 | 本次 |
| 2026-10-23 | 阶段 1 收尾：T1.4 版本协商测试（`tests/unit/abi_negotiation.cpp`）、T1.5 错误码表（`api/error_codes.md`） | 四端各 4/4 通过；docs/02 §12 完成 4/6 |
| 2026-10-23 | 阶段 2 起步：T2.1 PluginHost + 模块测试接入（`src/core`、`deepseek/ModuleTest`） | 四端全绿（UCRT64 5/5、Clang 5/5、CLANG64 4/4、MSVC 4/4）；注释英文合规 |
| 2026-10-23 | 阶段 2 续：T2.2 Registry + T2.3 TimeService（含 12 个新用例） | UCRT64 GCC/Clang 各 7/7；CLANG64/MSVC 各 4/4；英文注释无 CJK |
| 2026-10-23 | 阶段 2 收尾：T2.4 Diagnostics + T2.3 宏步调度（含 9 个新用例） | UCRT64 GCC/Clang 各 9/9；CLANG64/MSVC 各 4/4；5 套件 27 用例 |
| 2026-10-24 | 阶段 4 起步：`srsp-cli`（version / demo inproc）+ 集成测试层 | 四端 13/13、13/13、8/8、8/8；sidecar 显式返回 UNSUPPORTED |
| 2026-10-24 | 阶段 3 传输层：`srsp_proto` + `srsp_bridge`（Server/Host）+ 一致性测试 | 四端 14/14、14/14、8/8、8/8；inproc 与 sidecar 载荷逐字节一致 |
| 2026-10-24 | 阶段 3 收尾：T3.3 边车故障重启 + T4 走 core_api + manifest 装载 | 四端 18/18、18/18、8/8、7/7 |

## 10. 修订记录

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-10-22 | V1 | 初稿：现状盘点、6 个工作流、4 周排期、出口验收与决策项 |
| 2026-10-22 | V2 | 补充沙箱实测：`pacman`/`bash` 无法在 DSH 沙箱内启动，安装类动作需完全权限会话 |
| 2026-10-23 | V3 | 状态改为「实施中」；回填 §1 现状、§6 动作状态与执行记录 |
