# 01 总体架构设计

> 项目代号：**SRSP**（Space Robot Simulation Platform，空间机器人仿真平台）
> 文档版本：0.1（架构基线）｜状态：评审草案｜最后更新：2026-10-20

---

## 1. 定位、边界与假设

### 1.1 产品定位

SRSP 是一套面向**空间机器人**（在轨服务、目标捕获、舱外装配、巡视探测、地面遥操作训练）的**桌面级可视化仿真平台**。它以"平台 + 插件"的方式集成机器人模型、动力学求解器、空间环境、传感器模型、可视化引擎与控制器，支持从快速原型到高保真验证的多种使用强度。

### 1.2 设计目标（按优先级）

| 编号 | 目标 | 可验证的判据 |
|---|---|---|
| G1 | **可插拔**：模型、动力学、环境、渲染等皆为插件，可动态装载/替换 | 同一场景在 3 个不同动力学插件间切换，仅改配置，不改代码 |
| G2 | **语言无关**：组件可用 C/C++、Rust、Python、C#、Java、MATLAB 等实现 | 至少有 1 个非 C++ 插件（Python 边车）通过一致性测试 |
| G3 | **跨平台**：Windows 10/11 与 Linux 同源构建、同源打包 | CI 在 Windows/Linux 构建矩阵上构建并跑通冒烟用例 |
| G4 | **确定性可重放**：同一输入产生同一位序列，支持录制/回放/回归 | 录制回放 10 分钟后状态哈希与原始运行一致 |
| G5 | **可视化优先**：轨道尺度场景、地球/星空/光照、传感器成像 | 1080p 主视口 60 fps；3 路 640×480 相机 30 Hz |
| G6 | **工程可交付**：MSYS2 + VSCode 一键环境，安装包可离线部署 | 干净虚拟机 30 分钟内完成安装并跑通示例 |
| G7 | **可演进**：契约稳定、插件生态可扩展、内核可拆分为服务 | 1.x 期间 ABI 不破坏；可选 `srspd` 远程内核模式 |

### 1.3 v1.0 的非目标（明确不做，避免范围蔓延）

- **分布式集群并行仿真**（多机器人协同在单进程内支持，见 §6.6）、云端多用户协同编辑；
- 硬实时（μs 级抖动）与 RTOS 部署（保留 HIL 接口设计，v1 只做软实时）；
- 流体、热、结构有限元的强耦合（仅预留 `force` 扩展点；热网络能力见 07 §11 C16 预留）；
- 通用游戏引擎能力（动画、材质编辑器、关卡编辑器）；
- 移动端 / Web 端完整编辑器（Web 仅做查看与远程操作）；
- **GPU 并行强化学习训练平台**：不与 Isaac/MJX 竞争训练吞吐；仅保留数据互操作（MJCF/MCAP 导入导出、MuJoCo 边车插件）与批量接口预留（见 [07 §11](07-roadmap-risks.md)），推理与训练属于外部生态。

### 1.4 关键假设

| 假设 | 影响 | 若不成立 |
|---|---|---|
| 单机仿真，模型规模 ≤ 200 刚体、≤ 2000 状态量 | 单进程 + 单 sim 线程即可满足 | 需引入多线程动力学 / 分布式内核 |
| 团队 5–8 人，1.0 周期 9–12 个月 | 里程碑与人力安排 | 压缩 v1 范围（先做刚性多体 + 基础环境） |
| 目标机器有独立 GPU（Vulkan/DX11/GL 4.1 任一） | 默认渲染器用 bgfx | 降级到 Qt Quick 3D 或纯软件视图 |
| 部署环境可能离线/涉密内网 | 依赖必须可离线打包，禁止运行时下载 | 需额外搭建内网制品库 |
| 用户具备航天动力学基础 | UI 直接暴露 ECI/RIC、四元数等概念 | 需增加向导式场景模板 |

---

#### 1.4.1 规模假设（2026-10-27 采纳）

- 单机模型初始设计假设：**≤200 刚体 / ≤2000 状态量**；
- 超出时不直接拆服务，先重新评估并行策略与数据结构；
- 单机多 world 批量（集群仿真）在 M4 计划内；**跨机分布式**属非目标（见 docs/07 §17）。
## 2. 架构原则

