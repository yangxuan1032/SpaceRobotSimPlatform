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
