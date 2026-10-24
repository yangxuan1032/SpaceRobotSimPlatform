# 工程修改日志

> 记录规则：
> - 按日期分组，只记录日期，不记录具体时间。
> - 同一天的修改按先后顺序编号 V1、V2……；跨天重新从 V1 开始。
> - 每条记录包含：类型、文件、摘要、验证、关联计划。
> - 本文件是唯一修改日志，不拆分。
> - 追加记录优先使用 `tools/ai/Append-ChangeLog.ps1`。

## 2026-10-20

### V1 — 建立 SRSP 架构与技术栈文档基线

- 类型：新增
- 文件：
  - `README.md`
  - `docs/01-architecture.md`、`docs/02-plugin-contract.md`、`docs/03-data-contracts.md`
  - `docs/04-subsystems-and-stack.md`、`docs/05-msys2-dev-environment.md`
  - `docs/06-packaging.md`、`docs/07-roadmap-risks.md`
  - `api/**`
  - `examples/**`
  - `tools/**`
  - `CMakeLists.txt`、`CMakePresets.json`
  - `.vscode/**`、`.clang-format`、`.editorconfig`、`.gitattributes`、`.gitignore`
- 摘要：完成平台 + 插件总体架构、插件契约、标准数据契约、子系统技术选型、MSYS2 开发环境、跨平台打包与实施路线图；落地最小 C ABI 插件与宿主示例。
- 验证：GCC 16.1.0 编译并运行 hello 插件与宿主（含检查点/恢复）；全部 JSON 可解析；Markdown 相对链接检查通过。
- 说明：本条为日志体系建立前、当日已完成工作的补录。

### V2 — 建立工作区独立 Agent 配置与缓存/计划/日志体系

- 类型：配置
- 文件：
  - `AGENTS.md`
  - `deepseek/README.md`
  - `deepseek/Temp/README.md`
  - `deepseek/Temp/.gitkeep`
  - `deepseek/Log/Plan/README.md`
  - `deepseek/Log/Plan/_TEMPLATE.md`
  - `deepseek/Log/Plan/2026-10-20-srsp-architecture.md`
  - `deepseek/Log/Plan/2026-10-20-agent-workspace-config.md`
  - `deepseek/Logs/Log/CHANGELOG.md`
  - `deepseek/Scripts/Enter-AgentEnv.ps1`
  - `deepseek/Scripts/Append-ChangeLog.ps1`
  - `deepseek/Scripts/New-Plan.ps1`
  - `deepseek/Scripts/labels.json`
  - `CMakePresets.json`
  - `.vscode/settings.json`
  - `.vscode/launch.json`
  - `.vscode/tasks.json`
  - `.gitignore`
  - `README.md`
  - `docs/05-msys2-dev-environment.md`
  - `docs/06-packaging.md`
- 摘要：新增 AGENTS.md 工作区规则与 deepseek 运行目录；三个 PowerShell 辅助脚本（环境注入/计划创建/日志追加）与 labels.json；构建目录与 compile_commands 迁移到 deepseek/Temp，并同步 README、docs/05、docs/06 与 VSCode 配置。
- 验证：环境注入变量指向 deepseek/Temp；New-Plan 生成中文计划；Append-ChangeLog 自动编号；全部 JSON 文件解析通过；Markdown 相对链接检查通过。
- 关联计划：`deepseek/Log/Plan/2026-10-20-agent-workspace-config.md`

### V3 — 统一日志标题样式并修正计划链接

- 类型：文档
- 文件：
  - `deepseek/Scripts/labels.json`
  - `deepseek/Scripts/Append-ChangeLog.ps1`
  - `deepseek/Logs/Log/CHANGELOG.md`
  - `deepseek/Log/Plan/2026-10-20-agent-workspace-config.md`
- 摘要：日志标题改用 em dash（来自 labels.json）；补全无标签文件时的冒号回退；修正计划文档中变更日志的相对链接；统一 V2 标题格式。
- 验证：V1-V3 编号连续；计划链接检查 0 失效；三个 PowerShell 脚本保持纯 ASCII。
- 关联计划：`deepseek/Log/Plan/2026-10-20-agent-workspace-config.md`

### V4 — 修正 V2 验证描述中的 JSON 计数表述

- 类型：文档
- 文件：
  - `deepseek/Logs/Log/CHANGELOG.md`
- 摘要：V2 验证项原写死 JSON 文件数量，改为不依赖计数的表述，避免后续新增模式文件后失真。
- 验证：CHANGELOG 中 V1-V4 编号连续；无时间戳，仅日期分组。
- 关联计划：`deepseek/Log/Plan/2026-10-20-agent-workspace-config.md`

### V5 — 计划目录迁移并支持多模型命名空间

- 类型：重构
- 文件：
  - `AGENTS.md`
  - `CLAUDE.md`
  - `GEMINI.md`
  - `.cursorrules`
  - `.github/copilot-instructions.md`
  - `ai-models.json`
  - `tools/ai/README.md`
  - `tools/ai/Resolve-AiModel.ps1`
  - `tools/ai/Enter-AgentEnv.ps1`
  - `tools/ai/New-AiModelWorkspace.ps1`
  - `tools/ai/New-Plan.ps1`
  - `tools/ai/Append-ChangeLog.ps1`
  - `tools/ai/labels.json`
  - `tools/ai/templates/plan-template.md`
  - `tools/ai/templates/changelog-header.md`
  - `tools/ai/templates/model-README.md`
  - `tools/env/launch-vscode.ps1`
  - `CMakeLists.txt`
  - `CMakePresets.json`
  - `.vscode/settings.json`
  - `.vscode/launch.json`
  - `.vscode/tasks.json`
  - `.gitignore`
  - `README.md`
  - `docs/05-msys2-dev-environment.md`
  - `docs/06-packaging.md`
  - `deepseek/README.md`
  - `deepseek/Temp/README.md`
  - `deepseek/Logs/Plan/README.md`
  - `deepseek/Logs/Plan/2026-10-20-agent-workspace-config.md`
  - `deepseek/Logs/Plan/2026-10-20-srsp-architecture.md`
  - `deepseek/Logs/Log/CHANGELOG.md`
