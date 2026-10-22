# 方案：以 MSYS2 多环境（UCRT64 / CLANG64 / MSYS）组合编译工具链

- 日期：2026-10-22
- 状态：实施中（E-1～E-5 已按建议执行；V1–V4 完成，V5 负例待 CI）
- 提出人：用户
- 关联文档：[多语言/工具链方案](../../../kimi/Logs/Plan/2026-10-22-multilang-toolchain-plan.md)、[05 MSYS2 环境](../../../docs/05-msys2-dev-environment.md)、[02 插件契约](../../../docs/02-plugin-contract.md)、[06 打包](../../../docs/06-packaging.md)

## 1. 问题与目标

用户提出：除 git 外，能否把相关工具链都装进 MSYS2，并且不只 UCRT64，还可以用 CLANG64 等其他 MSYS2 环境联合组成编译工具链。

**结论先行：可以，而且 CLANG64 环境已经是"半成品"（已装 clang 22.1.8 + lld + lldb + libc++ + compiler-rt + llvm 工具，共 8.97 GB，只缺 cmake/ninja/gdb）。** 但必须按 C++ 运行时（libstdc++ vs libc++）划清职责，否则会掉进"两套标准库混链"的坑。

## 2. 技术事实（决定一切的三条）

| 事实 | 含义 |
|---|---|
| UCRT64 = MinGW-w64 + **libstdc++**（GCC 的 stdlib） | 与 GCC 构建的 C++ 目标兼容（同一 libstdc++） |
| CLANG64 = LLVM + **libc++** + libunwind（UCRT 运行时） | C++ 标准库 ABI **与 UCRT64 不兼容**；C ABI 兼容 |
| UCRT64 内也提供 clang 包（`mingw-w64-ucrt-x86_64-clang`） | 这是"clang 但用 libstdc++"，**C++ ABI 与 GCC 构建互通**，是性价比最高的选择 |

推论：

1. **纯 C ABI 的插件契约让跨环境加载始终可行**（我们的 `plugin_api.h` 已强制纯 C + 静态运行库）；
2. **C++ 目标不能跨环境混链**（UCRT64 的 libstdc++ 目标不能链 CLANG64 的 libc++ 库，反之亦然）；
3. 想在 UCRT64 里用 clang 工具（clangd/clang-tidy/ASan），应装 **ucrt64 版 clang**，而不是去调 CLANG64 的 clang——否则头文件与库会串味。

## 3. 环境矩阵与职责分工（建议）

| 环境 | 角色 | 产物用途 | 现状 |
|---|---|---|---|
| **UCRT64 + GCC** | 主工具链（CI 必过、发布） | 所有交付二进制 | ✅ 已就绪 |
| **UCRT64 + Clang** | 第二编译器（同 libstdc++） | ABI 断言、clang-tidy、clangd、ASan 探测 | ⚠️ 需装 `clang`/`clang-tools-extra`/`compiler-rt` |
| **CLANG64（libc++/lld）** | **验证层**：sanitizer、lldb、libc++ 可移植性检查 | 不作为插件分发工具链 | ⚠️ 缺 cmake/ninja/gdb |
| **MSVC 2026** | 生态层（Qt 官方包、vcpkg、PIX/RenderDoc） | 插件第二份二进制、调试 | ✅ 已就绪 |
| **MSYS（msys 子系统）** | 只做 shell 工具（bash/pacman/make/autotools） | **不产出任何 C++ 产物**（cygwin 式 ABI） | ✅ 已用 |

**关键决策：CLANG64 只做"验证与工具"层，不参与插件分发。** 理由：libc++ 动态运行库（`libc++.dll`/`libunwind.dll`）需要随包分发或静态链接，而 MSYS2 的 clang64 默认动态链接；把它变成分发工具链会多一套运行库矩阵，收益（多一个编译器验证）远小于成本。需要 libc++ 构建时，用 CLANG64 跑测试/ASan 即可。

## 4. 安装清单（按环境）