1. **契约先行（Contract First）**：先冻结数据模式与能力接口，再写实现；插件与内核只通过版本化契约耦合。
2. **内核最小化**：内核只负责调度、注册、时间、坐标系、数据总线、插件生命周期与故障处理，不含任何领域算法。
3. **数据与算法分离**：模型（SRM）、状态、场景是数据；动力学、环境、渲染是消费/生产这些数据的算法插件。
4. **进程边界即故障边界**：不受信任或运行时复杂的插件放入边车进程；进程内只允许稳定 ABI 的 C/C++/Rust 插件。
5. **同一语义，两种传输**：进程内 C ABI 与进程间 gRPC 承载完全相同的消息与方法语义，避免"两套 API"。
6. **确定性优先于便利性**：仿真线程内禁止墙钟、哈希遍历、无序并行归约；随机数只来自内核注入的确定性流。
7. **渲染与仿真解耦**：仿真步进不被渲染帧率阻塞；渲染消费最新快照，允许丢帧与插值。
8. **一切可观测**：每个宏步、每次插件调用都可打点；日志、指标、录像是同一套事件模型的不同投影。

---

## 3. 分层视图

```
┌──────────────────────────────────────────────────────────────────────────┐
│ L3 应用层                                                                │
│   srsp-studio（Qt 桌面）   srsp-cli（无头/批处理）   srsp-web（远程查看）  │
│   Python SDK   MATLAB SDK   CI 测试驱动                                   │
└───────────────┬──────────────────────────────────┬───────────────────────┘
                │ 进程内 API / gRPC / WebSocket     │
┌───────────────▼──────────────────────────────────▼───────────────────────┐
│ L2 平台内核 libsrsp（C++20）                                              │
│   Runtime/Scheduler   Service Registry   Data Bus   Frame Graph           │
│   Time Service        Package Manager    Snapshot/Replay   Diagnostics    │
│   Plugin Host（in-proc loader + sidecar supervisor）                      │
└───────┬───────────────────────────────────────────────┬──────────────────┘
        │ C ABI v1（dlopen / LoadLibrary）               │ gRPC + 共享内存
┌───────▼──────────────────────────────┐   ┌────────────▼──────────────────┐
│ L1a 进程内插件                        │   │ L1b 边车插件                   │
│  model.*  dynamics.*  env.*           │   │  pybridge（Python）            │
│  sensor.* render.* control.* io.*     │   │  unity-renderer / matlab / jvm │
└──────────────────────────────────────┘   └───────────────────────────────┘
┌──────────────────────────────────────────────────────────────────────────┐
│ L0 契约层（api/）：C ABI 头文件 · Protobuf · JSON Schema · glTF 扩展 · 版本 │
└──────────────────────────────────────────────────────────────────────────┘
```

**依赖方向**：L3 → L2 → L0；L1 插件只依赖 L0。禁止插件反向依赖内核内部头文件（由 CMake 目标隔离 + 头文件安装白名单保证）。

### 3.1 为什么"内核 + 插件"而不是"框架 + 模块"

- 模块是编译期链接，插件是运行期发现：后者允许第三方在不重新编译平台的情况下扩展能力，也允许同一场景切换实现（例如刚性动力学 ↔ MuJoCo）。
- 插件有独立版本号、独立发布节奏、独立许可证，便于高校/院所合作方只交付自己的算法。
- 故障隔离与资源配额只能在进程/动态库边界上实施。

---

## 4. 进程与部署视图

### 4.1 三种候选形态

| 形态 | 描述 | 优点 | 缺点 | 结论 |
|---|---|---|---|---|
| A. 单进程 | Studio = UI + 内核 + 默认插件 | 延迟最低、实现最简单、调试方便 | C++ 插件崩溃拖垮 UI；难以无头复用 | **v1.0 默认** |
| B. UI/内核分离 | `srspd` 内核进程 + Studio 客户端 | 崩溃隔离、天然支持远程与无头、便于集群 | 多一层 IPC，窗口/渲染归属需设计 | v1.x 可选模式 |
| C. 全隔离 | 内核 + 每插件一进程 | 最强隔离 | 通信开销大、调试复杂、打包庞大 | 仅对不受信任插件启用 |

**决策**：v1.0 采用形态 A，但**内核对外 API 一律按"可跨进程"设计**（gRPC service 定义与进程内 C ABI 同构），保证形态 B 是部署选项而非重写。渲染器与 UI 同进程，便于原生窗口嵌入与低延迟交互。

