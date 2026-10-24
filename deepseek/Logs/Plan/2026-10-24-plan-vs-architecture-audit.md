# 审计：M0 执行计划与软件架构设计的一致性

- 日期：2026-10-24
- 状态：已完成（审计报告，含 7 项偏离与建议）
- 提出人：用户
- 关联文档：[01 架构](../../../docs/01-architecture.md)、[02 插件契约](../../../docs/02-plugin-contract.md)、[03 数据契约](../../../docs/03-data-contracts.md)、[05 开发环境](../../../docs/05-msys2-dev-environment.md)、[07 路线图](../../../docs/07-roadmap-risks.md)、[M0 执行计划](2026-10-22-m0-execution-plan.md)

## 1. 结论

**总体方向一致，实现层面有 7 处偏离**，其中 2 处影响架构承诺（`core_api` 未被使用、运行时未读 manifest），3 处是实现简化（`remedy`、selection 策略、静态库），2 处是文档/进度未同步（测试位置、工具链矩阵）。

| 判定 | 数量 | 说明 |
|---|---|---|
| ✅ 符合 | 主体 | 契约文件、ABI 规则、双传输、能力寻址、目录与缓存约束 |
| ⚠️ 简化但可接受（M0 阶段） | 3 | `remedy` 字段、selection 仅优先级、`srsp_core` 静态库 |
| ❌ 偏离架构承诺 | 2 | CLI 未走 `core_api`；运行时未按 manifest 装载 |
| 📄 文档未同步 | 2 | 测试目录、工具链矩阵（bridge 仅 UCRT64） |

## 2. 对照矩阵

| 计划/设计条目 | 设计依据 | 实际实现 | 判定 |
|---|---|---|---|
| 契约：`plugin_api.h` + 布局断言 + manifest schema | 02 §4、§12 | 已落地，四端 static_assert 通过 | ✅ |
| 宿主侧 `core_api.h` | 01 §8 ADR A11；架构摘要 L3「CLI/SDK 通过 core_api 驱动内核」 | **draft 0.1 无实现**；`srsp-cli` 直接链接 `srsp_core` 的 C++ 类 | ❌ D1 |
| 插件装载流程（manifest → 校验 → 加载 → 握手） | 02 §3.2 | `PluginHost::load` 直接 `LoadLibrary`/`dlopen` + `srsp_plugin_query`，**未读 `plugin.json`** | ❌ D2 |
| 能力寻址与 selection（优先级 + 用户固定 + 基准得分） | 01 §6.1、03 §6.4 | `Registry::resolve` 只做「name+major 匹配 + 优先级 + 插入序」 | ⚠️ D3 |
| 诊断模型 `severity + code + message + context + remedy` | 01 §6.5 | `Diagnostic` 只有 severity/code/message/context_json，**无 remedy** | ⚠️ D4 |
| 双传输（inproc / sidecar）与一致性 | 02 §7、07 M0 | 已实现：gRPC + token/ABI 握手 + 一致性测试 + Python 边车 | ✅（T3.1/T3.2） |
| 内核骨架（宿主/注册表/调度/时钟/诊断） | 01 §5、§6 | 已实现（`src/core`），27 个单元用例 | ✅ |
| 测试分层（`tests/` 契约 vs 模块开发测试） | 计划 §3 写 `tests/unit`、`tests/conformance` | 单元/集成测试实际在 `<model>/ModuleTest/{unit,integration}`；`tests/` 只放工程级契约测试 | 📄 D5 |
| 工具链矩阵（四端各自通过） | 05 §2.4、§7 | 桥接层（gRPC）**仅 UCRT64**；CLANG64/MSVC 自动跳过（7 个测试 vs 15 个） | 📄 D6 |
| 内核形态（单进程，可拆 `srspd`） | 01 §4.1 B 案 | `srsp_core` 为静态库，仅 C++ 可用；C API 未导出 | ⚠️ D7 |
| 多机器人协同（v1） | 01 §6.6、03 §8 | 设计已入文档，实现属 M2 | ✅（未到实施期） |
| CI（Windows + Linux 双 job） | 05 §10、计划 T6 | 未建 | ⬜ 进度（非偏离） |
| 缓存与目录约束 | AGENTS §1 | 构建/缓存均在 `<model>/Temp` | ✅ |

## 3. 偏离明细

### D1（高）CLI 未经过 `core_api`，架构承诺未验证

- 事实：`apps/cli/main.cpp` 直接 `#include <srsp/core/plugin_host.hpp>` 并链接 `srsp_core`；`core_api.h` 只有声明。
- 影响：ADR A11 的「Rust CLI / Python SDK / Web 服务可嵌入内核」目前**无任何代码路径**；`core_api.h` 的所有权/缓冲释放规则（`srsp_buffer_release`）从未被编译验证，后续接入时容易返工。
- 建议：M0 收尾前加一个最小实现（`srsp_context_create/destroy` + `load_plugin` + `step` + `get_snapshot` 的桩），CLI 改走它；或在 01 中把 A11 标为「M1 交付」并同步 07。

### D2（高）运行时未读取插件 manifest

- 事实：`plugin.json` 只用于 schema 校验测试，`PluginHost` 不读它。
- 影响：02 §3.2 的装载流程（路径搜索、`api_major` 前置校验、依赖解析）没有实现；manifest 与实际加载可能不一致（声明的能力与 `query` 返回的 vtable 不交叉校验）。
- 建议：T1.x 增补一个小任务：`load()` 接受 manifest 路径，校验 `id`/`api_major`/平台标签后再 `dlopen`；不改变 ABI。

### D3（中）selection 策略只实现优先级

