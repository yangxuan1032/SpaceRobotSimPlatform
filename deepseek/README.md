# deepseek — DeepSeek 模型工作区

本目录是 **DeepSeek** 在本工程的独立运行空间，受根目录 [`AGENTS.md`](../AGENTS.md) 约束。
其他模型使用各自同级的 `<model>/` 目录（如 `claude/`、`gpt/`），互不混用。

别名：`deepseek`、`deepseek-flash`、`deepseek-official`、`deepseek-chat`、`deepseek-r1`、`deepseek-v3`、`deepseek-v3.1`（唯一真源见根目录 [`ai-models.json`](../ai-models.json)）。

```
deepseek/
├─ Temp/                 # 唯一缓存根（不纳入版本管理）
│  ├─ build/             # 构建目录，按 CMake preset 分目录
│  ├─ cache/             # ccache / pip / npm / pycache / xdg
│  └─ tmp/               # TEMP / TMP / TMPDIR
└─ Logs/
   ├─ Plan/              # 所有方案、规划、设计、路线图
   └─ Log/
      └─ CHANGELOG.md    # 唯一变更日志（按日期 + V1、V2…… 排序）
```

共享脚本位于 [`tools/ai/`](../tools/ai/README.md)，不放在本目录，避免每个模型各维护一份。

## 约定

1. **缓存**：所有临时文件、构建产物、下载与工具缓存都写入 `deepseek/Temp/`；可在任意时刻整体删除。
2. **计划**：每个方案一个文件，命名 `YYYY-MM-DD-<slug>.md`；同一方案迭代更新原文件，并在“修订记录”追加行。
3. **日志**：单文件 `deepseek/Logs/Log/CHANGELOG.md`；每条记录含日期（按日分组）、当天序号 `Vx`、类型、文件、摘要、验证、关联计划。
4. **可追溯**：计划状态与日志必须能对应到具体文件与验证方式；不接受“只改不记”。

## 快速使用

```powershell
# 0) 本机执行策略可能拦截本地脚本，仅对当前进程放开
Set-ExecutionPolicy -Scope Process Bypass -Force

# 1) 让当前 shell 的临时与缓存目录落到本目录
. .\tools\ai\Enter-AgentEnv.ps1 -Model deepseek

# 2) 新建计划
.\tools\ai\New-Plan.ps1 -Model deepseek -Title "..." -Slug "..." -Status planning

# 3) 追加日志
.\tools\ai\Append-ChangeLog.ps1 -Model deepseek `
  -Title "..." -Type modify -Files "path/a","path/b" `
  -Summary "..." -Verification "..."
```

## 与版本控制的关系

- `Temp/` 除 `README.md` 与 `.gitkeep` 外全部忽略；
- `Logs/` 纳入版本控制，作为交付物的一部分；
- 其他模型的 `Temp/` 同样被忽略，但其 `Logs/` 同样纳入版本控制。