### 4.2 典型部署

```
单机桌面（默认）
  srsp-studio.exe
    ├─ Qt UI 线程
    ├─ Sim 线程（内核调度）
    ├─ Render 线程（bgfx，主视口 + 传感器离屏视图）
    ├─ IO 线程（MCAP 录制、gRPC server）
    └─ 子进程：pybridge（Python 控制器/脚本）、可选 unity-renderer

无头/批处理
  srsp-cli --scenario capture.json --duration 600 --record run.mcap
    └─ 同一内核库，无 Qt，无渲染（或 renderer.null）

远程/训练
  srspd（Linux 服务器，无头） ← gRPC/WebSocket → srsp-web（浏览器）/ 轻客户端
```

### 4.3 线程模型

| 线程 | 归属 | 职责 | 约束 |
|---|---|---|---|
| UI | Qt 主线程 | 界面、面板、拾取请求 | 不得直接触碰仿真状态，只能发命令/取快照 |
| Sim | 内核 | 事件处理、步进、发布快照 | 唯一可修改仿真状态的线程 |
| Render | 渲染插件 | 消费快照、绘制、离屏成像 | 只读快照；不得阻塞 Sim |
| IO | 内核 | 录制、gRPC/WebSocket 服务 | 通过无锁队列与 Sim 交互 |
| Sidecar | 子进程 | 插件自身逻辑 | 通过 gRPC + 共享内存通信 |

**调用规则**：插件方法默认只在 Sim 线程被调用；清单 `entry.thread_safe` 中列出的方法可被 IO 线程并发调用（例如查询元数据）。任何插件不得自行创建长期线程去读写仿真状态。

---

### 4.3.1 设备与线程所有权（GPU / 批量前置）

v1 只有 CPU 路径，但接口按“可接 GPU”设计。所有权规则现在就定下来，避免后期插件各自建上下文：

| 资源 | 所有者 | 规则 |
|---|---|---|
| GPU 设备、队列、命令缓冲 | **host（内核）** | 插件不得自建 `VkInstance`/CUDA context；通过 host 提供的原语借用（对应预留接口 R2/R9） |
| 显存池 / buffer | host | 按 world 生命周期分配；插件不持有跨调用指针 |
| 批处理线程池 | host | 插件不得创建长期线程读写仿真状态（与本表 Sim/Render/IO 约束一致） |
| 渲染线程 | 渲染插件 | 与 compute 共用队列时由 host 仲裁；渲染丢帧不得阻塞 Sim |
| 边车子进程内的 GPU | 边车自身 | 与主进程设备互不可见；跨进程只传数据，不传句柄 |

**不变量**：任何插件在 `init` 之后都不得假设“自己独占设备”；host 可以在同一设备上串行调度多个 world 的 compute。
## 5. 运行时视图

### 5.1 宏步调度循环

```
while (running):
    t_next = t + dt
    events = event_queue.pop_until(t_next)          # 确定性排序：t, priority, seq
    dispatch(events)

    for sensor in sensors_due(t):                   # 采样时刻 = t
        sensor.sample(state, t) -> publish(topic, blob)

    for controller in controllers_due(t):
        controller.update(state, t) -> commands     # 写入命令总线

    forces = zero
    for provider in force_providers:                # 顺序固定 = 注册序号
        provider.accumulate(state, t, env, forces)

    dynamics.step(state, t, dt, commands, forces)   # 插件内部可子步
    t = t_next

    snapshot = state.freeze()                       # 双缓冲 / 不可变视图
    bus.publish("sim.state", snapshot)
    recorder.offer(snapshot)                        # 非阻塞
    renderer.offer(snapshot)                        # 非阻塞，允许丢帧
```

关键点：

- **事件驱动 + 固定步长**：默认 `dt = 1 ms`，可在 0.1–10 ms 配置；变步长由动力学插件在内部子步实现，对外仍以宏步汇报。
- **多速率**：传感器/控制器按 `rate_hz` 对齐宏步（只允许整数分频）；渲染按帧率独立。
- **快照不可变**：Sim 线程产出只读快照（引用计数），渲染/录制/网络各持一份，避免锁。
- **背压**：录制队列满时按策略丢弃低优先级话题并计数告警，绝不阻塞 Sim。

### 5.2 一次"加载场景并步进"的时序