- 摘要：计划从 deepseek/Log/Plan 迁移到 deepseek/Logs/Plan；脚本迁到 tools/ai 并全部支持 -Model；AGENTS.md 重写为模型无关规则，新增 CLAUDE.md/GEMINI.md/.cursorrules/Copilot 入口；新增 ai-models.json 注册表与模型脚手架；CMake/VSCode 构建与编译缓存改由 SRSP_AI_MODEL 解析；删除 deepseek/Scripts 与 deepseek/Log。
- 验证：隔离根目录测试 claude/gpt/gemini 命名空间、别名归一与未设置模型报错；10 个 JSON 解析通过；Markdown 相对链接 0 失效；CMake 未安装，preset 与 guard 仅做静态检查。
- 关联计划：`deepseek/Logs/Plan/2026-10-20-agent-workspace-config.md`

### V6 — 重命名工作区并同步 GitHub

- 类型：重构
- 文件：
  - `AGENTS.md`
  - `README.md`
  - `docs/05-msys2-dev-environment.md`
  - `deepseek/Logs/Log/CHANGELOG.md`
- 摘要：将工作区根目录由 SpaceRobotSimPlantform 重命名为 SpaceRobotSimPlatform；更新 AGENTS、README、docs/05 中的绝对与相对路径；初始化 Git 仓库、关联 GitHub origin，并以远端 main 为父提交推送完整基线。
- 验证：git push 成功 518bd6b..9428239（main）；远端 main 指向基线提交；旧名称引用检查为 0；目录重命名在本条目提交推送后执行。

### V7 — 修复 Enter-AgentEnv 点号引用污染调用方变量

- 类型：修复
- 文件：
  - `tools/ai/Enter-AgentEnv.ps1`
- 摘要：点号引用时脚本参数与内部变量会写入调用方作用域，导致调用方的 $root、$tmp、$cache 被置空；将 -Root 参数改为 -ProjectRoot（保留 Root 别名），内部临时变量统一加 srsp 前缀，行为与导出的环境变量不变。
- 验证：调用方 $root、$tmp、$cache、$context 均保持原值；SRSP_AI_MODEL 与 TEMP/TMP/CCACHE_DIR 等注入不变；-Root 别名仍生效；未指定模型时仍按 ai-models.json 报错；PowerShell 语法解析 0 错误。
- 关联计划：`deepseek/Logs/Plan/2026-10-20-agent-workspace-config.md`

## 2026-10-22

### V1 — 建立 M0 执行计划：契约冻结与骨架闭环

- 类型：文档
- 文件：
  - `deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`
- 摘要：分析 docs/01-07 与仓库现状后给出 M0 执行计划：差距盘点（缺内核、CLI、双传输边车、CI、SRM schema，本机缺 CMake/Ninja）、6 个工作流程（环境/契约/内核/双传输/CLI+Qt/CI）、4 周排期、出口验收与 6 项待决决策。
- 验证：计划内 14 个相对链接全部有效；10 个章节结构完整；工具链状态为 2026-10-22 实测（GCC 16.1.0、Python 3.14.6、CMake/Ninja 未安装）；沙箱实测 pacman/bash 因 signal pipe 无法启动，安装类动作需完全权限会话；未执行构建（cmake 缺失）。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

### V2 — 注册当前模型别名并校验 deepseek 模型工作区

- 类型：配置
- 文件：
  - `ai-models.json`
  - `deepseek/README.md`
- 摘要：ai-models.json 为 deepseek 增加 deepseek-flash、deepseek-official 别名，使当前运行模型解析到 deepseek 命名空间（避免误建 deepseek-flash/ 目录）；deepseek/README.md 补别名清单；清理 deepseek/Temp 下编译探测残留目录。
- 验证：ai-models.json 解析通过；Resolve-SrspAiModel -Model deepseek-flash 返回 deepseek；New-AiModelWorkspace 幂等执行无新增目录；Enter-AgentEnv 注入 SRSP_AI_MODEL=deepseek、TEMP=deepseek/Temp/tmp；Temp/Logs 八个必需路径均存在；仓库内仅 deepseek 一个模型工作区。
- 关联计划：`deepseek/Logs/Plan/2026-10-20-agent-workspace-config.md`

### V3 — 分析 Kimi 优化后的软件设计思路与开发环境构建设计

- 类型：文档
- 文件：
  - `deepseek/Logs/Plan/2026-10-22-kimi-design-analysis.md`
- 摘要：基于 4 个提交与更新后的 docs/api/预设/测试，归纳优化后的设计主线：契约双向化（core_api）、多语言政策收敛为三语言+三边界、可移植性写成 static_assert 门禁、os-arch-toolchain 分发、time_tai_ns 演进；开发环境方面分析 base/base-gcc/base-msvc/base-vcpkg 预设分层、工具链主备分级、测试即契约与依赖三层；列出 7 项风险（core_api 无实现、双份二进制、静态 CRT 隐性代价、FP 策略未定、计划与实现缺口、aarch64 未覆盖、沙箱内不可构建）与 6 条下一步。
- 验证：计划内 9 个相对链接全部有效；结论以提交内容与文档为准；本会话构建复验受沙箱子进程管道限制未完成（configure 挂起），已在文档中如实标注，构建结论引用 kimi 会话 CHANGELOG V4 的验证记录。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-kimi-design-analysis.md`

### V4 — 开发环境与编译工具链审计：修正 Catch2 包名并补基线

- 类型：配置
- 文件：
  - `tools/env/packages.txt`
  - `docs/05-msys2-dev-environment.md`
- 摘要：审计本机 MSYS2/VS/Python/git 配置：修正 packages.txt 中不存在的包名 mingw-w64-ucrt-x86_64-catch2 为 catch（MSYS2 实际包名）；在 docs/05 2.4 基线补充当前未安装项（Qt6、gRPC、protobuf、Eigen、Boost、ucrt64 clang/clangd、MSYS base-devel/git、python-pip）及由此导致的 clangd 路径失效与 pip 缺失；清理 deepseek/Temp 下无 CMakeCache 的半成品构建目录。
- 验证：包名与 pacman 本地库条目 mingw-w64-ucrt-x86_64-catch-3.15.2-1 一致；MSYS2 包库直接读取（237 条）核对；清理后 deepseek/Temp/build 无残留失败目录；未运行构建（沙箱管道限制）。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-kimi-design-analysis.md`

### V5 — 规划 MSYS2 多环境（UCRT64/CLANG64/MSYS）组合工具链

- 类型：文档
- 文件：
  - `deepseek/Logs/Plan/2026-10-22-msys2-multienv-toolchain-plan.md`
