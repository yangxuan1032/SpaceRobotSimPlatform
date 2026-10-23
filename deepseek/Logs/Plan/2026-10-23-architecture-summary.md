# SRSP 架构与数据交互总览（v0.1 基线摘要）

- 日期：2026-10-23
- 状态：已完成（摘要文档，依据 docs/01–07 与 api/ 现状整理）
- 提出人：用户
- 关联文档：[01 架构](../../../docs/01-architecture.md)、[02 插件契约](../../../docs/02-plugin-contract.md)、[03 数据契约](../../../docs/03-data-contracts.md)、[04 技术栈](../../../docs/04-subsystems-and-stack.md)、[插件 API](../../../api/include/srsp/plugin_api.h)、[宿主 API](../../../api/include/srsp/core_api.h)、[错误码](../../../api/error_codes.md)

## 1. 一句话架构

**平台 + 插件**：内核（libsrsp）只管调度、注册、总线、时间、坐标系与插件生命周期，所有领域能力（模型、动力学、环境、传感器、渲染、控制、录制）都是插件；插件与内核只通过**版本化契约**耦合（进程内纯 C ABI / 进程间 gRPC），插件之间**从不直接调用**。

## 2. 分层与组件

```
L3 应用    srsp-studio(Qt) · srsp-cli · srsp-web · Python/Rust SDK
              │ core_api.h（宿主侧 C API）
L2 内核    Scheduler/Runtime · Registry · DataBus · FrameGraph
           TimeService · PluginHost · Snapshot/Replay · Diagnostics
              │ plugin_api.h（插件侧 C ABI）      ╲ gRPC + shm
L1 插件    进程内：model/dynamics/force/contact/    边车：pybridge /
           env/sensor/render/control/io/ui          unity / matlab / jvm
L0 契约    C ABI · Protobuf · JSON Schema · Arrow · glTF · MCAP
```

| 层 | 职责 | 关键点 |
|---|---|---|
| L0 | 契约唯一真源 | 只增不改、结构体 `struct_size` 演进、schema 哈希校验 |
| L1 | 领域能力实现 | 进程内限 C/C++/Rust；其他语言一律边车 |
| L2 | 编排与运行时 | 不含领域算法；负责顺序、时间、快照、故障处理 |
| L3 | 人机/自动化入口 | 通过 `core_api.h` 驱动内核，不直接碰插件 |

## 3. 运行时模型

- **进程**：v1 单进程（UI+内核+默认插件同进程），但接口按「可跨进程」设计，后续可拆 `srspd`。
- **线程**：Sim 线程（确定性步进）、渲染线程（消费快照、允许丢帧）、IO/录制线程（非阻塞队列）、边车监督线程（心跳/重启）。
- **宏步循环**（默认 `dt = 1 ms`，可配 0.1–10 ms）：

```
t_next = t + dt
events   = queue.pop_until(t_next)        # 排序键 (t, priority, seq)
dispatch(events)
sensors_due(t).sample()                   # 采样时刻 = t，发布到总线
controllers_due(t).update()               # 写 cmd.actuator（带有效期）
forces = Σ force_providers.accumulate()   # 顺序 = 注册序号
dynamics.step(t, dt, commands, forces)    # 插件内部可子步，对外仍一个宏步
t = t_next
snapshot = state.freeze()                 # 不可变快照（引用计数）
bus.publish("sim.state", snapshot)        # 渲染/录制异步消费
```

- **快照**：Sim 线程产出只读快照，渲染/录制/网络各持一份，避免锁。
- **背压**：每话题有界队列（默认 1024）；满时传感器/渲染 `drop_oldest`，命令在非实时模式 `block`；录制绝不阻塞 Sim。

## 4. 插件体系

**能力寻址而非插件 ID**：场景只声明能力需求（如 `srsp.dynamics/solver@>=1.0`），内核按「优先级 + 用户 pin」选实现，换求解器不改场景。

| 能力 | ID 形式 | 典型实现 |
|---|---|---|
| 模型导入 | `srsp.model/importer@1` | URDF/SDF/MJCF/CAD 转换 |
| 动力学 | `srsp.dynamics/solver@1` | 自研刚性多体、Pinocchio、MuJoCo |
| 力模型 / 接触 | `srsp.dynamics/force@1`、`contact@1` | 引力/SRP/大气/推力器；GJK+顺序冲量 |
| 空间环境 | `srsp.env/{ephemeris,gravity,atmosphere,lighting}@1` | CSPICE、EGM2008、NRLMSISE-00 |
| 传感器 | `srsp.sensor/device@1` | IMU/星敏/相机/LiDAR/GNSS/F-T |
| 渲染器 | `srsp.render/renderer@1` | bgfx（默认）/Qt Quick 3D/Unity |
| 控制器 | `srsp.control/controller@1` | PD/计算力矩/MPC（Python、MATLAB 边车） |
| 记录 | `srsp.io/recorder@1` | MCAP/Parquet |
| 界面/脚本 | `srsp.ui/panel@1`、`srsp.script/engine@1` | Qt 面板、Web 面板 |

