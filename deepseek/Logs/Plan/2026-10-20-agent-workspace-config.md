# 工作区独立 Agent 配置与缓存/计划/日志规范

- 日期：2026-10-20
- 状态：已完成
- 提出人：用户
- 关联文档：[AGENTS.md](../../../AGENTS.md)、[ai-models.json](../../../ai-models.json)、[tools/ai/README.md](../../../tools/ai/README.md)、[deepseek/README.md](../../README.md)、[变更日志](../../Logs/Log/CHANGELOG.md)

## 1. 需求

1. 所有相关缓存文件保存在本工程 `<model>/Temp/`（初始为 `deepseek/Temp/`）；
2. 所有用户提出的方案规划保存为文件，目录 `<model>/Logs/Plan/`；
3. 所有工程文件修改、优化操作记录为日志：显示修改日期（不显示具体时间），同日按先后顺序编号 `V1、V2……`，记录在单一文件中，目录 `<model>/Logs/Log/`。
4. 多个不同 AI 模型可能参与本工程；缓存、计划、日志要求相同，但各自使用本工程根目录下的 `<ai模型名称>/`。

## 2. 方案

### 2.1 配置入口（模型无关）

- 根目录 [`AGENTS.md`](../../../AGENTS.md) 是唯一规则真源，适用于 DeepSeek、Claude、GPT、Gemini、Qwen、GLM、Llama、Mistral、Grok、Kimi、Doubao、ERNIE 及后续模型。
- 不同工具通过各自的入口文件转发到 `AGENTS.md`：`CLAUDE.md`、`GEMINI.md`、`.cursorrules`、`.github/copilot-instructions.md`。
- 不再维护第二套规则副本；入口文件只说明“先读 AGENTS.md”与自身命名空间。

### 2.2 目录布局

```
<project-root>/
├─ AGENTS.md
├─ ai-models.json            # slug / display / aliases
├─ tools/ai/                 # 共享脚本、模板、labels.json
├─ deepseek/                 # DeepSeek 命名空间
│  ├─ Temp/                  # build、cache、tmp
│  └─ Logs/{Plan,Log}
├─ claude/                   # 其他模型按需创建
├─ gpt/
└─ ...
```

- 每个模型只写自己的 `<model>/`，不跨目录。
- 首次使用：`tools/ai/New-AiModelWorkspace.ps1 -Model <slug>`。

### 2.3 命名空间解析

- 唯一真源 `ai-models.json`，支持别名归一（`DeepSeek V3` → `deepseek`）。
- 解析顺序：`-Model` > `SRSP_AI_MODEL` > `AI_MODEL`；都未设置时脚本报错，不猜测。
- 旧 `deepseek/Log/Plan/` 已废弃，计划迁移到 `deepseek/Logs/Plan/`。

### 2.4 缓存策略

- `CMakePresets.json` 的 `binaryDir` 改为 `${sourceDir}/$env{SRSP_AI_MODEL}/Temp/build/<preset>`；
- 顶层 `CMakeLists.txt` 在 `SRSP_AI_MODEL` 未设置时直接报错，避免污染仓库根；
- `compile_commands.json` 与 clangd 索引目录使用 `${workspaceFolder}/${env:SRSP_AI_MODEL}/Temp/`；
- `tools/ai/Enter-AgentEnv.ps1` 注入 `TEMP/TMP/TMPDIR`、`CCACHE_DIR`、`PIP_CACHE_DIR`、`NPM_CONFIG_CACHE`、`PYTHONPYCACHEPREFIX`、`XDG_CACHE_HOME`；
- `.gitignore` 忽略 `/*/Temp/*`，仅保留 `.gitkeep` 与 `README.md`。

### 2.5 计划落盘

- 路径 `<model>/Logs/Plan/YYYY-MM-DD-<slug>.md`，同一方案迭代更新原文件；
- 共享模板 `tools/ai/templates/plan-template.md`，含日期、状态、提出人、关联文档、执行记录、修订记录；
- `tools/ai/New-Plan.ps1` 从模板生成，自动处理日期、重名与状态映射。

### 2.6 变更日志

- 单文件 `<model>/Logs/Log/CHANGELOG.md`，按 `## 日期` 分组；
- 同日条目 `### V1 — 标题` 递增编号，只写日期；
- 每条包含类型、文件、摘要、验证、关联计划；
- `tools/ai/Append-ChangeLog.ps1` 自动定位当天分组、计算下一个 `Vx`、插入记录，保证编号连续。

### 2.7 编码与执行策略

- 共享 PowerShell 脚本保持纯 ASCII，中文标签放 `tools/ai/labels.json`（UTF-8 数据），避免 Windows PowerShell 5.1 将无 BOM 的 UTF-8 脚本按 ANSI 解析；
- 本机执行策略为 `RemoteSigned`，脚本被拦截时仅对当前进程放开：`Set-ExecutionPolicy -Scope Process Bypass -Force`。

## 3. 交付物与验收

| 交付物 | 验收标准 | 状态 |
|---|---|---|
| `AGENTS.md` | 模型无关，覆盖三条强制规则与命名空间解析 | 已完成 |
| `ai-models.json` | 覆盖主流模型与别名，脚本可解析 | 已完成 |
| `tools/ai/` 脚本与模板 | PowerShell 5.1 可运行；多模型隔离测试通过 | 已验证 |
| `deepseek/` 命名空间 | 计划迁移到 `Logs/Plan`；日志连续 | 已完成 |
| 构建路径 | CMake/VSCode 按 `SRSP_AI_MODEL` 解析 | 已完成 |
| 其他工具入口 | `CLAUDE.md`、`GEMINI.md`、`.cursorrules`、Copilot 指令 | 已完成 |

## 4. 风险与待确认

| 项 | 说明 | 处理 |
|---|---|---|
| 模型自识别 | 工具未提供模型名时无法自动判定命名空间 | 强制 `-Model`/`SRSP_AI_MODEL`，未设置即报错 |
| 未知模型 | 不在注册表中的模型 | 使用 `agent` 或补充 `ai-models.json` |
| DSH 自身会话缓存 | 由应用管理，无法重定向 | 不在本配置范围；必要时配置 `DSH_HOME` |
| 多模型并发写日志 | 各自文件不同，互不冲突；同一模型多会话仍可能竞争 | 单模型单会话使用；后续可加文件锁 |
| 旧路径记忆 | 历史日志与文档可能残留旧路径 | 历史日志不追改；文档已同步，新日志说明迁移 |

## 5. 执行记录

| 日期 | 动作 | 结果 |
|---|---|---|
| 2026-10-20 | 建立 deepseek 单模型配置（V1–V4） | 已完成 |
| 2026-10-20 | 计划目录迁移到 `deepseek/Logs/Plan` | 已完成 |
| 2026-10-20 | 脚本迁移到 `tools/ai` 并支持 `-Model` | 已完成 |
| 2026-10-20 | AGENTS.md 改写为模型无关，新增各工具入口 | 已完成 |
| 2026-10-20 | CMake/VSCode 路径改为 `SRSP_AI_MODEL` 解析 | 已完成 |
| 2026-10-20 | 隔离目录测试 claude/gpt/gemini 命名空间 | 通过 |

## 6. 修订记录

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-10-20 | V1 | 初稿并实施（DeepSeek 单模型） |
| 2026-10-20 | V2 | 计划目录迁移到 `Logs/Plan`；脚本共享化；支持多模型命名空间 |