```mermaid
sequenceDiagram
    autonumber
    participant U as Studio UI
    participant K as Kernel
    participant P as PluginHost
    participant D as dynamics.rigid
    participant R as renderer.bgfx
    participant S as Python sidecar

    U->>K: open_project("capture_demo.srproj")
    K->>P: resolve(requires) + validate(manifest)
    P->>D: init(host, config)
    P->>R: init(host, config)
    P->>S: spawn + handshake(token, shm)
    K->>D: call("dynamics.load", SRM)
    D-->>K: model_handle + dof
    K->>R: call("renderer.load_scene", glTF + entity map)
    K->>U: project_ready(scene graph, topics)
    loop 每个宏步
        K->>D: call("dynamics.step", StepRequest)
        D-->>K: StepResult
        K->>S: call("controller.update", state blob ref)
        S-->>K: commands
        K->>R: submit(snapshot delta)
        R-->>U: frame_ready
    end
    K->>S: shutdown
    K->>P: unload all
```

### 5.3 数据流与所有权

```
                 ┌───────────────┐
   SRM 模型 ───▶ │ dynamics 插件 │ ───▶ 状态(Arrow) ──┬──▶ 渲染器
                 └───────────────┘                    ├──▶ 录制器(MCAP)
                        ▲                             ├──▶ 传感器
   力模型/环境 ─────────┘                             └──▶ UI/网络
```

- 模型由 `model.importer` 解析为 SRM 后交给内核；内核持有只读副本，按引用传给动力学/渲染。
- 状态由动力学插件产出，内核负责发布；其他插件只读。
- 命令由控制器产出，动力学插件消费；命令带时间戳与有效期，过期丢弃。

---

### 5.1.1 面向世界集合与 GPU 的循环扩展（设计，1.x 保持单世界）

v1 实现是单世界、纯 CPU；为 M4 的批量世界与 GPU 计算预留以下形态（**当前不改实现**）：

```
for each macro step:
    t_next = t + dt
    dispatch(event_queue.pop_until(t_next))          # 顺序：t, priority, seq

    for world in worlds_due(t):                      # 新增：world 维度
        for sensor in sensors_due(world, t):  sample -> publish(world, topic, blob)
        for ctrl   in controllers_due(world, t): update -> commands(world)
        for force  in force_providers(world):    accumulate(state, env, forces)

    compute.dispatch(world_batch, backend)           # 新增：批量计算阶段
        # CPU 后端：按 world 串行/线程池；GPU 后端：一次提交多 world
        # 结果写回各自的 state；失败时该 batch 降级到 CPU 重试一次

    record(world snapshots)                          # 录制按 world 分 channel
    render(subset_of_worlds)                         # 渲染可只取可见子集，允许丢帧

    budget.check(plugin_calls)                       # 新增：时间预算
        # 超预算 → fail_policy（degrade/skip-step）+ 诊断事件，记录到宏步耗时分解
```

三条配套规则：

1. **确定性**：`worlds_due` 顺序按 `world_id` 升序；GPU 归约顺序固定；跨后端只承诺 D2（见 §6.3 的 D0/D1/D2 分级）。
2. **失败隔离**：单个 world 的 compute 失败不影响同批其他 world；batch 失败降级 CPU，仍失败则标记该 world 暂停并保留状态。
3. **录制**：channel 命名 `sim/<world_id>/<topic>`；`state_hash` 前缀含 `world_id`（预留字段，见 A16）。
### 5.1.2 写入与背压硬规则（2026-10-27 采纳）

1. **命令不得绕过队列**：控制器/UI/网络只能提交带 `(t, priority, source_id, seq)` 的命令；**不存在直接指向仿真状态的写指针**。事件键新增 `source_id`，用于同刻事件的可追溯排序。
2. **快照 lease + generation**：快照版本为 `step_index + generation`；读者持有只读 lease，`release` 前缓冲不得复用（防止旧 lease 读到复用后的新数据）。
3. **背压**：命令队列**不得静默丢弃**（满则阻塞提交方或显式拒绝并告警）；录制满盘 → 停录 + 故障事件，仿真继续或按策略停止；渲染/预览允许丢旧帧。
4. **sidecar 调用**：必须有 deadline、取消语义与失联状态；超时不得阻塞 Sim。
## 6. 关键机制

### 6.1 服务注册与能力发现

内核维护两层注册表：