**生命周期**：扫描目录 → schema 校验 → 平台/ABI 匹配 → 依赖解析（拓扑排序）→ 签名/信任检查 → `dlopen` 或启动边车 → `core.describe` 能力握手 → `core.configure` → Active；`shutdown` 逆序；崩溃插件进 `quarantined`，启动时跳过。

**清单（plugin.json）关键字段**：`id`（反向域名，发布后不变）、`version`（SemVer）、`api`（ABI 主版本）、`kind`、`transport{type, library|command, symbol}`、`capabilities[{name,version,alias}]`、`requires`/`optional_requires`、`platforms`、`permissions`、`config_schema`、`entry{init_timeout_ms, thread_safe}`。`transport.library` 的键是 `os-arch-toolchain`（旧 `os-arch` 作别名）。

## 5. 主机 ↔ 插件接口

### 5.1 进程内（纯 C ABI）

```c
const srsp_plugin_v1* srsp_plugin_query(uint32_t host_abi_major,
                                        const srsp_host_v1* host);
```

| 方向 | 结构 | 内容 |
|---|---|---|
| 宿主 → 插件 | `srsp_host_v1` | `log`、`time_tai`（double，遗留）、**`time_tai_ns`（int64，追加）**、`alloc/free`、`publish(topic,payload,schema_hash)`、`shm_map/unmap` |
| 插件 → 宿主 | `srsp_plugin_v1` | `id/version/self` + `init` / `call` / `checkpoint` / `restore` / `shutdown` |

约定：每个结构首字段 `uint32_t struct_size`、只允许尾部追加；UTF-8 长度前缀（非 NUL 结尾）；出参由 `host->alloc` 分配、调用方 `host->free`；禁止位域、枚举固定 32 位；异常不得跨边界；跨编译器布局由 `tests/abi` 断言保证。

**调用与错误**：`call(method, request_bytes) → response_buffer`，方法名是稳定字符串（`core.describe/ping/configure/checkpoint/restore/stats` 为保留方法）；失败返回 `srsp_status` 并填 `srsp_error{code,message,detail_json}`，语义见 [api/error_codes.md](../../../api/error_codes.md)。

**版本协商**：宿主传 `host_abi_major`；插件不支持则 `query` 返回 NULL，宿主报 `SRSP_E_VERSION_MISMATCH`（`tests/unit/abi_negotiation.cpp` 覆盖）。

### 5.2 进程间（边车）

- 启动：内核以 `python -m srsp_bridge --plugin <id> --endpoint 127.0.0.1:0 --token <一次性> --shm <段名> --parent-pid <pid>` 拉起；边车把 `{"endpoint","pid","api"}` 打到 stdout。
- 协议：`PluginRuntime` 七个 RPC —— `Handshake`（校验 token + manifest 摘要）、`AttachShm`、`Invoke`（与进程内 `call` 同语义）、`Checkpoint`/`Restore`、`Ping`（心跳，默认 1 s，连续 3 次失败判失联）、`Shutdown`（2 s 后杀进程树）。
- 数据面：>64 KiB 走共享内存 `BlobRef`（seqlock + generation），gRPC 只传引用；高频控制用 `step_batch(n)` 批量；目标单宏步往返 p99 ≤ 1 ms。
- 隔离：回环 TCP + 一次性 token；Windows Job Object / Linux cgroup 限制内存与 CPU；默认禁网。

## 6. 插件 ↔ 插件：只经内核

插件之间**没有直接调用通道**，两条合法路径：

1. **数据总线（话题）**——主要方式；
2. **能力查询**——插件向内核询问某能力的实现并拿到代理句柄（如相机传感器依赖渲染器）。

### 6.1 话题表（核心）