- 摘要：评估在 MSYS2 内多环境组合编译工具链：确认 CLANG64 已装 clang 22.1.8+lld+lldb+libc+++compiler-rt（8.97G），UCRT64 无 clang；给出环境职责矩阵（UCRT64+GCC 主、UCRT64+Clang 第二编译器、CLANG64 仅验证层、MSVC 生态层、MSYS 只跑脚本）、各环境安装清单、两个新预设、五条隔离规则、五项验证与五项决策。
- 验证：计划内 7 个相对链接全部有效；环境事实来自 pacman 本地库 237 条与目录实测（clang64 8.97G / ucrt64 2.29G）；未执行安装与构建（待评审后按其步骤执行）。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-msys2-multienv-toolchain-plan.md`

### V6 — 接入 MSYS2 多环境工具链：UCRT64+Clang 与 CLANG64 验证层

- 类型：配置
- 文件：
  - `CMakePresets.json`
  - `docs/02-plugin-contract.md`
  - `docs/05-msys2-dev-environment.md`
  - `deepseek/Logs/Plan/2026-10-22-msys2-multienv-toolchain-plan.md`
- 摘要：按 E-1~E-5 执行：安装 UCRT64 clang 22.1.8 + clang-tools-extra + compiler-rt（CLANG64 补 catch）；CMakePresets 新增 base-clang 与 win-ucrt64-clang-debug/release、win-clang64-debug（含 build/test 预设）；05 增补多环境隔离规则与预设表、更新本机基线；02 冻结清单补 Clang 证据。
- 验证：V1 Clang(UCRT64) 构建+cTest 3/3 通过；V2 GCC↔Clang 插件双向互载 exit 0；V3 CLANG64(libc++) 构建并运行 abi_layout 通过，全套 ctest 3/3 通过（test 预设自带 PATH）；V3b 附加实验：GCC 宿主加载 clang64(libc++) 插件成功，印证"纯 C ABI + 运行库可用"即可跨环境加载；V4 CLANG64+ASan 全套 ctest 3/3 通过；V5（负例：GCC 链 CLANG64 C++ 库应失败）留待 CI。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-msys2-multienv-toolchain-plan.md`

### V7 — 安装完整工具链并清理 Windows 侧重复工具链

- 类型：配置
- 文件：
  - `tools/env/packages.txt`
  - `docs/05-msys2-dev-environment.md`
- 摘要：按规划安装 M0 依赖：protobuf 35.1、gRPC 1.82、Qt6 6.11.1（base+tools，含 qmake6/windeployqt6）、Eigen3、python-pip、benchmark；清理重复/非必要项：clang-21 家族（clang/lld/llvm/compiler-rt，约 7.1 GiB）、MSYS 层重复 cmake 4.3.4 及其孤儿依赖（cppdap/libarchive/librhash/libuv/pkgconf/jsoncpp）；packages.txt 补 compiler-rt；05 基线更新。
- 验证：集成 smoke：临时工程同时 find_package Qt6/protobuf/gRPC/Eigen3 并链接运行通过（Qt 6.11.1、gRPC 1.82.0、Eigen trace 正确）；清理后四端 ctest 各 3/3 通过（GCC / Clang-UCRT64 / Clang-CLANG64 / MSVC 19.50）；schema 样例 10/10 通过；无孤儿包残留；ucrt64 3.55 GB、clang64 1.87 GB（清理前 8.97 GB）。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-msys2-multienv-toolchain-plan.md`

### V8 — schema 测试改用 MSYS2 UCRT64 Python 驱动

- 类型：配置
- 文件：
  - `CMakePresets.json`
  - `docs/05-msys2-dev-environment.md`
- 摘要：新增隐藏预设 win-python（Python3_EXECUTABLE=C:/msys64/ucrt64/bin/python.exe），所有 Windows 预设改为多重继承 [原有基类, win-python]，使 schema 样例校验由 pacman 管理的 UCRT64 Python + python-jsonschema 驱动，消除对系统 Windows Python 的依赖；Linux 预设仍用系统 python3 并在缺 jsonschema 时自动跳过。
- 验证：四个 Windows 预设的 CMakeCache 中 Python3_EXECUTABLE 均为 C:/msys64/ucrt64/bin/python.exe；ctest -V 显示测试命令使用该解释器并 10/10 通过；四端矩阵各 3/3 通过（GCC/Clang-UCRT64/Clang-CLANG64/MSVC）。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-msys2-multienv-toolchain-plan.md`

### V9 — 文档同步：多工具链状态、安装清单与 Qt 基线

- 类型：文档
- 文件：
  - `README.md`
  - `docs/01-architecture.md`
  - `docs/02-plugin-contract.md`
  - `docs/05-msys2-dev-environment.md`
  - `docs/06-packaging.md`
- 摘要：按当前实况同步文档：README 环境验证表改为四工具链（GCC/Clang-UCRT64/Clang-CLANG64/MSVC）与已装依赖（Qt 6.11.1、gRPC 1.82、protobuf 35.1、Eigen3、UCRT64 Python），新增 7.3 多工具链构建小节；05 安装命令补 clang/compiler-rt 并修正 catch 包名、补 qt6-tools；01 ADR A12 扩展为多工具链；02 布局条款补 Clang 验证证据；06 补 Qt 6.11.1 与 windeployqt6 基线。
- 验证：五个文档相对链接检查 0 失效；GCC 预设重建 + ctest 3/3 通过（文档改动无构建影响）。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-msys2-multienv-toolchain-plan.md`

### V10 — 规划：清理非 deepseek/kimi 的 AI Agent 入口配置

- 类型：文档
- 文件：
  - `deepseek/Logs/Plan/2026-10-22-agent-config-cleanup-plan.md`
- 摘要：盘点工程内 AI agent 配置：待删 CLAUDE.md/GEMINI.md/.cursorrules/.github/copilot-instructions.md 四个入口文件；保留 AGENTS.md、ai-models.json、tools/ai、.vscode 等通用设施与 deepseek/kimi 工作区；列出 README 与 AGENTS 的引用同步点；就 ai-models.json 是否裁剪提出 A/B 两方案（建议 A 保留注册表）。
- 验证：计划内相对链接全部有效；清单来自实测（根目录与 .github/.vscode 列表、全仓库 grep 引用点）；未执行删除。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-agent-config-cleanup-plan.md`

### V11 — 删除非 deepseek/kimi 的 AI Agent 入口配置（方案 A）

- 类型：删除
- 文件：
  - `CLAUDE.md`
  - `GEMINI.md`
  - `.cursorrules`
  - `.github/copilot-instructions.md`
  - `README.md`
  - `AGENTS.md`