| 环境 | 安装项 | 用途 | 预估体积 |
|---|---|---|---|
| UCRT64 | `clang`、`clang-tools-extra`、`compiler-rt`、`lld`（可选） | 第二编译器、clangd、clang-tidy、ASan | +0.6–1.0 GB |
| UCRT64 | `python-pip`、`eigen3`、`protobuf`、`grpc`、`catch`（已装）、`benchmark` | M0 依赖 | +1–2 GB（gRPC 大） |
| CLANG64 | `cmake`、`ninja`（或复用 UCRT64 的 cmake/ninja，见 §5.2） | 在该环境构建测试 | +0.2 GB |
| CLANG64 | `catch` | 在 clang64 跑单元测试 | +5 MB |
| MSYS | `base-devel`、`git`（可选，现有 Git for Windows 亦可） | bash 工具链完整性 | +0.3 GB |

> 磁盘现状：ucrt64 2.29 GB、clang64 8.97 GB（已装 clang 全家桶）、mingw64 0 MB。CLANG64 已是大头，本方案的增量主要是 UCRT64 侧。

## 5. CMake 预设扩展

### 5.1 新增预设

| 预设 | 编译器 | 说明 |
|---|---|---|
| `win-ucrt64-clang-debug` / `-release` | `C:/msys64/ucrt64/bin/clang++.exe` | 同 libstdc++，可与 GCC 目标互链；用于 ABI 断言与 clang-tidy |
| `win-clang64-debug` | `C:/msys64/clang64/bin/clang++.exe` | libc++ 验证层；产物仅供测试，不进分发 |

两者都继承 `base`（公共项），并各自显式写死编译器绝对路径，避免 PATH 决定命运——这与现有 `win-ucrt64-*` 的做法一致。

### 5.2 cmake/ninja 归属

两个选择：

- **A（推荐）**：CLANG64 只装 `catch`，cmake/ninja 复用 UCRT64 的（构建工具本身不进入产物，其 C++ 运行库与结果无关）；
- B：CLANG64 内装 `cmake`/`ninja`，环境自足但多一份维护。

### 5.3 环境隔离规则（必须写进文档）

1. **PATH 一次只放一个环境**：UCRT64 与 CLANG64 的 `bin` 绝不共存于同一 PATH——同名 DLL（`zlib1.dll`、`libpng16.dll` 等）会按 PATH 顺序被错误加载；
2. 预设一律写绝对路径；脚本里若需切环境，用 `$env:MSYSTEM` + 显式 PATH 重置，而不是追加；
3. 混载测试只走**纯 C ABI 插件 + 静态运行库**这条既有路径。

## 6. 验证计划（可执行）

| # | 验证 | 判据 |
|---|---|---|
| V1 | `win-ucrt64-clang-debug` 构建 + `ctest` | `abi_layout` 在 Clang 下同样通过（补齐"三编译器"里的第三端） |
| V2 | Clang(UCRT64) 构建的插件载入 GCC 宿主 | `core.describe` 调用成功（exit 0）——验证同 libstdc++ 下的互载 |
| V3 | CLANG64 构建 `abi_layout` | 断言通过，证明布局在 libc++ 环境同样成立 |
| V4 | CLANG64 + ASan 跑 `hello_plugin_abi` | 无 sanitizer 报错 |
| V5 | 混链反向实验（负例） | GCC 目标链接 CLANG64 的 C++ 库应失败——把"不可混链"写成 CI 反例 |

## 7. 风险与对策

| 风险 | 说明 | 对策 |
|---|---|---|
| 标准库混链 | libc++ 与 libstdc++ 目标互链会链接失败或运行期崩溃 | 环境规则 + CI 负例 V5；插件静态运行库 |
| PATH 污染 | 两个环境同时在 PATH，DLL 解析不确定 | 一次一环境；预设绝对路径 |
| 体积膨胀 | clang64 已 8.97 GB；再加 cmake/catch 有限 | 不重复安装 LLVM；只补缺项 |
| clangd 双份 | UCrt64 clangd 与 CLANG64 clangd 功能重叠 | 只用 UCRT64 那份（与头文件/库同环境） |
| CI 成本 | 每多一个编译器 job，时间与缓存翻倍 | Clang(UCRT64) 转必修，CLANG64 仅 nightly/允许失败 |
| MSYS 产物误用 | msys 子系统编出的 exe 依赖 cygwin 式运行时 | 文档红线：msys 只跑脚本，不编产物 |

