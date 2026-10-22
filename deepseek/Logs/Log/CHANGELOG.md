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
