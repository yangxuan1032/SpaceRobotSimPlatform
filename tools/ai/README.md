# tools/ai — 模型无关的 Agent 工具

本目录是 SRSP 工作区所有 AI 模型共享的 agent 工具与模板。每个模型的数据仍保存在各自根目录：

```
<project-root>/<model>/
├─ Temp/                 # 缓存
└─ Logs/
   ├─ Plan/
   └─ Log/CHANGELOG.md
```

## 模型解析

唯一真源：根目录 [`ai-models.json`](../../ai-models.json)。

解析顺序：

1. 脚本参数 `-Model <slug>`
2. 环境变量 `SRSP_AI_MODEL`
3. 环境变量 `AI_MODEL`
4. 都未设置 → 报错（不猜测、不借用其他模型目录）

别名会归一，例如 `DeepSeek V3` → `deepseek`，`Anthropic` → `claude`。

## 脚本

| 脚本 | 作用 |
|---|---|
| [`Enter-AgentEnv.ps1`](Enter-AgentEnv.ps1) | 将 `TEMP/TMP/TMPDIR`、ccache、pip、npm、pycache、XDG 缓存指向 `<model>/Temp`，并导出 `SRSP_AI_MODEL` |
| [`New-AiModelWorkspace.ps1`](New-AiModelWorkspace.ps1) | 初始化某个模型的目录、README 与日志头 |
| [`New-Plan.ps1`](New-Plan.ps1) | 从模板创建 `<model>/Logs/Plan/YYYY-MM-DD-<slug>.md` |
| [`Append-ChangeLog.ps1`](Append-ChangeLog.ps1) | 向 `<model>/Logs/Log/CHANGELOG.md` 追加一条记录，自动计算当天 `Vx` |
| [`Resolve-AiModel.ps1`](Resolve-AiModel.ps1) | 被其他脚本点号引用的解析器与路径帮助函数 |

中文标签与状态映射在 [`labels.json`](labels.json)；脚本本身保持纯 ASCII，兼容 Windows PowerShell 5.1。

## 常用命令

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force

# 注入缓存环境（当前 shell）
. .\tools\ai\Enter-AgentEnv.ps1 -Model deepseek

# 新模型首次使用
.\tools\ai\New-AiModelWorkspace.ps1 -Model claude

# 新建计划
.\tools\ai\New-Plan.ps1 -Model deepseek -Title "架构规划" -Slug "srsp-architecture" -Status planning

# 追加日志
.\tools\ai\Append-ChangeLog.ps1 -Model deepseek `
  -Title "迁移计划目录" -Type refactor `
  -Files "deepseek/Logs/Plan","tools/ai" `
  -Summary "计划目录迁移到 Logs/Plan，脚本改为多模型共享" `
  -Verification "脚本在隔离目录测试通过"
```

## 新增模型

1. 在 `ai-models.json` 中补充 `slug`、`display`、`aliases`（可选）；
2. 执行 `.\tools\ai\New-AiModelWorkspace.ps1 -Model <slug>`；
3. 该模型之后所有命令都带 `-Model <slug>`，或在其会话中设置 `SRSP_AI_MODEL`。

## 模板

| 模板 | 用途 |
|---|---|
| [`templates/plan-template.md`](templates/plan-template.md) | 计划文件 |
| [`templates/changelog-header.md`](templates/changelog-header.md) | 新日志文件的文件头 |
| [`templates/model-README.md`](templates/model-README.md) | 新模型目录的 README |

## 约束

- 不写入其他模型目录；
- 共享脚本不写死具体模型名；
- 新增/修改脚本后，用 `-Root <临时目录>` 做一次隔离测试，避免污染真实日志。
