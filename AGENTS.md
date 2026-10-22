# SRSP 工作区 AI Agent 规则（模型无关）

本文件是 SRSP 工作区的统一 agent 契约，适用于所有可能参与本工程开发的 AI 模型与工具：DeepSeek、Claude、GPT、Gemini、Qwen、GLM、Llama、Mistral、Grok、Kimi、Doubao、ERNIE，以及后续新增的模型。

**优先级**：系统/开发者指令 > 用户当轮直接指令 > 本文件。若冲突，以更高优先级为准，并在回复中说明。

**工作区根**：`C:\UserProjects\SpaceRobotSimPlatform`。下文路径均相对此根；用户写的 `/xxx` 按工作区相对路径解释。

---

## 0. 先确定你的命名空间

每个模型只在**自己的根目录**下写缓存、计划与日志：

```
<project-root>/<model>/
├─ Temp/                 # 缓存、构建、下载、临时文件
└─ Logs/
   ├─ Plan/              # 方案与规划
   └─ Log/CHANGELOG.md   # 唯一变更日志
```

| 模型/工具 | 命名空间（slug） |
|---|---|
| DeepSeek | `deepseek` |
| Claude / Anthropic | `claude` |
| GPT / OpenAI / Codex / Copilot | `gpt` |
| Gemini | `gemini` |
| Qwen / 通义千问 | `qwen` |
| GLM / 智谱 | `glm` |
| Llama | `llama` |
| Mistral | `mistral` |
| Grok | `grok` |
| Kimi / Moonshot | `kimi` |
| Doubao / 豆包 | `doubao` |
| ERNIE / 文心 | `ernie` |
| 其他/未知模型 | `agent` |

规则：

- 唯一真源是根目录 [`ai-models.json`](ai-models.json)，支持别名归一（如 `DeepSeek V3` → `deepseek`）。
- 解析顺序：脚本参数 `-Model` > 环境变量 `SRSP_AI_MODEL` > `AI_MODEL`。**都未设置时脚本会报错——不要猜、不要借用别的模型目录。**
- 首次使用某个命名空间：`.\tools\ai\New-AiModelWorkspace.ps1 -Model <slug>`。
- 不得写入其他模型目录；如需参考其他模型的计划或日志，只读并在回复中注明来源。
- 未知模型先使用 `agent`，并建议在 `ai-models.json` 中补充正式 slug 与别名。

示例：DeepSeek 会话使用 `deepseek/`，Claude 会话使用 `claude/`，GPT 会话使用 `gpt/`。

---

## 1. 缓存与临时文件（强制）

- **唯一缓存根**：`<model>/Temp/`。
- 适用：构建目录、ccache、pip/npm 缓存、下载、截图、探测脚本、临时导出、测试输出、日志草稿、一次性生成物。
- 构建目录：`<model>/Temp/build/<preset>`；`compile_commands.json`：`<model>/Temp/compile_commands.json`。
- 每个**新的 shell 进程**先执行（点号引用，使变量留在当前作用域）：

  ```powershell
  Set-ExecutionPolicy -Scope Process Bypass -Force
  . .\tools\ai\Enter-AgentEnv.ps1 -Model <slug>
  ```

  脚本会设置 `SRSP_AI_MODEL`、`SRSP_AI_HOME`、`TEMP/TMP/TMPDIR`、`PIP_CACHE_DIR`、`CCACHE_DIR`、`NPM_CONFIG_CACHE`、`PYTHONPYCACHEPREFIX`、`XDG_CACHE_HOME`。
- CMake 预设通过 `SRSP_AI_MODEL` 选择构建目录；未设置时顶层 `CMakeLists.txt` 会直接报错，避免污染仓库根。
- 禁止把缓存写到仓库根、`docs/`、`api/`、用户目录或系统 `%TEMP%`；若外部工具无法重定向，必须在日志中记录原因与路径。
- `<model>/Temp/` 可随时整体删除，不纳入版本管理；需要保留的成果不要放进去。

---

## 2. 方案与规划落盘（强制）

- 用户提出或要求制定的**方案、规划、设计、架构决策、路线图**，必须在回复前保存为：

  ```
  <model>/Logs/Plan/YYYY-MM-DD-<slug>.md
  ```

  `slug` 使用小写 ASCII 与连字符（如 `srsp-architecture`）。
- 同一方案的迭代：**更新原文件**，在文末“修订记录”追加一行，不新建重复文件。
- 文件头必须包含：日期、状态（规划中/已确认/实施中/已完成/已废弃）、提出人、关联文档。
- 统一模板：`tools/ai/templates/plan-template.md`；新建命令：

  ```powershell
  .\tools\ai\New-Plan.ps1 -Model <slug> -Title "方案标题" -Slug "plan-slug" -Status planning
  ```

