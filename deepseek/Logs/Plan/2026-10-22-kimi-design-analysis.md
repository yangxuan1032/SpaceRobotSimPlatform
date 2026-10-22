# Kimi 优化后的设计思路与开发环境构建设计分析

- 日期：2026-10-22
- 状态：已完成（分析报告，不含新决策）
- 提出人：用户
- 关联文档：[01 架构](../../../docs/01-architecture.md)、[02 插件契约](../../../docs/02-plugin-contract.md)、[04 技术栈](../../../docs/04-subsystems-and-stack.md)、[05 MSYS2 环境](../../../docs/05-msys2-dev-environment.md)、[06 打包](../../../docs/06-packaging.md)、[07 路线图](../../../docs/07-roadmap-risks.md)、[设计评审](../../../kimi/Logs/Plan/2026-10-22-design-review.md)、[多语言/工具链方案](../../../kimi/Logs/Plan/2026-10-22-multilang-toolchain-plan.md)

## 1. 分析范围与方法

- 阅读对象：`docs/01`–`07`（含 2026-10-22 更新）、`README.md`、`api/include/srsp/{plugin_api,core_api}.h`、`api/schema/**`、`CMakePresets.json`、`vcpkg.json`、`tests/**`、kimi 的两份计划；
- 对照基线：4 个提交 `e913fbb`（契约）、`abd30c8`（构建）、`59c0aa9`（文档）、`3ecb259`（agent 配置）；
- **验证说明**：本会话尝试从提交状态重建并复跑 ctest，但当前沙箱为 workspace-write，CMake 配置阶段卡在子进程管道（与此前 `g++` 编译、`sh.exe` 报 `couldn't create signal pipe, Win32 error 5` 同源），故**未完成复验**。下文构建相关结论引用 `kimi/Logs/Log/CHANGELOG.md` V4 与提交信息中的验证记录，并逐处标注。

## 2. 软件设计思路：从"允许插件用别的语言"到"内核本身可被别的语言调用"

### 2.1 优化前后的主线变化

| 维度 | 优化前 | 优化后 |
|---|---|---|
| 契约方向 | 只有"内核 → 插件"（`plugin_api.h`） | 增加"应用 → 内核"（`core_api.h` draft 0.1） |
| 语言自由度 | 限插件层（C/C++/Rust 进程内，其余边车） | 平台级政策：官方实现上限 C++/Python/Rust，混合只发生在三类边界 |
| 可移植性 | 约定性描述（"只允许尾部追加"） | 可执行条款：布局 static_assert、禁位域、枚举 32 位、插件静态 CRT |
| 分发粒度 | `os-arch` 一份二进制 | `os-arch-toolchain` 分层，按工具链选载，旧键兼容 |
| 时间契约 | 仅 `double time_tai()` | 追加 `int64 time_tai_ns()`（struct_size 演进） |

### 2.2 四条设计原则（从实现反推）

1. **契约双向化**：`plugin_api.h` 与 `core_api.h` 互为镜像，规则同构（`struct_size`、UTF-8 长度前缀、内核侧 `srsp_buffer_release`）。这条改动让 Rust CLI / Python SDK / Web 服务不必再"绕边车"才能驱动内核，是把"多语言"从**插件侧**推进到**宿主侧**的关键一步。
2. **自由度必须有上限**：官方实现只允许三种语言，其余语言一律走边车。这避免了"多语言"退化成"多套工具链 + 多套调试方式"的失控，也是 R13（多工具链维护成本）的第一道闸。
3. **可移植性写成可执行断言**：`tests/abi/layout_static_assert.cpp` 把"MSVC/GCC/Clang 布局一致"从文档承诺变成构建门禁；配合"插件静态链接 CRT"条款，跨工具链互载从"理论上可以"变成"已验证可以"（kimi 会话记录：双向互载 exit 0）。
4. **确定性优先的渐进演进**：`time_tai_ns` 以追加字段方式进入 `srsp_host_v1`，既修正了 double 秒在 J2000+26 年后只剩 ~0.1 µs 分辨率的精度问题，又保持了 ABI 1.0 兼容——是"只追加、不破坏"规则的一次真实演练。

### 2.3 与空间仿真领域设计的耦合

