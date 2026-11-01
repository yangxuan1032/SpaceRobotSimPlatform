# 插件数据交互接口与实现规范

- 状态：v1.0（2026-11-01，重构）
- 关联：`docs/02-plugin-contract.md`（ABI/能力/信任级别）、`docs/03-data-contracts.md`（数据契约）、`docs/09-code-standards.md`（代码规范）、`ai-models.json`（命名空间）

本文是**插件与主机之间的数据交换规范**：谁把什么数据、用什么格式、在什么时机、以什么错误语义交给谁，并给出可直接照抄的插件代码骨架。

## 1. 角色与边界

```text
        ┌──────────── host（内核）────────────┐
        │ 调度 / 时钟 / 状态 / 录制 / 寻址     │
        └───┬───────────────┬────────────────┘
            │ C ABI 进程内   │ gRPC + 共享内存（边车）
        ┌───▼────┐     ┌────▼─────┐
        │ plugin │     │ sidecar  │──► plugin（T2 隔离）
        └────────┘     └──────────┘
```

| 角色 | 职责 | 禁止 |
|---|---|---|
| host | 时钟、调度、状态所有权、录制、能力寻址、错误聚合 | 解析插件私有格式；替插件做单位/坐标系猜测 |
| plugin | 声明能力、处理调用、产出数据、报告错误 | 直接写主机状态；跨调用持有主机指针 |
| sidecar | 进程隔离、协议编解码、生命周期托管 | 绕过信任级别；把 T2 数据注入宿主进程 |

## 2. 交换通道总表

| 通道 | 载体 | 容量 | 何时用 |
|---|---|---|---|
| 调用请求/响应 | `srsp_bytes`（长度前缀） | ≤ **64 KiB** | 命令、配置、小结果 |
| 配置 | `srsp_str config_json`（UTF-8 JSON） | ≤ 1 MiB | `init` 时的静态配置 |
| 状态通道 | `raw-f64` 数组 | 无上限（流式） | 每步状态，录制/可视化 |
| **BlobRef** | 28 字节引用 + `BlobStore` | 无上限 | 图像、点云、大块结果 |
| 事件 | 有界命令队列 | 队列容量 | 跨子系统时序动作 |
| 录制 | MCAP（Channel/Attachment/Metadata） | 文件 | 回放与导出 |

**硬规则**：任何超过 64 KiB 的载荷必须走 BlobRef 或录制通道；调用参数里塞大数组视为违规。

## 3. 按插件类型定义数据流

### 3.1 模型插件（`kind: "model"`）

| 方向 | 内容 | 格式 |
|---|---|---|
| 输入 | SRM 文档 | UTF-8 JSON（`schema: srsp.model/1`） |
| 输出 | 归一化模型（体/关节/惯量/执行器） | 主机构造 `RigidBodyDynamics` 所需结构 |
| 约束 | 关节 frame 遵循 `docs/03` §4.1.1；轴为单位向量 | — |

### 3.2 动力学插件（`kind: "dynamics"`）

| 方向 | 内容 | 格式 |
|---|---|---|
| 输入 | `q, v, tau, dt` | `raw-f64` 连续数组 |
| 输出 | `q', v'` + 可选 `mass/h` 诊断 | `raw-f64`；诊断走独立通道 |
| 约束 | 确定性；禁止墙钟/随机；`dt` 由主机给 | — |

### 3.3 传感器插件（`kind: "sensor"`）

| 方向 | 内容 | 格式 |
|---|---|---|
| 输入 | 时间 `t_tai_ns`、位姿、环境状态 | 状态通道切片 + 配置 JSON |
| 输出 | 图像/点云/IMU 读数 | 图像与点云走 **BlobRef**；IMU 走 `raw-f64` |
| 约束 | `rate_hz` 决定调用节奏；同一 `t` 只产出一次；噪声需可复现（seed 在配置中） | — |

### 3.4 控制器插件（`kind: "controller"`）

| 方向 | 内容 | 格式 |
|---|---|---|
| 输入 | 当前状态、参考轨迹、传感器摘要 | 状态通道 + 小消息 |
| 输出 | 力矩/速度指令 | `raw-f64`（每关节一个） |
| 约束 | 输出受 `max_effort` 限制；不得回写状态 | — |

### 3.5 渲染插件（`kind: "renderer"`）

| 方向 | 内容 | 格式 |
|---|---|---|
| 输入 | `Scene`（体、地球、相机） | 主机结构体 |
| 输出 | 帧缓冲或窗口呈现 | 主机读回，不经 ABI 传像素 |
| 约束 | 渲染确定性不做承诺（`docs/01` §6.3） | — |

### 3.6 录制插件（`kind: "recorder"`）

| 方向 | 内容 | 格式 |
|---|---|---|
| 输入 | 状态快照、BlobRef、事件 | 快照 lease + 引用 |
| 输出 | MCAP 文件 | Channel `raw-f64`、Attachment、Metadata、Summary |
| 约束 | 单文件 blob 上限 256 MiB / 4096 条；超限**拒绝并报错** | — |

## 4. 调用协议

```c
srsp_status srsp_plugin_call(srsp_context* ctx,   /* 主机上下文 */
                             srsp_str method,      /* 能力方法名，如 "dynamics.step" */
                             srsp_bytes request,   /* 长度前缀 + 载荷 */
                             srsp_buffer* response,/* 主机构造，插件无需释放 */
                             srsp_error* err);
```

| 规则 | 说明 |
|---|---|
| 方法名 | `能力.方法`，小写 ASCII；未知方法返回 `SRSP_E_UNSUPPORTED` |
| 载荷 | 长度前缀（`u32` 小端 + 字节），按方法约定解析 |
| 返回缓冲区 | 由主机 `alloc` 分配，插件只写；不必也不得释放 |
| 错误 | 填 `err->code` + `err->message`（ASCII）；`detail_json` 可选 |
| 重入 | 同一插件不得并发进入两次 `call`（主机保证串行） |
| 生命周期 | `init → call* → shutdown`；`shutdown` 幂等 |