| 注册表 | 键 | 值 | 用途 |
|---|---|---|---|
| 插件表 | `plugin_id` | 清单、状态、进程信息 | 管理生命周期 |
| 能力表 | `capability@major` | 实现列表 + 优先级 + 约束 | 按能力解析依赖、支持多实现切换 |

场景文件不直接引用插件 ID，而引用**能力需求**（`srsp.dynamics/solver@>=1.0`）；由 `selection` 策略（优先级、用户固定、基准测试得分）选出实现。这样替换求解器不需要改场景。

### 6.2 时间与坐标系

- 时间：内核 `TimeService` 统一提供 TAI 纳秒 + 比例因子；支持实时/加速/单步/外部时钟四种模式；UTC↔TAI 使用内置闰秒表，TDB↔TT 使用简化周期项（详见 03 文档）。
- 坐标系：`FrameGraph` 管理 ECI(J2000)、ECEF(ITRF)、RIC（Radial-In-track-Cross-track，近距操作常用）、本体、关节、传感器、站点等；所有变换双精度，查询需带时刻。
- 单位：SI（m、kg、s、rad、N、N·m），角度一律弧度，四元数 `[x,y,z,w]`。

### 6.2.1 FrameGraph 与渲染双原点的分工

- **FrameGraph 是变换的唯一来源**：ECI/ECEF/RIC/本体/关节/传感器之间的变换查询都走它，查询带时刻；
- **渲染的双原点是优化，不是坐标系**：渲染器内部把世界位置拆成“相对地球/相对相机”两套原点以控制浮点误差，输入仍来自 FrameGraph；
- **传感器与录制**同样从 FrameGraph 取变换，避免各自维护一份；
- 当前实现状态：双原点投影已落地（`srsp_render`），FrameGraph 尚未实现，属 1.2 之前的缺口。
### 6.3 确定性

| 来源 | 措施 |
|---|---|
| 时间 | 仿真时间只由步长累加，不读墙钟 |
| 迭代顺序 | 全部按注册序号/实体 ID 排序，禁用 `unordered_map` 遍历 |
| 随机数 | 内核提供按 `(seed, entity_id, stream)` 派生的独立流 |
| 并行 | 力模型可并行计算，但按固定顺序归约；禁止浮点原子累加 |
| 编译 | 禁用 `-ffast-math`；固定 FP 收缩选项；记录编译器与 flags |
| 外部 | 边车插件必须实现 `checkpoint/restore`，重放时禁用其内部缓存 |

跨平台一致性目标：同一算法与顺序下相对误差 < 1e-9；**GPU 渲染与传感器图像不做跨驱动位级一致承诺**，仅同机同驱动内可复现，跨平台用图像相似度阈值。

### 6.4 故障处理

| 场景 | 策略 |
|---|---|
| 边车进程崩溃 | 看门狗检测 → 指数退避重启（默认 3 次）→ 从最近检查点恢复 → 仍失败则标记能力降级并告警 |
| 边车无响应 | 心跳超时（默认 2 s）→ 取消当前调用 → 重启 |
| 进程内插件崩溃 | 进程终止；启动时进入安全模式，禁用上次崩溃的插件并提示 |
| 插件返回错误 | 结构化错误码 + 诊断信息；调度器按 `fail_policy`（fail-fast / skip-step / degrade）处理 |
| 录制写满磁盘 | 停止录制并告警，仿真继续 |

### 6.5 可观测性

- 统一诊断模型：`severity + code + message + context + remedy`；
- 每个宏步记录：耗时分解（传感器/控制器/力模型/动力学/序列化）、状态哈希、事件数；
- Tracy 集成的帧/步剖析；可选 OpenTelemetry 导出（后续）；
- 日志分级 + 结构化 JSON 双输出，支持按插件过滤。

### 6.6 多机器人协同

v1 支持**单进程内 N 个机器人**（起步 N ≤ 8）协同，跨进程/跨机分布式由 1.2 的 `srspd` 承担。

**调度顺序（确定性的一部分）**：

1. 一个宏步内按 `robot_id` 字典序依次更新各机器人动力学；
2. 协同层（`srsp.control/coordinator@1`）在所有单体更新之后运行一次，读取 `team.state`，输出下一宏步指令；
3. 链路消息按 `(发送时刻, 发送方 id, 序号)` 排序投递，与墙钟无关；
4. 每个机器人的随机源由 `xxh3(global_seed, robot_id)` 派生，保证并行/串行一致；
5. 实际顺序写入 `state.meta.schedule_order`，回放时校验。