| 话题 | 方向 | 频率 | Schema |
|---|---|---|---|
| `sim.state` | 内核 → 全体 | 宏步 | `srsp.state/1`（BlobRef） |
| `sim.time` | 内核 → 全体 | 宏步 | `srsp.time/1` |
| `sim.event` | 内核 → 全体 | 事件 | `srsp.event/1`（碰撞/捕获/模式切换/故障） |
| `cmd.actuator` | 控制器 → 动力学 | 控制步 | `srsp.cmd/1`（带有效期） |
| `env.ephemeris` | 环境 → 全体 | 1–100 Hz | `srsp.ephem/1` |
| `sensor.<type>.<id>` | 传感器 → 消费者 | 按配置 | `srsp.sensor.*/1` |
| `render.scene` | 内核 → 渲染器 | 变更时 | `srsp.scene/1` |
| `diag.log` / `ui.command` | 全体 ↹ 内核 | 事件 | `srsp.diag/1`、`srsp.cmd.ui/1` |

投递语义：进程内同步回调（订阅顺序 = 注册序号）；跨进程至少一次（消费者幂等、带 `seq`）；不重放历史（需要历史用录制回放）。

### 6.2 一次数据流

```
SRM 模型 → model.importer → 内核只读副本
                                │
dynamics.solver ──state(Arrow)──▶ 内核总线 sim.state
      ▲                              ├──▶ sensor.* ──▶ 控制器 ──▶ cmd.actuator ──┐
      │                              ├──▶ renderer（SceneDelta，允许丢帧）        │
   force 插件累加广义力 ◀────────────┤ ├──▶ recorder（MCAP）                       │
      ▲                              │ └──▶ UI / Web（core_api 订阅）              │
      └──── 环境（星历/重力/大气/SRP/eclipse）──────────────────────────────────────┘
```

要点：状态由动力学产出、内核发布，其余只读；命令由控制器产出、动力学消费，过期丢弃；力模型只累加、不改状态。

## 7. 标准数据格式

### 7.1 全局约定

| 项 | 规定 |
|---|---|
| 单位 | SI：m、kg、s、rad、N、N·m、kg·m²；一律双精度（渲染顶点用 float32） |
| 坐标系 | `ECI_J2000`、`ECEF_ITRF`、`RIC:<origin>`、`BODY:<id>`、`JOINT:<id>`、`SENSOR:<id>`（相机 +Z 光轴，OpenCV 约定）、`SITE:<id>`；变换查询必须带时刻 |
| 旋转 | 单位四元数 `[x, y, z, w]`，Hamilton；`q_wb` 表示 body→world |
| 时间 | 主表示 `t_tai_ns: int64`（相对 J2000.0 TAI）；星历用 `t_tdb_s`；UTC↔TAI 用带版本的闰秒表；仿真时钟 `t = t0 + Σdt`，不读墙钟 |
| 标识 | 实体 `[a-z0-9_.-]{1,64}`；话题 `<域>.<对象>[.<实例>]`；schema `srsp.<name>/<major>`；插件反向域名 |
| 字节序/对齐 | 小端；结构体显式对齐；禁用编译器默认打包 |

### 7.2 模型：SRM（自定义 IR）

`.srm` 是 zip 包：`model.json` + `meshes/*.glb` + `collision/*.obj` + 可选 `materials/*.json`。

| 段落 | 内容 |
|---|---|
| `bodies[]` | `mass`（不含子树）、`com`、`inertia`（相对质心 3×3）、`visual/collision`、`aero{area,cd,reflectivity,solar_area}` |
| `joints[]` | `type/axis/origin/limit/damping/stiffness/motor`；v1 只支持树形（无闭链） |
| `actuators[]` | `reaction_wheel`（动量交换，轮速为状态）、`thruster`（力+力矩+质量流） |
| `sensors[]` | `mount`（body→sensor 固定变换）、`rate_hz`（须为宏步整数分频/倍）、噪声参数（1σ） |
| `force_models[]` | 引力梯度、SRP、气动等 |
| `flex[]`（1.1） | 模态频率/阻尼/参与因子；不支持须报 `unsupported`，禁止静默忽略 |

导入器（URDF/SDF/MJCF/CAD）必须输出 `import_report.json`（成功/降级/丢失/建议人工修正）。

### 7.3 状态：Arrow 表（不可变快照）

| 表 | 列 |
|---|---|
| `state.bodies` | `id`(dict)、`pos_w`、`quat_wb`、`vel_w`、`omega_b`、可选 `acc_w`/`alpha_b` |
| `state.joints` | `id, q, qd, tau, motor_tau, limit_flag` |
| `state.actuators` | `id, kind, momentum, speed, thrust, mass_flow, enabled` |
| `state.meta` | `t_tai_ns, step_index, dt, energy{kinetic,potential}, momentum_total` |

