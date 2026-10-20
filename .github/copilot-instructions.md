# Copilot instructions

Read [`AGENTS.md`](../AGENTS.md) first. It is the single source of truth for this workspace.

Determine the model family you are running as and use its namespace from [`ai-models.json`](../ai-models.json) (for example `gpt`). Never write caches, plans or logs into another model's directory. Pass `-Model <slug>` to every script under `tools/ai/`.
