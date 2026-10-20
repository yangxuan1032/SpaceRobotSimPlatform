# {{MODEL}} — AI 模型工作区

本目录是 **{{MODEL}}** 在本工程的独立运行空间，受根目录 `AGENTS.md` 约束。

- `Temp/`：缓存、构建、下载与临时文件（不纳入版本管理）
- `Logs/Plan/`：方案与规划
- `Logs/Log/CHANGELOG.md`：唯一变更日志

## 使用

```powershell
# 每个新的 PowerShell 进程
Set-ExecutionPolicy -Scope Process Bypass -Force
. .\tools\ai\Enter-AgentEnv.ps1 -Model {{MODEL}}

# 新建计划
.\tools\ai\New-Plan.ps1 -Model {{MODEL}} -Title "..." -Slug "..."

# 追加日志
.\tools\ai\Append-ChangeLog.ps1 -Model {{MODEL}} `
  -Title "..." -Type modify -Files "a","b" -Summary "..." -Verification "..."
```

## 约束

- 只在本目录内写缓存、计划与日志；不要写入其他模型目录。
- 目录缺失时可重新执行：`.\tools\ai\New-AiModelWorkspace.ps1 -Model {{MODEL}}`。
- 命名空间与别名的唯一真源是根目录 `ai-models.json`。

建立日期：{{DATE}}
