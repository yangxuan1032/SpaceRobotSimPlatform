# 方案：清理非 deepseek/kimi 的 AI Agent 入口配置

- 日期：2026-10-22
- 状态：已完成（按方案 A 执行：删除四个入口文件，保留注册表全量条目）
- 提出人：用户
- 关联文档：[AGENTS.md](../../../AGENTS.md)、[README](../../../README.md)、[ai-models.json](../../../ai-models.json)、[Agent 工作区配置计划](2026-10-20-agent-workspace-config.md)

## 1. 背景与目标

工程当前为「多模型 workspace」形态：`AGENTS.md` 是统一规则，另为 Claude / Gemini / Cursor / Copilot 各留了一个转发入口文件。用户要求：**删除除 deepseek、kimi 以外的 AI agent 配置文件；通用性配置保留**。

目标：清理入口文件，同时保证 `AGENTS.md`、`ai-models.json`、`tools/ai/*`、`.vscode/*` 这些通用设施不受影响，文档中的引用不出现悬空链接。

## 2. 现状清单与处置

| 文件 / 目录 | 性质 | 处置 |
|---|---|---|
| `CLAUDE.md`（472 B） | Claude Code 入口（转发 AGENTS.md） | **删除** |
| `GEMINI.md`（463 B） | Gemini CLI 入口 | **删除** |
| `.cursorrules`（353 B） | Cursor 入口 | **删除** |
| `.github/copilot-instructions.md` | GitHub Copilot 入口 | **删除**（`.github/` 变空目录，git 不跟踪空目录） |
| `AGENTS.md`（7.9 KB） | 全仓库统一规则（模型无关） | **保留**（通用） |
| `ai-models.json`（2.2 KB） | 模型命名空间注册表（`tools/ai` 的唯一真源） | **保留**，是否裁剪条目见 §6 |
| `tools/ai/*.ps1`、`templates/`、`labels.json` | 共享脚本与模板 | **保留**（通用） |
| `.vscode/{settings,launch,tasks,extensions}.json` | 编辑器/构建配置 | **保留**（`extensions.json` 未推荐 Copilot，无需改） |
| `deepseek/`、`kimi/` | 两个保留下来的模型工作区 | **保留** |
| `.clang-format`、`.editorconfig`、`.gitattributes`、`.gitignore`、`CMakeLists.txt`、`CMakePresets.json`、`vcpkg.json` | 通用工程配置 | **保留** |
| `docs/` | 设计文档 | **保留**（含对入口文件的少量提及，见 §5） |

已确认：仓库里没有 `.claude/`、`.gemini/`、`.cursor/`、`.aider*`、`.continue/` 等其他 AI 隐藏目录；模型命名空间目录也只有 `deepseek/`、`kimi/`。

## 3. 需要同步修改的引用

删除文件后，以下位置会指向不存在的文件，必须同步：

| 位置 | 现状 | 改法 |
|---|---|---|
| [README §5 仓库结构](../../../README.md) L83 | `├─ CLAUDE.md / GEMINI.md  # 其他模型工具入口` | 删除该行 |
| [README §6 入口文件](../../../README.md) L124 | 列出 AGENTS/CLAUDE/GEMINI/.cursorrules/Copilot | 改为只列 `AGENTS.md`（DSH/Codex 等直接读取） |
| [AGENTS §7 各工具入口](../../../AGENTS.md) L165–168 | Claude / Gemini / Cursor / Copilot 四行 | 删除四行，保留 Codex/DSH 一行，并补一句"其余工具若无入口文件，请直接读取 AGENTS.md" |
| [AGENTS §0 模型表](../../../AGENTS.md) L27 附近 | 列出 12 个模型与命名空间映射 | **不删**（见 §6 决策） |
| `deepseek/Logs/Log/CHANGELOG.md` L84–87 | 历史日志提到这些文件 | **不追改**（AGENTS 规则：历史日志保留当时路径） |
| `deepseek/Logs/Plan/2026-10-20-agent-workspace-config.md` L20/L82 | 历史计划记录 | **不追改**（历史记录），在新计划中说明 |
| `deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md` L15 | 「`.github` 下只有 Copilot 说明」 | 保留原文（当时事实），新日志中说明现状已变 |

## 4. 执行步骤

