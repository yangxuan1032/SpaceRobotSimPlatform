# Gemini 入口

Read [`AGENTS.md`](AGENTS.md) first. It is the single source of truth for this workspace.

- If you run as **Gemini**: namespace is `gemini`.
  - cache: `gemini/Temp/`
  - plans: `gemini/Logs/Plan/`
  - log: `gemini/Logs/Log/CHANGELOG.md`
  - scripts: `tools/ai/*.ps1 -Model gemini`
- If you run as another model: ignore the namespace above and resolve yours through `AGENTS.md` and `ai-models.json`.

Never write into another model's directory.