**通信模型**（`srsp.io/link@1`）：有向链路，参数为整数纳秒延迟、带宽上限、丢包率、队列容量；消息进入确定性事件队列，按时间戳投递。

**边界**：协同算法以插件形式实现，内核只提供 `team.state`、调度顺序与链路原语；不内建编队/任务分配算法，避免内核变重。

**团队级诊断**：链路丢包、协同超时、成员退出各有独立错误码，写入 `team.state` 与结构化日志。

---

## 7. 质量属性目标

| 属性 | 指标（目标值，x86-64 / -O2） | 验证方式 | 当前实测 / 状态 |
|---|---|---|
| 动力学性能 | 100 刚体 + 30 关节，单步 ≤ 300 μs | 基准测试（Google Benchmark） | **9.06 ms（豁免到 1.1）** |
| 插件开销 | 进程内单次调用 p99 ≤ 20 μs；边车单宏步 ≤ 1 ms | 微基准 + 端到端 | 未实测 |
| 渲染 | 1080p、2M 三角形、60 fps；4 视口不降主视口帧率 | GPU 计时查询 + 帧分析 | Qt 软件后端，未达（1.1 QRhi） |
| 启动 | 冷启动 ≤ 3 s，热启动 ≤ 1.5 s | 启动打点 | 未实测 |
| 内存 | 典型场景 ≤ 2 GB | 进程 RSS 采样 | 未实测 |
| 确定性 | 同平台同构建 10 分钟回放哈希一致 | 回归用例 | 200/2 000 步一致（长稳未跑） |
| 稳定性 | 8 小时连续运行无内存增长（< 2%） | soak test | 未做 |
| 可用性 | 边车崩溃后 ≤ 3 s 恢复或降级提示 | 故障注入 | 重启一次已测（P2 相关） |

---

### 7.1 支持等级规则（2026-10-27 采纳）

- **supported**：CI 与发布流程实际覆盖（有作业、有产物、有测试）；
- **planned**：路线图承诺但无 CI/产物；
- **unverified / experimental**：本地可用但无 CI 证据。

凡是未满足 supported 的项，文档必须显式标注等级，不得只在路线图里写“支持”。

**当前支持矩阵（D5，2026-10-28 核对）**

| 能力 | 等级 | CI 证据 |
|---|---|---|
| UCRT64 GCC 构建与 40 项测试 | **supported** | `windows / UCRT64 (GCC)` 作业 |
| 文档/JSON/YAML/链接/schema 哈希 | **supported** | `docs and data files` 作业 |
| Qt 软件渲染后端 | **supported** | `unit.render` / `integration.studio_*` |
| QRhi（Null 后端） | **experimental** | `unit.render_rhi`；无 GPU 路径 |
| QRhi（D3D11 真机） | **planned** | 无（P33） |
| UCRT64 Clang | **unverified** | 无 CI 作业（仅本地可用） |
| MSVC | **unverified** | 无 CI 作业（P27） |
| CLANG64（libc++） | **experimental** | 预设已移除（P28） |
| Linux 构建 | **planned** | 作业已移除，1.2 恢复 |
| 性能 300 µs | **planned** | 已豁免到 1.1（docs/07 §12.1） |当前对应：UCRT64 GCC=supported，UCRT64 Clang=supported（构建+测试），MSVC=unverified，CLANG64=experimental（已移除预设）。
## 8. 架构决策摘要（ADR Index）

