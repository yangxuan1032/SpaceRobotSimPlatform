# 插件清单样例（plugin.json examples）

对照 `../plugin-manifest.schema.json` 的校验样例，供 CI 与 `srsp-cli validate` 使用。
命名即期望：`valid-*` 必须通过校验，`invalid-*` 必须被拒绝。

| 文件 | 期望 | 覆盖点 |
|---|---|---|
| `valid-inproc-legacy-tags.json` | ✅ | 旧式 `os-arch` 库标签（向后兼容） |
| `valid-inproc-toolchain-tags.json` | ✅ | 新式 `os-arch-toolchain` 库标签（ucrt64/msvc/gcc） |
| `valid-multi-capability-mixed-tags.json` | ✅ | 多能力 + requires/optional_requires + 新旧标签混用（回退解析） |
| `valid-sidecar-python.json` | ✅ | sidecar + `${ENDPOINT}/${TOKEN}/${SHM}` 命令模板 |
| `valid-minimal.json` | ✅ | 仅必填字段 |
| `valid-renderer-null.json` | ✅ | 无头渲染器（CI/服务器用） |
| `invalid-unknown-toolchain-tag.json` | ❌ | 未注册的库标签 `windows-x86_64-gcc`（MinGW 应为 `windows-x86_64-ucrt64`） |
| `invalid-inproc-missing-library.json` | ❌ | `inproc` 缺少 `transport.library` |
| `invalid-sidecar-missing-command.json` | ❌ | `sidecar` 缺少 `transport.command` |
| `invalid-capability-name.json` | ❌ | 能力名不符合 `^srsp\.[a-z]+/[a-z_]+$` |

本地快速校验（需要 `pip install jsonschema`）：

```bash
python tools/ai/../../tests/schema/validate_manifest_examples.py
```