## 8. 执行步骤（与 M0 对齐）

1. **D1**：`pacman -S mingw-w64-ucrt-x86_64-{clang,clang-tools-extra,compiler-rt}`，验证 `clangd --version` 后修正 `.vscode/settings.json` 的 clangd 路径（当前悬空）；
2. **D1**：新增 `win-ucrt64-clang-debug` 预设 + `win-clang64-debug` 预设；
3. **D1–D2**：跑 V1/V2/V3，把结果写进 `docs/05` 与日志；
4. **D2**：`pacman -S mingw-w64-clang-x86_64-catch`，跑 V4（ASan）；
5. **M0 末**：V5 负例进 CI；`docs/05 §7` 补预设表与"一次一环境"规则；
6. **M1**：CI 增加 clang job（Clang-UCRT64 必修，CLANG64 允许失败）。

## 9. 决策项

| 编号 | 决策 | 建议 |
|---|---|---|
| E-1 | CLANG64 是否作为分发工具链 | **否**，仅验证层 |
| E-2 | clang 装在 UCRT64 还是用 CLANG64 | **UCRT64**（同 libstdc++，服务 clangd/ABI/ASan） |
| E-3 | CLANG64 是否自装 cmake/ninja | 否，复用 UCRT64 的 |
| E-4 | MSYS 层 git 是否替换 Git for Windows | 否，保持现状（凭据/CRLF 行为已验证） |
| E-5 | Clang 在 CI 的级别 | UCRT64-clang 必修；CLANG64 允许失败 |

## 10. 执行记录

| 日期 | 动作 | 结果 |
|---|---|---|
| 2026-10-22 | 调研 MSYS2 各环境实况（包清单、可执行文件、体积），形成方案 | 待评审 |
| 2026-10-22 | 按建议执行：安装 UCRT64 clang 22.1.8 + clang-tools-extra + compiler-rt；CLANG64 补 catch | 安装成功；clangd/clang-tidy 就位，`.vscode` 的 clangd 路径不再悬空 |
| 2026-10-22 | 新增预设 `win-ucrt64-clang-debug/release`、`win-clang64-debug`（含 build/test 预设） | 预设可用 |
| 2026-10-22 | V1：Clang(UCRT64) 构建 + ctest | 3/3 通过，`abi_layout` 在 Clang 下通过 |
| 2026-10-22 | V2：GCC 宿主 ↔ Clang 插件双向互载 | 双向 exit 0 |
| 2026-10-22 | V3：CLANG64（libc++）构建并运行 `abi_layout` | 布局与 GCC/MSVC 完全一致（16/16/16/24/80/72/48）；全量构建后全套 ctest 3/3 通过 |
| 2026-10-22 | V3b 附加实验：GCC 宿主加载 CLANG64(libc++) 插件 | 成功（libc++ 运行库在 PATH 时），实证"纯 C ABI + 运行库可用即可跨环境加载"，也说明 CLANG64 不分发的理由（需随包带 libc++/libunwind） |
| 2026-10-22 | clang64 test 预设补 `environment.PATH` | `ctest --preset win-clang64-debug` 免手动 PATH，3/3 通过 |
| 2026-10-22 | V4：CLANG64 + ASan 跑全套 ctest | 3/3 通过，无 sanitizer 报错 |
| 2026-10-22 | 文档同步 | 05 §2.4 基线、§7 预设表与多环境隔离规则；02 §12 勾选补 Clang 证据 |

## 11. 修订记录

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-10-22 | V1 | 初稿：环境矩阵、安装清单、预设扩展、验证计划、5 项决策 |
| 2026-10-22 | V2 | 执行记录：E-1～E-5 关闭，V1–V4 验证通过；V5 负例留待 CI |