- `core_api.h` 的快照语义直接绑定 Arrow IPC（"checkpoint 与 get_snapshot 共用一条序列化路径"），把 O7（两套状态序列化并存）的收敛方向写进了新契约；
- 订阅接口按话题路由，与 03 文档的话题表（`sim.state`/`sim.event`/`cmd.actuator`）对齐，未引入新的消息模型；
- 插件选择仍按能力寻址，工具链标签只影响**二进制解析**，不影响场景语义——这个边界切得很干净，场景文件不会因为换工具链而失效。

## 3. 开发环境构建设计

### 3.1 预设分层的设计

```
base            Ninja、编译命令导出、构建目录 = <model>/Temp/build/<preset>
├─ base-gcc     -ffp-contract=off
│  ├─ win-ucrt64-debug/release   （绝对路径写死 gcc/g++/ninja，避开 MSVC 探测）
│  └─ linux-gcc-debug/release、linux-clang-asan
├─ base-msvc    （依赖 vcvars 环境，不写死 cl 路径）
│  ├─ win-msvc-debug/release
│  └─ base-vcpkg  CMAKE_TOOLCHAIN_FILE=$env{VCPKG_ROOT}/...
│     └─ win-vcpkg-debug/release
```

设计意图很明确：**编译器专属的 flag 下沉到 `base-gcc`/`base-msvc`**，公共项留在 `base`；这正是把 GCC 的 `-ffp-contract=off` 从 `base` 移出去的原因——否则 MSVC 会因未知参数失败。构建目录继续绑定 `SRSP_AI_MODEL`，与 AGENTS.md 的命名空间规则一致，未因多工具链而破坏。

### 3.2 工具链矩阵：主备分级而非平权

- 主：MSYS2 UCRT64（CI 必过、发布用它）；备：MSVC 2022+/vcpkg（CI 先"允许失败"再转必修）；
- 补充形态：WSL2（单机双平台）、Docker/Dev Container（本地=CI）；
- 关键规则：**ABI 不可混链**（MSVC 与 MinGW 产物禁止互链，跨工具链只走纯 C ABI 加载或边车）、**边车优先**（只在一种工具链能编的组件默认做边车）。

这套分级的实质是：**用"主工具链"保证交付确定性，用"备工具链"换取生态（Qt 官方包、vcpkg、MuJoCo 预编译、PIX/RenderDoc）**，并用"边车优先"限制矩阵爆炸。

### 3.3 验证体系：测试即契约

| 测试 | 覆盖 | 进入方式 |
|---|---|---|
| `hello_plugin_abi` | 加载/init/call/checkpoint/restore/shutdown | ctest |
| `abi_layout` | 40+ 条 `sizeof`/`offsetof`/`alignof` 断言 | ctest |
| `schema_manifest_examples` | 10 个清单样例（6 正 4 反） | ctest（python + jsonschema） |

三个测试都被设计成"换工具链也照跑"，这是环境设计里最有价值的部分：**契约的可移植性由构建系统保证，而不是靠人记着去验证**。

### 3.4 依赖与基线管理

- 三层依赖：系统包（pacman/apt/vcpkg manifest）→ superbuild（FetchContent 固定 tag + URL_HASH）→ 系统自带；
- 本机基线写进 `docs/05 §2.4`（CMake 4.4.0 / Ninja 1.13.2 / ccache / Catch2 / MSVC 19.50），可被后来者复现与对照；
- `vcpkg.json` 采用 manifest 模式，与 MSYS2 的 `packages.txt` 形成"每工具链一份清单"的对称结构。

## 4. 评价

### 4.1 做得好的地方

1. **契约从纸面变成门禁**：三条 `static_assert` 类断言 + schema 样例校验都进了 ctest，且双工具链跑过（kimi 记录：GCC 16.1 与 MSVC 19.50 全过）；
2. **多语言被收敛而不是放开**：三种官方语言 + 三类边界 + "禁止组件内部多语言交织"，既满足需求又控住复杂度；
3. **矩阵有主次、有门槛**：备选工具链"允许失败→转必修"的渐进策略，避免一上来四套 CI 同时红；
4. **文档与代码同步到位**：`docs/02 §12` 两项冻结清单被真实勾销（附验证证据与日期），不是"改完就算"；
5. **"边车优先"是个被低估的杠杆**：它把 MuJoCo 这类单工具链依赖挡在进程内插件之外，直接压低了 R13 的暴露面。