- 摘要：按方案 A 删除 Claude/Gemini/Cursor/Copilot 四个转发入口文件（.github 目录随之消失）；保留 AGENTS.md、ai-models.json（全量注册表）、tools/ai、.vscode 等通用设施与 deepseek/kimi 工作区；README 结构图与入口清单、AGENTS §7 入口表同步更新。
- 验证：全仓库搜索无残留引用（仅历史日志与历史计划命中，按规则不追改）；ai-models.json 解析通过；Resolve-SrspAiModel 对 deepseek/kimi 正常；ctest 3/3 通过。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-agent-config-cleanup-plan.md`

## 2026-10-23

### V1 — 清理 Temp 探测残留并回填 M0 计划状态

- 类型：配置
- 文件：
  - `deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`
  - `deepseek/Temp/tmp/`
- 摘要：删除 deepseek/Temp/tmp 下三处探测残留（git-askpass、toolchain-probe、system-commandline-sentinel-files），tmp 占用归零；M0 执行计划由「规划中」改为「实施中」，回填 §1 现状（环境/契约已完成、内核/CLI/双传输/CI 待建）、§6 首批动作状态与执行记录。
- 验证：Temp/tmp 清空，业务缓存（build 30 MB、cache 9 MB）未动；计划文件状态与执行记录与实际进度一致；相对链接仍有效。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

### V2 — 完成 M0 阶段 1：ABI 版本协商测试与错误码表

- 类型：新增
- 文件：
  - `tests/unit/abi_negotiation.cpp`
  - `tests/unit/CMakeLists.txt`
  - `tests/CMakeLists.txt`
  - `api/error_codes.md`
  - `docs/02-plugin-contract.md`
- 摘要：T1.4：新增 tests/unit/abi_negotiation.cpp，覆盖四种拒绝/兼容路径（不支持主版本返回 NULL、缺 host 服务返回 SRSP_E_INVALID_ARGUMENT、截断 host 表仍可用、未知方法返回 SRSP_E_UNSUPPORTED），并接入 ctest；T1.5：新增 api/error_codes.md（12 个错误码的触发条件、责任方、处理建议、边车映射规则），docs/02 §4.5 引用、§12 冻结清单勾销两项。
- 验证：四套工具链各 4/4 通过（GCC、Clang-UCRT64、Clang-CLANG64、MSVC）；新测试在 GCC 与 MSVC 下均通过；docs/02 §12 现为 4/6 项完成（剩能力消息冻结、双传输 hello）。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

### V3 — 整理架构与数据交互总览文档

- 类型：文档
- 文件：
  - `deepseek/Logs/Plan/2026-10-23-architecture-summary.md`
- 摘要：依据 docs/01-07 与 api/ 现状输出一页式架构摘要：分层（L0-L3）与内核模块、宏步运行时与快照/背压、插件能力与生命周期、主机↔插件的 C ABI 与边车 gRPC 协议、插件间只经数据总线的交互（话题表与数据流）、标准数据格式（单位/坐标系/TAI 时间、SRM、Arrow 状态表、BlobRef、场景/渲染协议、传感器帧、项目文件、MCAP、版本演进），并附实现进度对照。
- 验证：计划内相对链接全部有效；内容与 docs/01-07、api/plugin_api.h、api/core_api.h、api/error_codes.md 一致；未改动代码。
- 关联计划：`deepseek/Logs/Plan/2026-10-23-architecture-summary.md`

### V4 — 调研机器人/空间环境/航天器仿真软件现状

- 类型：文档
- 文件：
  - `deepseek/Logs/Plan/2026-10-23-simulator-landscape.md`
- 摘要：按四类（通用机器人、空间机器人/OOS、轨道与 GNC、地球可视化）检索并整理 2026 年主流仿真软件：A 类 MuJoCo/MJX、Isaac Sim+Isaac Lab、Gazebo、Drake 等；B 类 Space ROS、mjorbit、OnOrbitROS；C 类 STK、GMAT、Orekit、Basilisk、Tudat、Nyx、FreeFlyer；给出横向对比矩阵与 Basilisk 对 GMAT/Orekit 的米级精度参考，结论是 SRSP 的差异化应压在确定性回放、空间环境+双原点可视化、能力寻址插件三点，而非 GPU-RL 吞吐。
- 验证：内部相对链接 0 失效；外部来源均给出可访问链接（arXiv 2609.08010、NASA NTRS、Isaac Lab 文档、Gazebo releases、Basilisk 精度对比页、Nyx 论文等）；未改动代码。
- 关联计划：`deepseek/Logs/Plan/2026-10-23-simulator-landscape.md`

### V5 — 按调研结论更新定位与技术选型，并提出规划优化方案

- 类型：文档
- 文件：
  - `docs/01-architecture.md`
  - `docs/04-subsystems-and-stack.md`
  - `docs/07-roadmap-risks.md`
  - `deepseek/Logs/Plan/2026-10-23-roadmap-optimization-plan.md`
- 摘要：执行调研建议：01 §1.3 非目标补「GPU 并行 RL 训练平台」；04 新增 §14 同类工具定位对比（四类代表、四条选型约束、三项差异化）；07 §5 新增 R15 竞品生态风险。另提出规划优化方案：M0 收缩（Qt 空壳移 M1）、M1 加固（可视化/录制/回放不可裁剪 + 新增 W7 互操作与基准）、M2 环境对标与容差表、M3 双轨接触与基准报告，附 D-13~D-16 决策项。
- 验证：四份文档内部相对链接 0 失效；未改动代码；ctest 状态未受影响（仅文档）。
- 关联计划：`deepseek/Logs/Plan/2026-10-23-roadmap-optimization-plan.md`

### V6 — 按规划优化方案同步文档：M0 收缩、M1 加固、容差分级

- 类型：文档
- 文件：
  - `docs/03-data-contracts.md`
  - `docs/07-roadmap-risks.md`
  - `deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`
  - `deepseek/Logs/Plan/2026-10-23-roadmap-optimization-plan.md`
- 摘要：落地 D-13~D-16：07 的 M0 去掉 Qt 空壳并加 tests/bench 骨架、M1 增加 Qt 空壳（renderer.null）、W7 互操作与对标脚本行、M2 增加容差表成文与环境对标改 Orekit/GMAT、前两周清单第 10 项纳入容差表；03 新增 2.5 精度与容差分级（L1 位级/L2 相对/L3 物理量级 + Basilisk 参考基线）；M0 计划阶段 5 标注移入 M1 并调整排期；优化方案状态改「已确认」并回填执行记录。
- 验证：四份文档内部相对链接 0 失效；容差数字与 Basilisk 官方对比页一致（0.02-1.2 m / 7-39 m / 17-72 m）；未改动代码。
- 关联计划：`deepseek/Logs/Plan/2026-10-23-roadmap-optimization-plan.md`

### V7 — 将竞品优势能力登记为预留扩展点（不进入 v1）

- 类型：文档
- 文件：
  - `docs/01-architecture.md`
  - `docs/02-plugin-contract.md`
  - `docs/04-subsystems-and-stack.md`
  - `docs/07-roadmap-risks.md`
  - `deepseek/Logs/Plan/2026-10-23-extension-roadmap.md`
- 摘要：把调研中 15 项竞品优势（GPU 批量并行、可微分物理、高保真接触、柔性/流体、传感器高保真、合成数据管线、Gym 训练环境、策略部署、任务级分析、多航天器、FSW/HIL、3D Tiles、影视级渲染、生态桥接等）登记为后续优化目标：07 新增 §11（E1-E6 扩展机制 + C1-C15 映射 + 触发条件），02 新增 §13（预留能力 ID 与 reserved 调用约定：未实现返回 SRSP_E_UNSUPPORTED），01 §1.3 非目标补互操作/批量接口预留，04 §14.3 指向预留路线。
- 验证：五份文档内部相对链接 0 失效；预留能力均为占位、无代码实现；现有 ctest 不受影响（未改代码）。
- 关联计划：`deepseek/Logs/Plan/2026-10-23-extension-roadmap.md`

### V8 — 规划通用 Agent 工作流与三项基础技能

- 类型：文档
- 文件：
  - `deepseek/Logs/Plan/2026-10-23-agent-workflow-skill-design.md`
- 摘要：设计模型无关的 Agent 配置：两条工作流（规划 planner / 执行 executor）、三个基础技能（module-build 含 Doxygen 强制规范、module-test 规定 <model>/ModuleTest/{unit,integration} 目录与用例要求、work-log 规定日志标准格式与提交/推送征询协议及多次未提交的编号批量提交规则）；给出落地步骤、演练判据、5 项决策（F-1~F-5）。
- 验证：计划内相对链接全部有效；方案为纯设计，未创建技能文件、未改代码；本次按新规则未执行 commit/push，等待用户同意。
- 关联计划：`deepseek/Logs/Plan/2026-10-23-agent-workflow-skill-design.md`

### V9 — 方案补充语言规则：注释与提交信息一律英文

- 类型：文档
- 文件：
  - `deepseek/Logs/Plan/2026-10-23-agent-workflow-skill-design.md`
- 摘要：按用户第 6 条需求更新 Agent 配置方案：Doxygen 与行内注释、CMake/YAML/Schema 描述、commit message 一律英文；Markdown 文档与对话保持中文。补充注释语言扫描验收（源码不出现 CJK）与决策 F-6，记录现状核对结果。
- 验证：现有源文件（api/include、examples）与 CMakeLists/CMakePresets/manifest schema 扫描均为纯英文，规则与现状一致无需返工；本次仍未提交，等待用户同意。
- 关联计划：`deepseek/Logs/Plan/2026-10-23-agent-workflow-skill-design.md`

### V10 — 落地通用 Agent 工作流与三项基础技能

- 类型：配置
- 文件：
  - `tools/ai/workflows/planner.md`
  - `tools/ai/workflows/executor.md`
  - `tools/ai/skills/module-build/SKILL.md`
  - `tools/ai/skills/module-test/SKILL.md`
  - `tools/ai/skills/work-log/SKILL.md`
  - `AGENTS.md`
  - `tools/ai/README.md`
- 摘要：按方案执行：新增规划与执行两条工作流（触发/步骤/门禁/产物/失败处理）；新增 module-build（Doxygen 强制、注释英文、构建与 CJK 扫描验收）、module-test（<model>/ModuleTest/{unit,integration}、用例与 CMake 接入规范）、work-log（日志格式、提交/推送征询、多次修改的编号批量提交）；AGENTS §4 补三条硬规则并新增 §8 工作流与技能索引；tools/ai/README 增加工作流与技能章节。
- 验证：5 个新文件与 2 处文档更新完成；AGENTS 6 个链接、tools/ai/README 15 个链接、新文件内部链接均 0 失效；未改动代码，构建与测试状态不变；按新规则本次未 commit/push，已向用户提交提交计划征询。
- 关联计划：`deepseek/Logs/Plan/2026-10-23-agent-workflow-skill-design.md`

### V11 — 工作日志技能增加临时操作权限申请协议

- 类型：文档
- 文件：
  - `tools/ai/skills/work-log/SKILL.md`
  - `AGENTS.md`
  - `tools/ai/README.md`
- 摘要：work-log 技能新增 §3 临时操作权限：区分 A 业务授权（commit/push）与 B 沙箱执行授权（单命令 danger-full-access 升级），给出申请模板、可行性边界（agent 不能自授权/不能持久化/审批 never 时自动拒绝）、四类阻塞与回退（sh.exe 管道、拒绝、无凭据、非快进）、五条禁止；AGENTS §4 与 tools/ai/README 同步引用。
- 验证：三份文件链接校验 0 失效；协议与 DSH 实际机制一致（本会话已实测：升级需用户确认、never 时拒绝、sh.exe 管道不可用需 GIT_ASKPASS）；未改动代码；按规则未提交，等待用户同意。
- 关联计划：`deepseek/Logs/Plan/2026-10-23-agent-workflow-skill-design.md`

### V12 — 工作日志技能增加计划文件 git 策略

- 类型：文档
- 文件：
  - `tools/ai/skills/work-log/SKILL.md`
  - `AGENTS.md`
  - `tools/ai/README.md`
- 摘要：work-log 新增 §3 计划文件 git 策略：Planning/Confirmed/In-progress 入库，已完成/已废弃用 git rm 移出版本库（历史保留），分析/调研/摘要类豁免；给出带锚点的判定命令（修正了'实施中（…已完成…）'被误判的问题）；AGENTS §2 与 tools/ai/README 同步引用。
- 验证：修正后的判定命令重扫 13 份计划：4 份命中（agent-workspace-config、srsp-architecture、agent-config-cleanup-plan、agent-workflow-skill-design），2 份实施中与 3 份知识资产正确保留；三份文档链接 0 失效；尚未执行 git rm，等待用户同意。
- 关联计划：`deepseek/Logs/Plan/2026-10-23-agent-workflow-skill-design.md`

### V13 — 按计划文件 git 策略移出 4 份已执行计划

- 类型：删除
- 文件：
  - `deepseek/Logs/Plan/2026-10-20-agent-workspace-config.md`
  - `deepseek/Logs/Plan/2026-10-20-srsp-architecture.md`
  - `deepseek/Logs/Plan/2026-10-22-agent-config-cleanup-plan.md`
  - `deepseek/Logs/Plan/2026-10-23-agent-workflow-skill-design.md`
- 摘要：按 work-log §3 计划文件 git 策略，将状态为「已完成」的 4 份可执行规划用 git rm 移出版本库；分析/调研/摘要类文档（architecture-summary、simulator-landscape、kimi-design-analysis、design-review）与未执行完的计划（M0、msys2 多环境、multilang、extension-roadmap、roadmap-optimization）保留。
- 验证：Plan 目录剩余 6 份计划 + README，分类与策略一致；被移出文件仍可通过 git log/git show 追溯；本次仅执行 git rm，未 commit/push（等待用户同意）。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

### V14 — 实现内核 PluginHost 与模块测试（M0 阶段 2 起步）

- 类型：新增
- 文件：
  - `src/core/include/srsp/core/plugin_host.hpp`
  - `src/core/src/plugin_host.cpp`
  - `src/core/CMakeLists.txt`
  - `CMakeLists.txt`
  - `deepseek/ModuleTest/CMakeLists.txt`
  - `deepseek/ModuleTest/unit/CMakeLists.txt`
  - `deepseek/ModuleTest/unit/plugin_host/CMakeLists.txt`
  - `deepseek/ModuleTest/unit/plugin_host/test_plugin_host.cpp`
  - `deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`
- 摘要：T2.1：新增 srsp::core::PluginHost（加载/ABI 主版本校验/vtable struct_size 校验/init/call/checkpoint/restore/shutdown/unload，英文 Doxygen），含 LoadLibrary/dlopen 双平台实现；顶层 CMake 增加 SRSP_BUILD_CORE 与 srsp_core 静态库；接入 <model>/ModuleTest 目录（仅 UCRT64 工具链启用 Catch2 模块测试）；新增 6 个单元用例覆盖未加载调用、缺库、加载初始化调用、重复加载、checkpoint/restore、卸载后拒绝。
- 验证：四端构建与测试：UCRT64 GCC 5/5、UCRT64 Clang 5/5、CLANG64 4/4、MSVC 4/4（模块测试按设计仅在 UCRT64 启用）；新增源码无 CJK（英文注释合规）；构建产物位于 deepseek/Temp/build/<preset>。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

### V15 — 实现内核 Registry 与 TimeService（M0 阶段 2 续）

- 类型：新增
- 文件：
  - `src/core/include/srsp/core/registry.hpp`
  - `src/core/src/registry.cpp`
  - `src/core/include/srsp/core/time_service.hpp`
  - `src/core/src/time_service.cpp`
  - `src/core/CMakeLists.txt`
  - `deepseek/ModuleTest/unit/registry/`
  - `deepseek/ModuleTest/unit/time_service/`
  - `deepseek/ModuleTest/unit/CMakeLists.txt`
  - `deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`
- 摘要：T2.2：Registry（插件表 + 能力表，按 name+major 解析、优先级高者胜、同优先级取先注册、注销时级联删除能力）；T2.3 时钟部分：TimeService（整数纳秒累加、暂停/恢复、步长校验、reset），为宏步调度打底；新增 12 个单元用例（registry 7、time 5）；英文 Doxygen 注释。
- 验证：UCRT64 GCC 7/7、UCRT64 Clang 7/7、CLANG64 4/4、MSVC 4/4（模块测试仅 UCRT64 启用）；新增源码 CJK 扫描为 0；构建产物在 deepseek/Temp/build/<preset>。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

### V16 — 实现 Diagnostics 与宏步调度骨架（M0 阶段 2 收尾）

- 类型：新增
- 文件：
  - `src/core/include/srsp/core/diagnostics.hpp`
  - `src/core/src/diagnostics.cpp`
  - `src/core/include/srsp/core/scheduler.hpp`
  - `src/core/src/scheduler.cpp`
  - `src/core/CMakeLists.txt`
  - `deepseek/ModuleTest/unit/diagnostics/`
  - `deepseek/ModuleTest/unit/scheduler/`
  - `deepseek/ModuleTest/unit/CMakeLists.txt`
- 摘要：T2.4：Diagnostics（severity/code/message/context、JSON 单行渲染含转义、容量上限、可选 sink）；T2.3 后半：Scheduler（固定步长宏步循环、暂停时发 debug 诊断、run(n) 返回实际执行步数）；新增 9 个用例（diagnostics 5、scheduler 4）。内核骨架 T2.1–T2.4 全部完成。
- 验证：UCRT64 GCC 9/9、UCRT64 Clang 9/9、CLANG64 4/4、MSVC 4/4；模块测试共 5 套件 27 个 TEST_CASE；新增源码 CJK 0；构建产物在 deepseek/Temp/build/<preset>。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

### V17 — 新增开发问题记录 ISSUES 与技能规范

- 类型：文档
- 文件：
  - `deepseek/Logs/Log/ISSUES.md`
  - `tools/ai/skills/work-log/SKILL.md`
  - `AGENTS.md`
  - `tools/ai/README.md`
- 摘要：新增 <model>/Logs/Log/ISSUES.md（与 CHANGELOG 同目录），按 P1..Pn 记录 2026-10-22/23 两天共 14 条开发问题（沙箱管道、包名、MSVC 抢占、跨工具链 CRT、Python 选择、变量污染、git 凭据/代理、提交信息拆分、BOM、扫描口径、Catch2 ABI、未构建测试注册、计划状态误判），每条含现象/根因/解决/验证；work-log 技能拆为 §2.1 变更日志与 §2.2 问题记录（触发条件、格式、必填项、复用规则）；AGENTS §3 与 tools/ai/README 同步。
- 验证：ISSUES.md 14 条与 CHANGELOG 历史一致；四份文档链接校验 0 失效；问题条目均来自本会话实测记录。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

## 2026-10-24

### V1 — 实现 srsp-cli 与 CLI 集成测试（M0 T4 起步）

- 类型：新增
- 文件：
  - `apps/CMakeLists.txt`
  - `apps/cli/CMakeLists.txt`
  - `apps/cli/main.cpp`
  - `CMakeLists.txt`
  - `deepseek/ModuleTest/CMakeLists.txt`
  - `deepseek/ModuleTest/integration/CMakeLists.txt`
  - `deepseek/ModuleTest/integration/cli/CMakeLists.txt`
  - `deepseek/Logs/Log/ISSUES.md`
- 摘要：新增 srsp-cli：version 与 demo --transport inproc --plugin <path> [--method]；sidecar 传输按 T3 预留并返回 SRSP_E_UNSUPPORTED（退出码 3），不伪装可用；顶层 CMake 增加 SRSP_BUILD_APPS 与 apps 子目录；新增 integration 测试层（不依赖 Catch2、全工具链可跑）：version、inproc 调用、未知方法失败、sidecar 预留共 4 个用例；ISSUES 补 2026-10-24 两条（uint8_t 指针条件表达式、PowerShell 引号拆分参数）。
- 验证：四端通过：UCRT64 GCC/Clang 各 13/13（4 契约 + 4 集成 + 5 单元套件），CLANG64/MSVC 各 8/8；新增源码无 CJK；CLI 实测输出被 PASS_REGULAR_EXPRESSION 匹配。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

### V2 — 将多机器人协同纳入 v1 并同步设计文档

- 类型：文档
- 文件：
  - `deepseek/Logs/Plan/2026-10-24-multi-robot-collaboration.md`
  - `docs/01-architecture.md`
  - `docs/02-plugin-contract.md`
  - `docs/03-data-contracts.md`
  - `docs/07-roadmap-risks.md`
- 摘要：按用户要求把多机器人协同从 C11 预留提升为 v1 交付：01 新增 6.6（按 robot_id 字典序的确定性调度、协同层在单体之后、链路消息排序、随机源派生、schedule_order 记录；链路模型与能力边界）并澄清 1.3 仅分布式集群为非目标；02 能力表新增 coordinator 与 link，预留表补 thermal；03 话题表新增 team.state/link.message、场景新增 teams[] 与 thermal 预留段；07 M2/M3 增加协同交付与验收、C11 更新、新增 C16 热网络预留与风险 R16。
- 验证：五份文档内部链接 0 失效；方案文档含范围界定/验收指标/风险；未改代码，构建状态不变。
- 关联计划：`deepseek/Logs/Plan/2026-10-24-multi-robot-collaboration.md`

### V3 — 实现 gRPC 边车桥接与双传输一致性测试（M0 T3 起步）

- 类型：新增
- 文件：
  - `src/bridge/CMakeLists.txt`
  - `src/bridge/include/srsp/bridge/sidecar_host.hpp`
  - `src/bridge/include/srsp/bridge/sidecar_server.hpp`
  - `src/bridge/src/sidecar_host.cpp`
  - `src/bridge/src/sidecar_server.cpp`
  - `CMakeLists.txt`
  - `deepseek/ModuleTest/integration/CMakeLists.txt`
  - `deepseek/ModuleTest/integration/sidecar/CMakeLists.txt`
  - `deepseek/ModuleTest/integration/sidecar/test_sidecar_roundtrip.cpp`
  - `deepseek/Logs/Log/ISSUES.md`
- 摘要：新增 srsp_proto（protoc 生成 C++ 与 gRPC 桩）与 srsp_bridge：SidecarServer 把进程内插件暴露为 PluginRuntime gRPC 服务（handshake token/ABI 校验、invoke、ping、checkpoint/restore、shutdown，AttachShm 返回 UNIMPLEMENTED），SidecarHost 为客户端（deadline、错误映射）；新增一致性测试：同一插件经 inproc 与 sidecar 调用返回逐字节一致；桥接层限定 UCRT64（CLANG64 会误抓 UCRT64 头文件，MSVC 无 gRPC）；ISSUES 补 P3–P5。
- 验证：四端：UCRT64 GCC 14/14、UCRT64 Clang 14/14、CLANG64 8/8、MSVC 8/8；一致性测试实测 inproc 与 sidecar 载荷相同；新增源码无 CJK。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

### V4 — CLI 支持 sidecar 传输并自动拉起边车进程（M0 T3.1）

- 类型：新增
- 文件：
  - `apps/sidecar/main.cpp`
  - `apps/sidecar/CMakeLists.txt`
  - `apps/CMakeLists.txt`
  - `apps/cli/main.cpp`
  - `apps/cli/CMakeLists.txt`
  - `CMakeLists.txt`
  - `src/bridge/include/srsp/bridge/sidecar_process.hpp`
  - `src/bridge/src/sidecar_process.cpp`
  - `src/bridge/CMakeLists.txt`
  - `deepseek/ModuleTest/integration/cli/CMakeLists.txt`
  - `deepseek/Logs/Log/ISSUES.md`
- 摘要：新增 srsp-sidecar（把插件暴露为 gRPC 服务，绑定临时端口并打印 READY <port>）与 SidecarProcess（Windows CreatePipe/CreateProcess、POSIX pipe/fork，带超时逐字节读取就绪行）；srsp-cli demo --transport sidecar 现自动拉起边车、握手、调用并回收进程；移除旧的 reserved 用例，新增 integration.cli_demo_sidecar（与 inproc 同断言）；顶层 CMake 顺序调整为 core→bridge→apps 以便 apps 判断目标；ISSUES 补 P6。
- 验证：四端：UCRT64 GCC 14/14、UCRT64 Clang 14/14、CLANG64 7/7、MSVC 7/7；sidecar 路径实测输出与 inproc 相同的载荷；无 CJK。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

### V5 — 新增 Python 边车与跨语言调用（M0 T3.2）

- 类型：新增
- 文件：
  - `tools/sidecar/srsp_sidecar.py`
  - `tools/sidecar/generated/srsp/v1/plugin_pb2.py`
  - `tools/sidecar/generated/srsp/v1/plugin_pb2_grpc.py`
  - `tools/sidecar/generated/srsp/__init__.py`
  - `tools/sidecar/generated/srsp/v1/__init__.py`
  - `apps/cli/main.cpp`
  - `deepseek/ModuleTest/integration/cli/CMakeLists.txt`
  - `deepseek/Logs/Log/ISSUES.md`
- 摘要：通过 pacman 安装 python-grpcio/grpcio-tools（1.82.0，规避 PEP 668），用 grpc_tools.protoc 生成 Python 桩；新增 tools/sidecar/srsp_sidecar.py：PluginRuntime 服务（token/ABI 握手、core.ping/describe/update、Ping、Checkpoint/Restore、Shutdown、AttachShm 返回 UNIMPLEMENTED），绑定临时端口后打印 READY；CLI 增加 --sidecar-script 以支持 python.exe + 脚本形式拉起；新增 integration.cli_demo_sidecar_python。
- 验证：四端：UCRT64 GCC 15/15、UCRT64 Clang 15/15、CLANG64 7/7、MSVC 7/7；Python 边车实测返回 lang=python；ISSUES 补 P7。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

### V6 — 审计 M0 计划与架构设计的一致性

- 类型：文档
- 文件：
  - `deepseek/Logs/Plan/2026-10-24-plan-vs-architecture-audit.md`
- 摘要：逐项对照 M0 执行计划与 01/02/03/05/07：结论为方向一致、7 处偏离。高：CLI 未经 core_api（A11 未验证）、运行时未读 manifest；中：selection 仅优先级、诊断缺 remedy；低：测试目录与计划不一致、工具链矩阵未反映桥接仅 UCRT64、内核为静态库未导出 C API。另列 4 项文档与进度不同步，并给出 P0–P2 修复建议。
- 验证：结论均附文件与行为证据（apps/cli/main.cpp 引用、srsp_core STATIC、Diagnostic 字段、02 §12 勾选状态、05 行 140 数字）；未改代码。
- 关联计划：`deepseek/Logs/Plan/2026-10-24-plan-vs-architecture-audit.md`

### V7 — 按审计结论修正文档：测试分层、能力矩阵、清单勾选

- 类型：文档
- 文件：
  - `docs/02-plugin-contract.md`
  - `docs/05-msys2-dev-environment.md`
  - `docs/07-roadmap-risks.md`
  - `deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`
- 摘要：P0 文档修正：(1) 02 新增 §10.3 测试分层（tests/ 契约测试 vs <model>/ModuleTest 开发期测试）并勾选双传输条目（5/6）；(2) 05 新增 §2.5 能力矩阵，写明四端测试数与桥接仅 UCRT64 的原因；(3) 07 M0 条目与 M0 计划 T3.4 改为引用 ModuleTest/integration；(4) 修复 M0 计划中指向已移出版本库的架构规划文件的失效链接。
- 验证：四份文档链接校验 0 失效（02:2、05:0、07:0、M0 计划:16）；02 §12 勾选 5/6；数字与四端实测一致（15/15/7/7）。
- 关联计划：`deepseek/Logs/Plan/2026-10-24-plan-vs-architecture-audit.md`

### V8 — 实现宿主侧 core_api 与 manifest 装载（审计 D1/D2）

- 类型：新增
- 文件：
  - `api/include/srsp/core_api.h`
  - `src/core/src/core_api.cpp`
  - `src/core/CMakeLists.txt`
  - `src/core/include/srsp/core/plugin_host.hpp`
  - `src/core/src/plugin_host.cpp`
  - `apps/cli/main.cpp`
  - `deepseek/ModuleTest/integration/host_api/test_host_api.c`
  - `deepseek/ModuleTest/integration/host_api/CMakeLists.txt`
  - `deepseek/ModuleTest/integration/CMakeLists.txt`
  - `deepseek/ModuleTest/unit/plugin_host/test_manifest_load.cpp`
  - `deepseek/ModuleTest/unit/plugin_host/CMakeLists.txt`
  - `src/bridge/CMakeLists.txt`
  - `deepseek/Logs/Log/ISSUES.md`
- 摘要：D1：新增 core_api 实现（context 创建/销毁、load_plugin、start/pause/step、time_tai_ns、plugin_call、buffer_release；project/snapshot/subscribe 明确返回 UNSUPPORTED），srsp_core 改共享库并拆出内部静态库 srsp_kernel，CLI 的 inproc 路径改走 C API；新增纯 C 语言的 host_api 集成测试。D2：PluginHost 新增 load_with_manifest（读 plugin.json 校验 id/api 主版本/平台库路径后再加载），nlohmann-json 按工具链启用，新增 manifest 单元测试。ISSUES 补 P8–P11。
- 验证：四端：UCRT64 GCC 17/17、UCRT64 Clang 17/17、CLANG64 8/8、MSVC 7/7（host_api 的 C 测试在 MSVC 跳过并说明原因）；C API 从 C 语言成功驱动内核；manifest 加载返回正确 plugin_id。
- 关联计划：`deepseek/Logs/Plan/2026-10-24-plan-vs-architecture-audit.md`

### V9 — 实现边车故障后单次重启（M0 T3.3）

- 类型：新增
- 文件：
  - `apps/cli/main.cpp`
  - `tools/sidecar/srsp_sidecar.py`
  - `deepseek/ModuleTest/integration/cli/CMakeLists.txt`
  - `docs/05-msys2-dev-environment.md`
- 摘要：CLI 的 sidecar 路径改为两段式：调用失败（握手或 Invoke 非 OK）时终止子进程、重新拉起、重连并重试一次，可用 --no-restart 关闭；Python 边车支持 SRSP_SIDECAR_FAULT=first-invoke 故障注入（首次 Invoke 返回 INTERNAL）；新增 integration.cli_demo_sidecar_restart 覆盖该路径；05 能力矩阵数字更新为 18/18/8/7（单元测试 30 例、集成测试 7/7/4/3）。
- 验证：四端：UCRT64 GCC 18/18、UCRT64 Clang 18/18、CLANG64 8/8、MSVC 7/7；重启用例实测在注入故障后成功（PASS 正则命中 restarting the sidecar once）。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`

### V10 — 冻结 core.ping 与 controller.update 消息并补 3 正例 3 反例（T1 收尾）

- 类型：新增
- 文件：
  - `api/schema/capabilities/controller.update.schema.json`
  - `api/schema/capabilities/core.ping.schema.json`
  - `api/schema/examples/capabilities/`
  - `tests/schema/validate_capability_examples.py`
  - `tests/CMakeLists.txt`
  - `docs/02-plugin-contract.md`
  - `docs/05-msys2-dev-environment.md`
- 摘要：T1 最后一项：为 core.ping 与 controller.update 定义请求/响应 JSON Schema（\.request/response，additionalProperties=false），各方向 3 正例 + 3 反例；新增 validate_capability_examples.py（强制每个方向 ≥3 正 3 反，正例必须通过、反例必须被拒），接入 ctest 为 schema_capability_examples；02 补消息冻结说明并勾选第 3 项（6/6），05 能力矩阵总数更新为 19/19/9/8。
- 验证：UCRT64 GCC/Clang 各 19/19、CLANG64 9/9；schema_capability_examples 通过，正反例按预期通过/被拒；02 §12 勾选 6/6。
- 关联计划：`deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md`