- 事实：`Registry::resolve` 忽略「用户固定」与「基准得分」。
- 影响：01 §6.1 允诺的实现选择能力不完整；用户无法固定某个求解器。
- 建议：M1 前补 `pin(plugin_id, capability)` 与 `score` 字段（结构已预留 `priority`，追加字段即可）。

### D4（中）诊断缺 `remedy`

- 事实：`Diagnostic` 无 remedy；`api/error_codes.md` 有「处理建议」列但未映射到代码。
- 影响：结构化诊断对用户的可操作性下降；CLI 无法给出「怎么修」。
- 建议：`Diagnostic` 追加 `std::string remedy`（追加字段，不改已有），`error_codes.md` 生成一张 code→remedy 表。

### D5（低）测试目录与计划不一致

- 事实：计划写 `tests/unit`、`tests/conformance`；实际在 `<model>/ModuleTest/`。
- 影响：新人按计划找测试会找错位置；`docs/02 §12` 勾选项引用 `tests/unit/abi_negotiation.cpp`（该文件确实在 `tests/`，属于工程级契约测试），容易被误读为「所有单测都在 tests/」。
- 建议：把边界写进 07 与 02：**契约/ABI/schema/双传输 = `tests/`；模块开发期测试 = `<model>/ModuleTest/`**；并更新 M0 计划的 T3.4 描述。

### D6（低）工具链矩阵未反映桥接限制

- 事实：05 §2.4 写「四套工具链各自 ctest 3/3 通过」；当前实际为 UCRT64 15/15、Clang 15/15、CLANG64 7/7、MSVC 7/7，且 gRPC 桥接仅 UCRT64。
- 影响：读者会误以为四端能力等价；实际上 CLANG64/MSVC 无边车能力。
- 建议：05 增「能力矩阵」小节（构建 / 单元测试 / 集成测试 / 边车桥接 分列），并注明桥接层的平台限制与原因（UCRT64 gRPC 头文件与 libc++ 冲突）。

### D7（低）内核为静态库，C API 未导出

- 事实：`srsp_core` 为 `STATIC`；`core_api.h` 无导出宏。
- 影响：D1 的宿主侧入口无法被 Rust/Python 调用；M1 前需改为共享库 + 导出符号（或提供 C 包装层）。
- 建议：与 D1 合并处理：在实现 `core_api` 时把 `srsp_core` 改为 `SHARED` + 导出宏（`SRSP_API`），并加导出符号检查测试。

## 4. 文档与进度不同步（非偏离，但需修）

| 项 | 现状 |
|---|---|
| `docs/02 §12` 检查项 | 「双传输 hello」已实现但未勾选；「能力消息冻结」仍未完成（6 项中 4 项勾选） |
| `docs/05 §2.4` | 测试数字停留在 3/3，需更新为当前矩阵 |
| M0 计划 T3.4 | 写 `tests/conformance`，实际在 `ModuleTest/integration`，需二选一并统一 |
| 01 §1.3 非目标 | 已更新（仅分布式集群），与实际一致 ✅ |

## 5. 建议动作（按优先级）

| 优先级 | 动作 | 归属 |
|---|---|---|
| P0 | 统一测试目录语义（D5）：更新 02 §12、07、M0 计划 | 文档 |
| P0 | 05 增能力矩阵，注明桥接仅 UCRT64（D6） | 文档 |
| P1 | M0 收尾前落 `core_api` 最小实现 + `srsp_core` 改共享库（D1/D7） | 代码 + 计划 |
| P1 | `load()` 读 manifest 并交叉校验（D2） | 代码 + 计划 |
| P2 | `Diagnostic` 加 `remedy`；Registry 加 pin/score（D3/D4） | M1 |

## 6. 执行记录

| 日期 | 动作 | 结果 |
|---|---|---|
| 2026-10-24 | 对照 M0 计划与 01/02/03/05/07 逐项审计 | 7 项偏离（2 高、2 中、3 低）+ 4 项文档不同步 |

## 7. 修订记录

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-10-24 | V1 | 初稿：对照矩阵、偏离明细、建议动作 |

## 8. 修复记录（2026-10-24 复核）

| 编号 | 状态 | 证据 |
|---|---|---|
| D1 CLI 未走 core_api | ✅ 已修复 | `src/core/src/core_api.cpp` 实现 context/load_plugin/step/plugin_call/buffer_release；`apps/cli/main.cpp` 的 inproc 路径改为纯 C 调用；新增 `integration.host_api_c`（C 语言测试） |
| D2 运行时未读 manifest | ✅ 已修复 | `PluginHost::load_with_manifest` 校验 `id`/`api` 主版本/平台库路径后再加载，`plugin_id()` 可查；`unit.manifest_load` 覆盖成功与缺文件 |
| D7 内核静态库未导出 C API | ✅ 已修复 | 拆为 `srsp_kernel`（STATIC，内部 C++）+ `srsp_core`（SHARED，仅 C API，`SRSP_CORE_BUILD` 控制导出） |
| D3 selection 仅优先级 | ⬜ 待 M1 | — |
| D4 诊断缺 remedy | ⬜ 待 M1 | — |
| D5 测试目录语义 | ✅ 已修复（文档） | 02 §10.3 分层表；07 与 M0 计划改为引用 `ModuleTest/integration` |
| D6 工具链矩阵 | ✅ 已修复（文档） | 05 §2.5 能力矩阵，注明桥接/单测仅 UCRT64 |

复核后剩余：D3、D4（均已在 07 §11 与 M1 计划中登记）；MSVC 的 C 语法面验证转入 CI 待办（ISSUES P11）。