- 计划进入实施或完成后，同步更新状态与“执行记录”，保持计划与代码一致。
- 旧目录 `deepseek/Log/Plan/` 已废弃，不得再写入；历史计划已迁移到 `deepseek/Logs/Plan/`。

---

## 3. 变更日志（强制）

- **唯一日志文件**：`<model>/Logs/Log/CHANGELOG.md`（单文件，不拆分）。
- 触发条件：创建、修改、删除、重命名、移动工程文件，或调整构建、配置、脚本、文档结构。
- 记录粒度：一次任务一条；**同一天按先后顺序编号 V1、V2……**；只记录日期，不记录具体时间；跨天重新从 V1 开始。
- 追加命令（优先使用）：

  ```powershell
  .\tools\ai\Append-ChangeLog.ps1 -Model <slug> `
    -Title "一句话说明" -Type modify `
    -Files "docs/01-architecture.md","CMakePresets.json" `
    -Summary "改了什么、为什么" `
    -Verification "如何验证、结果" `
    -Plan "<model>/Logs/Plan/2026-10-20-xxx.md"
  ```

- 手工追加时必须遵守同样结构，并保证当天编号连续递增、序号不重复。
- 日志条目不得省略“文件 / 摘要 / 验证”三项；确实无法验证时写“未验证”并说明原因。
- 迁移或重构目录时，历史日志条目保留当时的路径，不追改；在新条目中说明迁移。

---

## 4. 工作方式

- 小步修改，优先用 `edit` 做定点替换，避免无关的整文件重写与格式化噪音。
- 不删除用户内容；确需删除或覆盖时，在日志中说明原因。
- 验证优先级：单元测试/构建 > JSON/YAML/脚本解析 > 链接与路径检查 > 人工核对。
- 不伪造验证结果；受环境限制无法验证时如实说明（例如沙箱禁止命名管道导致 MSYS2 bash 无法启动）。
- 中文用于文档与交流，代码标识符、提交信息、文件名用英文。
- 共享 PowerShell 脚本保持 **ASCII 源码**，中文文案放在 `tools/ai/labels.json`（UTF-8 数据文件）；避免 Windows PowerShell 5.1 把无 BOM 的 UTF-8 源码按 ANSI 解析。
- 执行策略拦截脚本时，仅对当前进程放开（不修改系统设置）：

  ```powershell
  Set-ExecutionPolicy -Scope Process Bypass -Force
  ```

---

## 5. 项目上下文（SRSP）

- 定位：空间机器人可视化仿真平台，**平台 + 插件**架构。
- 技术基线：C++20 内核 + 纯 C ABI 插件；跨语言插件走 gRPC + 共享内存边车；控制面 Protobuf，数据面 Arrow；模型 IR 为 SRM，场景为 glTF；录制为 MCAP。
- 构建与开发：Windows 用 MSYS2 UCRT64 + CMake Presets + Ninja；Linux 共用同一套 CMake 配置。
- 文档索引：`README.md`、`docs/01-architecture.md` 至 `docs/07-roadmap-risks.md`。
- 当前阶段：架构基线 v0.1，M0 之前；开放决策见 `docs/07-roadmap-risks.md` §10 与 `docs/03-data-contracts.md` §12。

---

## 6. 快速命令（以 deepseek 为例）

```powershell
# 每个新的 PowerShell 进程
Set-ExecutionPolicy -Scope Process Bypass -Force
. .\tools\ai\Enter-AgentEnv.ps1 -Model deepseek

# 新模型首次使用
.\tools\ai\New-AiModelWorkspace.ps1 -Model claude

# 新建计划
.\tools\ai\New-Plan.ps1 -Model deepseek -Title "..." -Slug "..." -Status planning

# 追加变更日志
.\tools\ai\Append-ChangeLog.ps1 -Model deepseek `
  -Title "..." -Type modify -Files "a","b" -Summary "..." -Verification "..."
```

---

## 7. 各工具的入口

| 工具/模型 | 入口文件 | 说明 |
|---|---|---|
| Codex、DSH、多数 agent | `AGENTS.md` | 本文件，直接生效 |

仓库只维护 `AGENTS.md` 一个入口（2026-10-22 起移除 Claude / Gemini / Cursor / Copilot 的转发文件）。没有专用入口文件的工具直接读取本文件；命名空间仍按 `ai-models.json` 解析，当前实际使用的命名空间为 `deepseek` 与 `kimi`。

若本文件与更高优先级的系统/开发者指令冲突，以后者为准。