混合约定：线速度在世界系、角速度在体系；插件在 `ModelInfo` 中声明，内核按需转换。每个宏步算 `state_hash = xxh3_128(canonical_bytes(tables))`，用于确定性回归与录制校验。

**大数组传递**：`BlobRef{shm_name, offset, length, schema_id, schema_hash, generation}`；生产写完后 release 递增 `generation`，消费前后各校验一次（seqlock）。进程内插件可直接拿 `ArrowArray*`（唯一允许的快路径差异）。

### 7.4 场景与渲染协议

- 场景包 `srsp.scene/1`：`gltf`（GLB）+ `entity_map`（节点路径 → 实体 ID，不用索引）+ `environment{earth,stars,lights}` + `materials_override`。
- glTF 为 Y-up、右手；加载时对根节点施加固定变换到 ECI Z-up，**不逐帧转换**。
- `SceneDelta`：`generation`、`origin_eci`（双原点浮动）、相机、`entities[{id,pos_rel(float32),quat_wb,visible,highlight,material_override}]`、覆盖层、灯光、选中态。
- 渲染：双原点 + 反转 Z + 无限远平面（远 1e12 m、近 1 cm），解决 7000 km 处 float32 精度抖动。
- 离屏成像 `CaptureRequest` → `ImageRef{BlobRef,width,height,stride,format,t_tai_ns,frame_index}`；允许 1–3 帧流水线延迟但必须回带仿真时刻；分割/ID 通道不过色调映射。

### 7.5 传感器帧

统一信封：`SensorFrame{sensor_id, t_tai_ns, seq, valid, status(OK/SATURATED/ECLIPSE/DROPOUT), schema_id, data(BlobRef), meta_json}`；噪声用内核注入的确定性随机流，`(seed, sensor_id, seq)` 可复现。

### 7.6 项目与场景文件

| 文件 | 角色 | 关键字段 |
|---|---|---|
| `.srproj` | 项目（人读、可 diff） | `requires{能力: 版本区间}`、`plugins{配置}`、`render`、`recording`、`seed` |
| `scenario.json` | 初始状态与事件 | `epoch_utc`、`initial_state{orbit/attitude/joints}`、`environment`、`events[]`（按 t 排序，同时刻按文件顺序） |

### 7.7 录制：MCAP

4 MiB chunk、zstd-3；每话题一个 channel，`message_encoding = protobuf|arrow|json`；内嵌 schema 保证离线可解析；时间索引 + 每通道索引；`run.json` 作为 attachment（构建、git、插件版本、种子、模型哈希）。回放是一种时钟源（`ReplayClock`），逐宏步比对 `state_hash` 定位首个偏离项；可导出 Parquet/CSV 供 DuckDB/pandas 分析。

### 7.8 版本演进规则

新增可选字段任意版本；新增枚举值/表列算次版本；字段语义变更、删字段、改单位一律主版本。Protobuf 字段号不复用；schema 用 `schema_id + hash` 校验，不匹配拒绝并提示升级。

## 8. 关键设计约束

| 约束 | 体现 |
|---|---|
| 确定性 | 注册序/实体 ID 排序、种子随机流、无墙钟、`-ffp-contract=off`、状态哈希回归 |
| ABI 稳定 | `struct_size` 追加、纯 C、跨编译器布局断言、插件静态链接 CRT |
| 语言自由 | 官方实现限 C++/Python/Rust；其余走边车；同一语义两种传输 |
| 工具链矩阵 | GCC（主）/Clang-UCRT64/Clang-CLANG64（验证）/MSVC；跨工具链只允许 C ABI 加载 |

## 9. 当前实现进度对照

| 层 | 状态 |
|---|---|
| L0 契约 | ✅ ABI + 布局断言、manifest schema + 10 样例、`core_api.h` v0（draft）、错误码表、版本协商测试 |
| 环境 | ✅ 四工具链构建与测试（各 4/4） |
| L1 插件 | ⚠️ 仅 hello（C++ 进程内）可运行；Python 边车仍是目标形态示例 |
| L2 内核 | ❌ 未实现（`src/` 不存在） |
| L3 应用 | ❌ 无 `srsp-cli` / `srsp-studio` |
| CI | ❌ 未建立 |

## 10. 执行记录

| 日期 | 动作 | 结果 |
|---|---|---|
| 2026-10-23 | 依据 docs/01–07、`api/` 与已落地测试整理架构与数据交互摘要 | 完成 |

## 11. 修订记录

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-10-23 | V1 | 初稿：分层、运行时、插件体系、主机/插件接口、插件间交互、数据格式、进度对照 |
