# deepseek/Logs/Plan

保存 DeepSeek 会话提出或要求制定的所有方案、规划、设计与路线图。
其他模型使用各自的 `<model>/Logs/Plan/`，不在此目录写入。

## 命名

```
YYYY-MM-DD-<slug>.md
```

- 日期为计划建立日（本地时区）；
- `slug` 为小写 ASCII 与连字符，避免中文与空格；
- 同一方案迭代时更新原文件，不新建副本。

## 文件头

```yaml
- 日期：YYYY-MM-DD
- 状态：规划中 | 已确认 | 实施中 | 已完成 | 已废弃
- 提出人：用户
- 关联文档：
```

## 模板与创建

- 共享模板：[`tools/ai/templates/plan-template.md`](../../../tools/ai/templates/plan-template.md)
- 命令：

```powershell
.\tools\ai\New-Plan.ps1 -Model deepseek -Title "方案标题" -Slug "plan-slug" -Status planning
```

## 与日志的关系

计划进入实施或完成时，在 `deepseek/Logs/Log/CHANGELOG.md` 对应条目中填写“关联计划”，并在计划文末“修订记录”补一行。
