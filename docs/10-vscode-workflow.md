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
## 可视化检查（2026-11-09）

三种启动方式，效果相同：

| 方式 | 操作 |
|---|---|
| 鼠标双击 | 仓库根 `VisualCheck.cmd` |
| VSCode 任务 | `Ctrl+Shift+P` → `Tasks: Run Task` → **SRSP: visual check (user inspection)** |
| 快捷键 | `Ctrl+Alt+V` |

脚本 `tools/visual/VisualCheck.ps1` 构建 `srsp-studio`，用 Qt 与 RHI 各渲染一次参考场景，把 PNG 写到 `<model>/Temp/visual/<时间戳>/`，并（默认）用系统看图器打开。

**人工检查要点**

| 后端 | 应看到 |
|---|---|
| Qt | 地球与明暗分界、黄色卫星（左）、旋转的青绿色立方体（右）、原点白色小球，以及三色坐标标记 |
| RHI | 同上；左上角另有管线内文字 `SRSP VISUAL CHECK`（B12），且该文字在 readback 中可见 |

**坐标轴标记**：红 = +X（右）、绿 = +Y（上）、蓝 = +Z（画面顶部），白色小球 = 世界原点；它们用于判断旋转方向与尺度，两个后端都应出现。

退出码：`0` 全部成功；`2` Qt 成功但 RHI 失败（机器无 D3D11 时属预期）；抛错表示构建或 Qt 快照失败。
### CI 中的可视化冒烟

`ctest -R visual` 会运行 `visual.snapshot_qt`：用 Qt 后端渲染参考场景并写出 PNG（`apps/studio/visual-qt.png`），断言标准输出包含 `visual snapshot:`。它不依赖 GPU，因此每个平台都能跑。GPU 变体为 `visual.snapshot_rhi`：具备 D3D11 时渲染并写出 `visual-rhi.png`，无 GPU 时以退出码 77 标记 SKIP（`SKIP_RETURN_CODE`），不会让 CI 变红。