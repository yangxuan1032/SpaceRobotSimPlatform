# glm — AI 模型工作区

本目录是 **GLM / Zhipu** 在本工程的独立运行空间，受根目录 [`AGENTS.md`](../AGENTS.md) 约束。
其他模型使用各自同级的 `<model>/` 目录（如 `deepseek/`、`kimi/`），互不混用。

别名：`glm`、`chatglm`、`zhipu`、`glm-4`、`glm-5`、`glm-5.3-flash`、`zai`、`智谱`（唯一真源见根目录 [`ai-models.json`](../ai-models.json)）。

- `Temp/`：缓存、构建、下载与临时文件（仅 `Temp/README.md` 与 `.gitkeep` 入库）
- `Logs/Plan/`：方案与规划（本地，不入库）
- `Logs/Log/CHANGELOG.md`：唯一变更日志（本地，不入库）

配置设计见 [`Logs/Plan/2026-11-05-glm-agent-config-design.md`](Logs/Plan/2026-11-05-glm-agent-config-design.md)：
GLM 不复制 agent 规则副本，只保留命名空间、别名注册与 `-Model glm` 入参。

## 使用

```powershell
# 每个新的 PowerShell 进程
Set-ExecutionPolicy -Scope Process Bypass -Force
. .\tools\ai\Enter-AgentEnv.ps1 -Model glm

# 新建计划
.\tools\ai\New-Plan.ps1 -Model glm -Title "..." -Slug "..."

# 追加日志
.\tools\ai\Append-ChangeLog.ps1 -Model glm `
  -Title "..." -Type modify -Files "a","b" -Summary "..." -Verification "..."
```

## 约束

- 只在本目录内写缓存、计划与日志；不要写入其他模型目录。
- 目录缺失时可重新执行：`.\tools\ai\New-AiModelWorkspace.ps1 -Model glm`。
- 命名空间与别名的唯一真源是根目录 `ai-models.json`。

建立日期：2026-11-05
