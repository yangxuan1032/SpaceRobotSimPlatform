# api/schema

数据模式的唯一真源。约定：

| 文件 | 模式 | 状态 |
|---|---|---|
| `plugin-manifest.schema.json` | `srsp.plugin/1` | ✅ v1 初稿 |
| `model/model_v1.schema.json` | `srsp.model/1` | ⏳ M1 |
| `state/state_v1.schema.json` | `srsp.state/1`（Arrow 表描述） | ⏳ M1 |
| `scene/scene_v1.schema.json` | `srsp.scene/1` | ⏳ M1 |
| `project/project_v1.schema.json` | `srsp.project/1` | ⏳ M1 |
| `scenario/scenario_v1.schema.json` | `srsp.scenario/1` | ⏳ M1 |

## registry.json

`registry.json` 由 `tools/codegen` 在构建时生成：遍历本目录模式文件，计算内容哈希
（xxh3-128），写入 `{id, file, hash, encoding}`。运行时内核据此校验插件声明的
`schema_hash`。

开发期可手写 `registry.json` 覆盖生成结果；提交前必须重新生成，保证哈希与文件一致。

## 校验

```bash
srsp-cli validate --schema api/schema/plugin-manifest.schema.json examples/hello_plugin/plugin.json
```

CI 会校验：所有 `*.json` 符合 2020-12 元模式、示例清单符合对应模式、文档中抽取的片段可解析。
