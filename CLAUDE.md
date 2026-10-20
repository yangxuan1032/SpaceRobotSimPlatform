# Claude 入口

本仓库的统一规则见 [`AGENTS.md`](AGENTS.md)，请先完整阅读。

- 若你以 **Claude** 运行：命名空间为 `claude`。
  - 缓存：`claude/Temp/`
  - 计划：`claude/Logs/Plan/`
  - 日志：`claude/Logs/Log/CHANGELOG.md`
  - 脚本：`tools/ai/*.ps1 -Model claude`
- 若你以其他模型运行：忽略上面的命名空间，按 `AGENTS.md` 与 `ai-models.json` 解析自己的命名空间。

不得写入其他模型的目录。