| ADR | 决策 | 主要理由 | 详见 | 状态 |
|---|---|---|---|---|
| A01 | 内核 C++20，插件契约为纯 C ABI | 跨语言 FFI 成本最低，ABI 稳定可控 | 02 | 有效 |
| A02 | 控制面 Protobuf；**状态面用 typed raw block（f64/bytes）**；Arrow C Data 仅用于跨语言分析（2.0 再评估） | 状态面数据简单且高频，raw block 零依赖、可直接进 MCAP；Arrow 的成本在当前阶段不划算 | 02/03 | **已修订**（2026-10-27）：原“数据面 Arrow”未实现，改为 raw block |
| A03 | 进程内 + 边车双传输 | 性能与语言自由/隔离兼得 | 02 | 有效 |
| A04 | 自定义模型 IR（SRM），URDF/SDF/MJCF 仅作导入器 | 空间机器人需要柔性、动量交换、推力器等扩展 | 03 | 有效（URDF/MJCF 子集已实现） |
| A05 | 场景用 glTF 2.0 + 自定义扩展 | 工具链成熟、可复用 DCC 资产 | 03 | 有效（尚未实现渲染侧加载） |
| A06 | 渲染器为插件，默认 bgfx | 多后端、可控管线、BSD 许可 | 04 | **superseded → A13**（2026-10-26） |
| A07 | Python 不内嵌，作为边车 | 规避 MinGW/MSVC ABI 与 GIL 风险，可用完整生态 | 04/05 | 有效（MuJoCo 边车原型已验证） |
| A08 | 录制用 MCAP | 自描述、可索引、跨语言、Foxglove 可直接看 | 03/04 | 有效（子集，无 Summary → A15） |
| A09 | Windows 用 MSYS2 UCRT64 原生工具链 | 满足约束、包生态完整、产物为原生 Win64 | 05 | 有效 |
| A10 | 打包可重定位 runtime + `.srpkg` 插件包 | 离线部署、插件独立升级 | 06 | 未实现（M3） |
| A11 | 内核对外暴露宿主侧 C API（`core_api.h`） | Rust CLI、Python SDK、Web 服务等非 C++ 应用可嵌入内核；与插件 ABI 同规则 | 02/api | 有效（插件加载已接线） |
| A12 | Windows 多工具链：UCRT64-GCC 为主，UCRT64-Clang / CLANG64 / MSVC 为辅 | 第二编译器与 sanitizer 走 Clang（同 libstdc++ 可互链），CLANG64 仅作 libc++ 验证层；插件按 `os-arch-toolchain` 分发，跨工具链只允许 C ABI 加载或边车 | 02/05/06 | **收窄 → A14**（CLANG64 已移除，MSVC 未验证） |
| A13 | 渲染后端分层：`renderer.null` / `renderer.qt` / 计划中的 `renderer.rhi`（QRhi，D3D11/Vulkan） | bgfx 无 MSYS2 包且集成成本高；QRhi 零新增依赖、可与 compute 共用设备、保留跨平台可能 | 04 附录 A | 有效（null/qt 已实现，rhi 待 1.1） |
| A14 | 当前受支持工具链 = UCRT64 GCC（主）+ UCRT64 Clang（次）；MSVC 待验证，CLANG64 experimental | CLANG64 的 libc++ 环境不可靠（P28）；MSVC 缺 vcvars 验证（P27）；收窄可保证 CI 与本地一致 | 05 | 有效（2026-10-26） |
| A15 | MCAP 采用子集实现：写入 Magic/Header/Schema/Channel/Message/DataEnd/Footer，暂不写 Summary 与压缩 | 顺序读取与官方 SDK 已实测兼容；Summary/压缩列入 1.1 | 03 §13 | 有效（2026-10-26） |
| A16 | GPU 与批量世界：能力寻址 + host 拥有设备；确定性分 D0/D1/D2；批量能力命名 `srsp.dynamics/batch@1` | 为集群与 GPU 预留接口而不在 ABI 放未实现符号 | 07 §13 | 设计已定（1.1/M4 落地） |
| A17 | 模型命名空间治理：仅 `tools/ai`、`AGENTS.md`、`ai-models.json`、`<model>/ModuleTest` 入库；`<model>/Logs`、`<model>/Temp` 本地保留 | 避免个人计划/缓存进入共享历史 | 05/AGENTS | 有效（2026-10-26） |

---

## 9. 术语表

| 术语 | 含义 |
|---|---|
| SRM | Space Robot Model，平台自定义的机器人模型中间表示 |
| SRSP | 本项目代号；C API 前缀 `srsp_` |
| 能力（Capability） | 形如 `srsp.dynamics/solver@1` 的接口契约标识 |
| 宏步 | 调度器对外暴露的最小步进单位，默认 1 ms |
| 边车（Sidecar） | 独立进程运行的插件，通过 gRPC + 共享内存与内核通信 |
| 快照 | 某一时刻不可变的仿真状态视图（Arrow 表 + 元数据） |
| 降级 | 某项能力不可用时，平台以受限功能继续运行 |
| 场景 | 项目（plugins/config） + 模型 + 环境 + 初始状态 + 脚本的集合 |
