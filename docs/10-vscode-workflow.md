# VSCode 开发工作流

- 状态：v0.1（2026-11-01）
- 关联：`.vscode/tasks.json`、`docs/05-msys2-dev-environment.md`

目标：**一个快捷键看到当前开发进度**——构建 + 全量测试一次完成。

## 1. 任务一览

按 `Ctrl+Shift+P` → `Tasks: Run Task`，或直接使用快捷键。

| 任务 | 快捷键 | 作用 |
|---|---|---|
| `CMake: configure` | — | 选择 preset 并配置到 `<model>/Temp/build/<preset>` |
| `CMake: build` | **Ctrl+Shift+B** | 增量构建（默认构建任务） |
| `SRSP: build + test (dev loop)` | — | **构建 + 全量 ctest**，一条命令看进度 |
| `CMake: test` | — | 只跑测试，不构建 |
| `Docs: check (json/yaml/links)` | — | 文档/JSON/schema 哈希门禁 |
| `SRSP: bench dynamics` | — | 构建动力学基准 |
| `SRSP: studio (RHI viewport)` | — | 构建 studio（`--renderer rhi` 走 D3D11） |
| `SRSP: soak (1 minute smoke)` | — | 1 分钟录制长稳冒烟 |
| `Demo: hello plugin` / `Demo: srsp-cli (inproc/sidecar)` | — | 跑示例插件链路 |
| `Format: clang-format (C/C++)` | — | 格式化 C/C++（**不要**用于 CMake/YAML） |

## 2. 推荐使用方式

1. 首次：`CMake: configure` 选择 `win-ucrt64-debug`；
2. 日常：`Ctrl+Shift+B` 构建，再跑 `CMake: test`；
3. **汇报进度时**：跑 `SRSP: build + test (dev loop)`，把输出里的
   `tests passed, N tests failed out of M` 贴出来——这就是当前进度；
4. 改渲染/性能时：`SRSP: studio (RHI viewport)` 或 `SRSP: bench dynamics`。

## 3. 绑定自定义快捷键

`tasks.json` 的任务本身不能设快捷键，需要 `keybindings.json`（`Ctrl+K Ctrl+S` → 打开 keybindings JSON）：

```json
[
  {
    "key": "ctrl+alt+b",
    "command": "workbench.action.tasks.runTask",
    "args": "SRSP: build + test (dev loop)"
  }
]
```

## 4. preset 选择

任务通过 `inputs.preset` 选择构建类型，可选：

`win-ucrt64-debug`（默认）、`win-ucrt64-release`、`win-ucrt64-clang-debug`、
`win-clang64-debug`、`win-msvc-debug`、`linux-*`。

构建目录由 `SRSP_AI_MODEL` 决定（默认 `deepseek`），即
`<model>/Temp/build/<preset>`，不会污染仓库根。

## 5. 环境变量

首次使用前在终端执行一次：

```powershell
. .\tools\ai\Enter-AgentEnv.ps1 -Model deepseek
```

它会设置 `SRSP_AI_MODEL`、`TEMP`、缓存目录等；VSCode 任务里通过
`${env:SRSP_AI_MODEL}` 引用。

## 6. 判断进度的方法

| 观察点 | 含义 |
|---|---|
| `tests passed, 0 tests failed out of 44` | 全绿基线 |
| 失败用例名 | 定位到模块（如 `unit.dynamics_golden`） |
| `bench_dynamics` 输出 | 性能进度（目标 `step_us ≤ 300`） |
| `bench_rhi` 输出 | GPU 帧率进度 |
