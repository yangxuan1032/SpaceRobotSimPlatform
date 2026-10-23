# SRSP 错误码表（ABI 1.0）

`srsp_status` 的取值与语义，对应 `api/include/srsp/plugin_api.h` 中的枚举。
本表是 `srsp_error.code`、边车 gRPC `Error.code`（`api/proto/srsp/v1/plugin.proto`）
与诊断文案的共同依据（docs/02 §4.5）。

## 1. 总表

| 值 | 名称 | 含义 | 典型触发 | 责任方 | 建议处理 |
|---|---|---|---|---|---|
| 0 | `SRSP_OK` | 成功 | — | — | — |
| 1 | `SRSP_E_INVALID_ARGUMENT` | 参数非法 | 空指针、长度为 0 的必需字符串、越界句柄 | 调用方 | 修正调用；插件应校验后再使用 |
| 2 | `SRSP_E_UNSUPPORTED` | 不支持 | 未知方法名、不支持的配置项、未实现的可选能力 | 被调方 | 内核记录诊断并跳过；不视为故障 |
| 3 | `SRSP_E_NOT_READY` | 未就绪 | `init` 前调用、模型未 `load`、资源未惰性加载完成 | 被调方 | 调用方按生命周期重试；内核应保证顺序 |
| 4 | `SRSP_E_INVALID_STATE` | 状态非法 | `init` 两次、未暂停时重配、恢复状态与模型不匹配 | 调用方 | 内核修正调用序列；插件返回诊断 |
| 5 | `SRSP_E_NOT_FOUND` | 未找到 | 插件 ID、能力、文件、实体不存在 | 环境/配置 | 校验 manifest 与路径；给出可读定位信息 |
| 6 | `SRSP_E_TIMEOUT` | 超时 | 边车握手/心跳/调用超时 | 环境/对端 | 内核按 `fail_policy` 处理（重试或降级） |
| 7 | `SRSP_E_OUT_OF_MEMORY` | 内存不足 | `host->alloc` 失败、大缓冲申请失败 | 环境 | 减小批量或降级画质/精度；不得崩溃 |
| 8 | `SRSP_E_BUFFER_TOO_SMALL` | 缓冲区过小 | 调用方预分配缓冲不足 | 调用方 | 按返回所需大小重试（需插件回填所需尺寸） |
| 9 | `SRSP_E_VERSION_MISMATCH` | 版本不匹配 | ABI 主版本不同、schema 哈希不符、能力版本区间无解 | 配置/部署 | 拒绝加载并提示升级插件或平台（附期望/实际版本） |
| 10 | `SRSP_E_CANCELLED` | 已取消 | 用户中止、回放跳转、上层取消 | 调用方 | 静默清理，不记为故障 |
| 11 | `SRSP_E_INTERNAL` | 内部错误 | 不变量被破坏、未捕获异常已转为错误、断言失败 | 被调方 | 记录 `detail_json` 与步号；视为缺陷上报 |

## 2. 使用规则

1. **只增不改**：新增错误码只能追加值，不复用、不改变既有语义（与 ABI 演进规则一致）。
2. **区分责任方**：`INVALID_ARGUMENT` / `INVALID_STATE` / `BUFFER_TOO_SMALL` / `CANCELLED` 属于调用方问题，插件不得吞掉后返回 `OK`；其余多由被调方或环境引起。
3. **`srsp_error` 配套**：非 `OK` 时应填 `code`；`message` 面向用户（中文/英文均可，短句）；`detail_json` 放结构化上下文（字段名、期望值、实际值）。
4. **边车映射**：gRPC `Error.code` 直接使用本表数值；`detail_json` 透传为 `Error.detail_json`。
5. **不得用错误码表达控制流**：`UNSUPPORTED` 只用于「能力不存在」，不要用它表示「这一步暂时没数据」。

## 3. 与测试的对应

- `tests/unit/abi_negotiation.cpp` 覆盖 `SRSP_E_INVALID_ARGUMENT`（缺 host 服务）与 `SRSP_E_UNSUPPORTED`（未知方法）两条拒绝路径；
- 版本不匹配的拒绝（`srsp_plugin_query` 返回 NULL + 内核侧 `SRSP_E_VERSION_MISMATCH`）在同文件的第 1 项检查中覆盖。