### 4.2 代价与风险

| # | 问题 | 说明 | 严重度 |
|---|---|---|---|
| K1 | `core_api.h` 是 draft 且**无实现** | 契约先于实现，缺少一次"宿主侧调用跑通"的验证；若内核骨架迟迟不落，契约可能与实现脱节（如快照所有权、回调线程语义） | 高 |
| K2 | 双工具链 = 双份二进制 | 每个进程内插件要出 `ucrt64`/`msvc` 两份，打包、签名、体积、测试矩阵全部翻倍；R13/R14 是真实成本，不是纸面风险 | 中 |
| K3 | 静态 CRT 的隐性代价 | 跨工具链互载解决了"加载"，但每个插件有独立的 CRT 全局状态（locale、errno、文件句柄表、堆）；`host->alloc/free` 只统一了分配器，未统一这些状态 | 中 |
| K4 | FP 策略未定稿 | MSVC 预设描述里明确"FP contraction 暂用编译器默认"，意味着 GCC(`-ffp-contract=off`) 与 MSVC 的数值结果不保证一致；跨工具链回归缺基线，与评审 O5 的容差策略尚未对齐 | 中 |
| K5 | 计划与实现有缺口 | 方案承诺的 `win-clang-cl-debug`、`wsl-gcc-debug`、`docker-gcc-release` 预设**未落地**；`win-vcpkg-*` 因未配置 `VCPKG_ROOT` 未验证；CI 仍只存在于文档示例中 | 中 |
| K6 | 工具链标签只覆盖 x86-64 | 布局断言含 `sizeof(void*)==8`，aarch64（Windows ARM / Linux ARM）未覆盖；扩容时需重做断言矩阵 | 低 |
| K7 | 沙箱内无法构建 | DSH 的 workspace-write 模式限制子进程管道，CMake/g++ 会卡住或失败（本次分析已复现 configure 挂起）；"本地构建"实际上得在完全权限或原生终端里做，这一点值得写进 05 的排错手册 | 低（环境相关） |

### 4.3 结构性问题（跨两份文档）

- `core_api.h` 声明的"错误对象归内核所有、缓冲由 `srsp_buffer_release` 释放"，与 `plugin_api.h` 的"`message` 有效期至下一次调用、由被调方持有"仍是两套规则（评审 O8 未做）；跨语言绑定生成时容易各写一套，建议在 M0 内核实现前统一；
- `checkpoint` 与 `get_snapshot` 的"共用一条序列化路径"只写在 `core_api.h` 注释里，`docs/02` 的 `core.checkpoint` 定义尚未同步改写（O7 未落）。

## 5. 建议的下一步（按优先级）

1. **给 `core_api.h` 落最小实现**（哪怕先在 `examples/` 里做一个 50 行的 `host_api_demo`），验证上下文生命周期、快照释放、订阅回调线程语义——否则 draft 契约无法收敛；
2. **配置 `VCPKG_ROOT` 并跑通 `win-vcpkg-release`**，闭环"第三方依赖"这条目前完全空白的路径；
3. **补 Clang 端的 `abi_layout`**（`linux-clang-asan` 已有预设，`win-clang-cl-debug` 未落地），把三编译器承诺补齐到三；
4. **定稿 MSVC 的 FP 策略**（`/fp:strict` 或明确接受 contraction + 容差表），并同步到 01 §6.3 / 03 §2.1 与预设描述；
5. **CI 落地**：msys2 + msvc + linux 三个 job + schema/abi 两条契约测试，把手工验证固化成门禁；
6. 把方案中承诺的 `wsl-gcc-debug` / `docker-gcc-release` 预设补齐或从方案里显式删除（避免"文档承诺 > 实现"）。

## 6. 执行记录

| 日期 | 动作 | 结果 |
|---|---|---|
| 2026-10-22 | 通读文档/契约/预设/测试与 4 个提交，尝试重建验证 | 分析完成；构建复验受沙箱管道限制未完成（configure 挂起），已如实标注 |

## 7. 修订记录

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-10-22 | V1 | 初稿 |