## 5. 状态、Blob 与时间

| 项 | 规则 |
|---|---|
| 状态布局 | `[pos_w(3), quat_wb(4), vel_w(3), omega_b(3), ...]`，SI，小端，TAI ns |
| 角速度 | 系统一使用**体系**；插件须在 `ModelInfo.omega_frame` 声明，边界转换由主机做 |
| BlobRef | `id + generation`；`generation` 不符 = 错误，不得回退到旧数据 |
| lease | `acquire` 得到的 `shared_ptr` 保证字节存活；用完释放 |
| 事件键 | `(t_tai_ns, priority, source_id, seq)` 严格全序；队列满时返回拒绝，不静默丢弃 |
| 回放 | 按 `t_tai_ns` 对齐；附件缺失 → 告警跳过，不中断 |

## 6. 插件代码规范与例程

### 6.1 目录与命名

```text
plugins/<plugin-id>/
├─ CMakeLists.txt          # 只构建 MODULE 库，静态 CRT
├─ plugin.json             # 清单（能力、版本、信任级别、平台库路径）
├─ include/                # 可选：私有头
└─ src/plugin.cpp          # 入口实现
```

| 元素 | 规则 |
|---|---|
| 插件 id | 反向域名，小写：`org.example.imu` |
| 文件名 | snake_case |
| 注释 | **英文**，公共函数带 Doxygen |
| 导出符号 | 只导出 `srsp_plugin_descriptor` / `srsp_plugin_create`（按 ABI 版本），其余 `static` 或隐藏 |

### 6.2 清单（`plugin.json`）最小示例

```json
{
  "schema": "srsp.plugin/1",
  "id": "org.example.imu",
  "name": "Example IMU",
  "version": "1.0.0",
  "api": "1",
  "kind": "sensor",
  "trust_level": 1,
  "transport": { "library": { "windows-x86_64": "bin/example_imu.dll" } },
  "capabilities": [{ "id": "sensor.imu", "version": "1" }],
  "platforms": ["windows-x86_64"],
  "permissions": ["read:state"]
}
```

`trust_level`：0=T0 内置、1=T1 受管（可进程内）、2=T2 隔离（仅边车）、3=T3 开发（不得进发布）。

### 6.3 入口与能力声明

```c
/* Every exported symbol is documented and uses the C ABI types only. */
SRSP_EXPORT const srsp_plugin_descriptor* srsp_plugin_descriptor(void) {
  static const srsp_plugin_descriptor descriptor = {
      .struct_size = sizeof(srsp_plugin_descriptor),
      .abi_major = SRSP_ABI_MAJOR,
      .id = "org.example.imu",
      .kind = "sensor",
  };
  return &descriptor;
}
```

### 6.4 调用处理（含错误语义）

```c
SRSP_EXPORT srsp_status srsp_plugin_call(void* self, const srsp_host* host,
                                         srsp_str method, srsp_bytes request,
                                         srsp_buffer* response, srsp_error* err) {
  if (!strcmp(method.data, "sensor.read")) {
    const uint32_t needed = 6 * sizeof(double);
    double* out = (double*)host->alloc(needed);
    if (out == NULL) {
      err->code = SRSP_E_INTERNAL;
      err->message = "allocation failed";
      return SRSP_E_INTERNAL;
    }
    /* Fill six doubles, then hand ownership to the host. */
    response->data = out;
    response->size = needed;
    return SRSP_OK;
  }
  err->code = SRSP_E_UNSUPPORTED;
  err->message = "unknown method";
  return SRSP_E_UNSUPPORTED;
}
```

### 6.5 大载荷（Blob）用法

```c
/* Never copy megabytes through srsp_bytes: store once, pass the reference. */
BlobRef ref = blob_store->store(image_bytes, image_size);
if (!send_blob_ref(ctx, ref, t_tai_ns)) {
  blob_store->release(ref);            /* failure: do not leak */
  return SRSP_E_INTERNAL;
}
/* Host releases after recording or consumption. */
```

### 6.6 必须遵守的六条

1. 不直接写主机状态；只通过调用返回或事件提交；
2. 不保存主机指针到 `shutdown` 之后；
3. 不在确定性路径使用墙钟、`random_device`、未固定 seed；
4. 超过 64 KiB 一律走 BlobRef；
5. 错误必须填 `code` + `message`，不得只返回裸状态；
6. `shutdown` 幂等，重复调用不得崩溃。

## 7. 与主机侧的对应关系

| 主机实现 | 承担的规则 |
|---|---|
| `PluginHost::load_with_manifest` | 校验 `api` 主版本、`trust_level`、平台库路径 |
| `trust.hpp` | T2/T3 拒绝进程内加载 |
| `snapshot.hpp` | 快照 `step_index + generation`，lease 读一致性 |
| `event.hpp` | 事件键全序、有界队列、拒绝计数 |
| `blob.hpp` / `blob_recorder.hpp` | BlobRef 生命周期、附件与 metadata 落盘 |
| `mcap.cpp` | Channel/Attachment/Summary 写入与回读 |

## 8. 验收清单（新插件并入前）

- [ ] `plugin.json` 通过 schema 校验（`ctest -R docs_check`）
- [ ] `api` 主版本与宿主一致
- [ ] 每个 method 有正常路径与错误路径测试
- [ ] 大载荷走 BlobRef，无 > 64 KiB 调用参数
- [ ] `shutdown` 幂等测试
- [ ] 注释为英文且公共符号有 Doxygen
