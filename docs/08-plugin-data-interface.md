# 插件数据交互接口

- 状态：v0.1（2026-11-01）
- 关联：`docs/02-plugin-contract.md`（ABI 与能力）、`docs/03-data-contracts.md`（数据契约）

本文说明插件在**数据面**上的交互方式：数据从哪来、用什么格式、何时失效。控制面（调用、事件）见 `docs/02`。

## 1. 三种交互通道

| 通道 | 传输 | 使用场景 | 生命周期 |
|---|---|---|---|
| 调用参数/返回值 | `srsp_plugin_call` 的 `srsp_bytes` | 小消息（< 64 KiB） | 调用期间 |
| BlobRef | `BlobRef{id, generation, size, flags}` + `BlobStore` | 图像、点云、大块状态 | 引用有效直到 store 释放或 generation 变更 |
| 录制 | MCAP 通道 + Attachment | 离线回放、数据导出 | 文件生命周期 |

选择规则：**超过 64 KiB 一律走 BlobRef**，不要塞进调用参数。

## 2. 状态数据的线格式

| 项 | 格式 |
|---|---|
| 通道 | `sim/<world>/state`，encoding `raw-f64` |
| 布局 | `[pos_w(3), quat_wb(4), vel_w(3), omega_b(3), …]` 按 body 顺序拼接 |
| 单位 | SI（m, rad, s, kg）；时间 `TAI ns` |
| 四元数 | `[x, y, z, w]`，body→world |
| 角速度 | **体系**（`omega_b`）；求解器内部世界系，边界转换（`docs/03` §4） |
| 字节序 | 小端 |

插件若输出自己的状态，必须声明约定（`ModelInfo.omega_frame`）并自行转换，内核不做猜测。

## 3. BlobRef 交互

```text
producer                          consumer
  store(bytes) -> BlobRef
  send(ref) --------------------->  acquire(ref)
                                      └ 空 => 引用过期/未知，必须报错而非重试等待
  release(ref)                      用完释放 lease
```

规则：

1. `generation` 不匹配 → **视为错误**，不得回退到旧数据；
2. `release` 只影响 store 侧，已发出的 lease 仍可读（shared ownership）；
3. 录制时由 `BlobRecorder` 写 Attachment + Metadata，随后释放；
4. 跨进程（边车）由共享内存承载，磁盘上 Attachment 为准。

## 4. 事件与时间

| 项 | 规则 |
|---|---|
| 事件键 | `(t_tai_ns, priority, source_id, seq)`，严格全序 |
| 队列 | 有界；满时返回 `kRejectedFull`，**不静默丢弃** |
| 时间源 | 只用仿真时间；禁止墙钟进入确定路径 |
| 回放 | 按 `t_tai_ns` 对齐状态、引用与附件；缺失则告警跳过 |

## 5. 错误约定

| 情形 | 返回 |
|---|---|
| 能力不支持 | `SRSP_E_UNSUPPORTED` |
| 参数非法（含过期 BlobRef） | `SRSP_E_INVALID_ARGUMENT` |
| 版本不匹配 | `SRSP_E_VERSION_MISMATCH` |
| 内部失败 | `SRSP_E_INTERNAL` |

错误信息通过 `srsp_error.message` 返回 ASCII 文本，`detail_json` 可选（UTF-8）。

## 6. 最小示例

```cpp
// 生产者：把一帧深度图交给内核
const auto ref = store.store(depth_bytes);
call_plugin(ctx, "sensor.depth", wire(ref), &response, &err);
// 内核在录制或消费完成后释放
store.release(ref);

// 消费者：取用
if (auto lease = store.acquire(ref)) {
  consume(lease->data(), lease->size());
} else {
  report(SRSP_E_INVALID_ARGUMENT, "stale blob reference");
}
```

## 7. 禁止事项

1. 不通过调用参数传递 > 64 KiB 的载荷；
2. 不在插件内假设内核使用世界系或体系角速度——必须声明并转换；
3. 不依赖 `generation` 的数值大小做逻辑判断（只做相等比较）；
4. 不在事件回调里做阻塞 IO；大文件写入走录制通道。
