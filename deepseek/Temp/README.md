# deepseek/Temp

DeepSeek 命名空间的缓存根，受 [`AGENTS.md`](../../AGENTS.md) 约束。内容可随时整体删除，不纳入版本管理。

| 子目录 | 用途 | 对应环境变量 |
|---|---|---|
| `tmp/` | 临时文件 | `TEMP`、`TMP`、`TMPDIR` |
| `cache/ccache/` | 编译缓存 | `CCACHE_DIR` |
| `cache/pip/` | Python 包缓存 | `PIP_CACHE_DIR` |
| `cache/npm/` | Node 包缓存 | `NPM_CONFIG_CACHE` |
| `cache/pycache/` | Python 字节码 | `PYTHONPYCACHEPREFIX` |
| `cache/xdg/` | XDG 缓存 | `XDG_CACHE_HOME` |
| `build/` | CMake 构建树 | `cmake --preset` 的 `binaryDir` |

快速注入（当前 shell 生效）：

```powershell
. .\tools\ai\Enter-AgentEnv.ps1 -Model deepseek
```

其他模型把 `-Model` 换成自己的 slug，缓存会落到其同名目录，例如 `claude/Temp/`。

注意：本目录只放“可丢弃”内容。需要保留的脚本、文档、计划与日志请放到 `tools/ai/`、`deepseek/Logs/Plan/`、`deepseek/Logs/Log/`。