1. `git rm CLAUDE.md GEMINI.md .cursorrules .github/copilot-instructions.md`（用 `git rm` 保证索引同步）；
2. 改 README 两处、AGENTS §7 一处；
3. 校验：全仓库搜索 `CLAUDE.md|GEMINI.md|cursorrules|copilot` 应只剩历史日志/计划中的记录；
4. 运行一次 `ctest`（确认与构建无关）、`ai-models.json` 解析、`Resolve-SrspAiModel` 对 `deepseek`/`kimi` 仍正常；
5. 记 CHANGELOG（deepseek 命名空间）并提交。

## 5. 保留理由（防止误删）

- `AGENTS.md` 不是"某个 AI 的配置"，而是所有 agent 的共同规则（DSH/Codex 直接读取）；删了会导致无规则可依；
- `ai-models.json` 被 `tools/ai/Resolve-AiModel.ps1` 读取，决定 `<model>/Temp`、`Logs` 路径；删了 deepseek/kimi 的命名空间解析会失效；
- `tools/ai/*` 是 deepseek/kimi 两个工作区共用的脚本（`Enter-AgentEnv`、`New-Plan`、`Append-ChangeLog`）；
- `.vscode/*` 是通用开发配置，与 AI 无关。

## 6. 需要确认的一个决策：`ai-models.json` 是否裁剪

`ai-models.json` 当前登记 12 个模型（deepseek/claude/gpt/gemini/qwen/glm/llama/mistral/grok/kimi/doubao/ernie）+ ci/local。它是"通用的命名空间注册表"，但内容确实是其他模型的元数据。

| 方案 | 做法 | 优点 | 缺点 |
|---|---|---|---|
| **A（建议）** | 保留注册表全量条目，只删四个入口文件 | 保持"通用配置"定义；将来若加回某模型入口无需改注册表；`tools/ai` 行为不变 | 仓库里仍能看到其他模型的名字（仅元数据） |
| B | 注册表裁剪为 deepseek/kimi + ci/local，并同步精简 AGENTS §0 模型表 | 彻底只留两个模型 | 其他模型的 `-Model` 解析与别名归一失效；AGENTS 需改表格；将来恢复要再加回 |

**建议 A**：用户要求删除的是"agent 配置文件"（各工具入口），`ai-models.json` 属于通用基础设施。若目标是"仓库内不再出现其他模型名"，则选 B。

## 7. 风险与回滚

| 风险 | 说明 | 应对 |
|---|---|---|
| 误删通用文件 | 如误把 `AGENTS.md` 或 `ai-models.json` 列入删除 | 删除清单已显式限定 4 个文件；执行前 `git status` 复核 |
| 文档悬空链接 | README/AGENTS 仍指向被删文件 | §3 已列出全部引用点，改后做全文搜索校验 |
| 其他成员仍用 Claude/Cursor | 入口文件消失后工具读不到规则 | 入口删除后这些工具会直接读 `AGENTS.md`（多数工具支持），在 AGENTS §7 补一句说明 |
| 历史记录不一致 | 旧日志/计划提到已删文件 | 按规则不追改，新日志说明 |

## 8. 验收标准

- [ ] 4 个入口文件已删除且 `git status` 干净；
- [ ] 全仓库搜索 `CLAUDE.md|GEMINI.md|cursorrules|copilot` 仅命中历史日志/计划（无代码/配置引用）；
- [ ] `ai-models.json` 解析通过，`Resolve-SrspAiModel -Model deepseek|kimi` 正常；
- [ ] `ctest --preset win-ucrt64-debug` 3/3 通过；
- [ ] README/AGENTS 中不再有指向已删文件的链接。

## 9. 执行记录

| 日期 | 动作 | 结果 |
|---|---|---|
| 2026-10-22 | 盘点 AI agent 配置文件与引用点，形成方案 | 待确认（需先定 §6 方案 A/B） |
| 2026-10-22 | 用户选择方案 A；`git rm` 删除 `CLAUDE.md`、`GEMINI.md`、`.cursorrules`、`.github/copilot-instructions.md`（`.github/` 随之消失） | 4 个文件已删除 |
| 2026-10-22 | 同步 README（结构图、命名空间示例、入口清单）与 AGENTS §7（删除四行、补"只维护 AGENTS.md"说明） | 引用全部更新 |
| 2026-10-22 | 验证 | 全文搜索无残留引用（仅历史日志/计划命中）；`ai-models.json` 解析通过；`Resolve-SrspAiModel` 对 deepseek/kimi 正常；`ctest` 3/3 |

## 10. 修订记录

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-10-22 | V1 | 初稿：清单、引用同步、两套注册表方案、验收标准 |
| 2026-10-22 | V2 | 按方案 A 执行完成：四个入口文件已删、引用已同步、验证通过 |
