# 03 标准数据与领域模型

> 本文定义平台的**数据契约**：单位、坐标系、时间、模型中间表示（SRM）、状态表、场景/渲染协议、录制格式与版本演进规则。
> 数据模式版本：`1.x` ｜ 状态：评审草案

---

## 1. 契约分层

| 层 | 载体 | 用途 | 谁生产 / 谁消费 |
|---|---|---|---|
| 描述层 | JSON + JSON Schema | 插件清单、项目、场景、配置 | 人 / 工具链 / 内核 |
| 模型层 | SRM（JSON + GLB 资源包） | 机器人运动学、质量特性、执行器、传感器安装 | 导入器 / 动力学 / 渲染 |
| 场景层 | glTF 2.0 + 扩展 | 网格、材质、灯光、实体映射 | DCC 工具 / 渲染器 |
| 消息层 | Protobuf | 控制、元数据、小消息 | 全部 |
| 数组层 | Arrow C Data Interface | 状态、矩阵、图像、点云 | 动力学 / 传感器 / 录制 |
| 记录层 | MCAP | 全量时序记录与回放 | 录制器 / 分析工具 |

**原则**：能用模式表达的就不用代码约定；能零拷贝的就不序列化；每个二进制块必须自带 `schema_id` 与哈希。

---

## 2. 全局约定（必须遵守）

### 2.1 单位与数值

| 量 | 单位 | 说明 |
|---|---|---|
| 长度 | m | 轨道量用 m，不用 km；显示层再换算 |
| 质量 | kg | |
| 时间 | s | 内部用 TAI 纳秒整数 + 双精度秒 |
| 角度 | rad | 界面显示可转度，契约内一律弧度 |
| 力 / 力矩 | N / N·m | |
| 转动惯量 | kg·m² | 相对质心、本体系 |
| 温度 | K | 预留 |
| 磁场 | nT | |
| 角度速率 | rad/s | |

- 所有浮点为 IEEE-754 双精度（`float64`），仅渲染顶点/像素用 `float32`。
- 小端序；结构体显式对齐；禁止依赖编译器默认打包。
- 缺失值用 `NaN` 或 Arrow 的 null，二者不可混用：状态表用 null，标量消息用 `NaN`。

### 2.2 坐标系

| 名称 | 标识 | 定义 |
|---|---|---|
| 地心惯性系 | `ECI_J2000` | J2000.0 平赤道平春分点，右手系，+Z 指向北天极 |
| 地心地固系 | `ECEF_ITRF` | ITRF 实现，随地球旋转 |
| 轨道系 | `RIC:<origin>` | Radial-In-track-Cross-track：+X 径向、+Z 轨道面法向、+Y 完成右手系（沿迹） |
| 本体系 | `BODY:<body_id>` | 由模型定义，质心为原点（可配置为结构原点） |
| 关节系 | `JOINT:<joint_id>` | 关节父侧安装系 |
| 传感器系 | `SENSOR:<sensor_id>` | 相机：+Z 光轴向前、+X 向右、+Y 向下（OpenCV 约定） |
| 站点系 | `SITE:<site_id>` | 地面站/靶标 |

- 变换查询必须显式携带时刻：`T(a → b, t_tai)`。
- 旋转用单位四元数，**顺序 `[x, y, z, w]`**，Hamilton 约定；`q_wb` 表示"把本体系向量转到世界系"：`v_w = R(q_wb) · v_b`。
- 与外部库转换集中在 `srsp::math::from_*`：Eigen 为 `[x,y,z,w]`（一致）；MuJoCo 为 `[w,x,y,z]`；Unity 为 `[x,y,z,w]` 但左手系；ROS 为 `[x,y,z,w]`。转换函数必须有单元测试。

### 2.3 时间

| 项 | 定义 |
|---|---|
| 纪元 | J2000.0 TAI = 2000-01-01T11:59:27.816 TAI = 2000-01-01T11:58:55.816 UTC（对应 TT 2000-01-01T12:00:00；2000 年闰秒差 TAI−UTC = 32 s） |
| 主表示 | `t_tai_ns: int64`，相对纪元的 TAI 纳秒 |
| 辅助 | `t_tdb_s: float64` 供星历使用 |
| 转换 | UTC↔TAI 用内置闰秒表（可更新，需记录版本）；TT = TAI + 32.184 s；TDB↔TT 用简化周期项（精度 < 2 ms，足够可视化；高精度用 CSPICE) |
| 仿真时钟 | `t = t0 + Σ dt`，不读墙钟；倍速仅影响墙钟映射 |

**约定**：所有消息的时间戳为 TAI；星历请求显式使用 TDB；日志同时打印 UTC 便于人读。

### 2.4 标识符与命名

- 实体 ID：`[a-z0-9_.-]{1,64}`，模型内唯一，场景内全局唯一（冲突时加前缀）。
- 话题：`<域>.<对象>[.<实例>]`，如 `sim.state`、`sensor.camera.cam0`、`cmd.actuator`。
- Schema ID：`srsp.<name>/<major>`，如 `srsp.state/1`、`srsp.model/1`。
- 插件 ID：反向域名，如 `org.srsp.dynamics.rigid`。

### 2.5 精度与容差分级

「可信」来自**公开的对照方法与容差**，不是绝对精确。分三级承诺：

| 级别 | 场景 | 判据 |
|---|---|---|
| L1 位级一致 | 同平台、同构建、同输入（`state_hash` 回归） | 逐宏步哈希完全相同 |
| L2 相对容差 | 跨编译器/跨标准库（GCC↔Clang↔MSVC）、跨工具链同场景 | 位置相对误差 ≤ 1e-6；四元数分量 ≤ 1e-9；能量相对 ≤ 1e-8 |
| L3 物理量级容差 | 与外部参考实现（Pinocchio / Orekit / GMAT / Basilisk）对比 | 见下表，按量给出 |

**L3 参考基线**（依据 Basilisk 官方对比，30 天传播：EGM96 8×8 + 日月 + SRP 工况）：三者位置偏差 0.5–1.2 m；无阻力两体 0.02–0.3 m；Molniya 全摄动 7–39 m（原因未定）；400 km 带阻力 17–72 m（主要为离散化步长差异）。因此：

- 轨道位置：LEO 典型工况 30 天 ≤ 10 m；带阻力工况 ≤ 100 m 并在报告中注明步长；
- `M/h/J` 与 Pinocchio：同编译器同 flags 下相对误差 ≤ 1e-10，跨平台按 L2；
- 姿态四元数：与参考实现夹角 ≤ 1e-6 rad（同工况）。

约定：任何对外报告必须写明**参考工具版本、力模型配置、积分器与步长**；不同配置间的差异不当作工具排名。

---

## 3. SRM：空间机器人模型中间表示

### 3.1 为什么自定义

URDF/SDF/MJCF 都无法同时表达：浮基、反作用轮/CMG 动量交换、推力器、柔性附件、在轨捕获接口、传感器安装与空间环境属性。SRM 作为**唯一真源**，外部格式只做导入。

### 3.2 文件组织

`.srm` 是一个 zip 包（可重命名为目录调试）：

```
orbital_servicer.srm/
  model.json            # 主描述
  meshes/base.glb       # 视觉网格
  meshes/collision/*.obj
  materials/*.json      # 可选，PBR 参数
  schema.json           # 可选，随包携带的模式副本
```

### 3.3 主描述结构

```json
{
  "schema": "srsp.model/1",
  "id": "orbital-servicer",
  "name": "Orbital Servicer",
  "revision": "2026.10",
  "units": { "length": "m", "mass": "kg", "angle": "rad" },
  "world": { "frame": "ECI_J2000" },
  "parameters": {
    "panel_length": { "value": 4.2, "unit": "m", "range": [3.0, 6.0] }
  },
  "bodies": [
    {
      "id": "base",
      "parent": null,
      "mass": 850.0,
      "com": [0.0, 0.0, 0.02],
      "inertia": [[620.0, 0.0, 0.0], [0.0, 710.0, 0.0], [0.0, 0.0, 540.0]],
      "visual": { "mesh": "meshes/base.glb", "node": "base_visual" },
      "collision": [
        { "type": "box", "size": [1.2, 1.2, 1.8], "pose": { "r": [0, 0, 0], "q": [0, 0, 0, 1] } }
      ],
      "aero": { "area": 12.0, "cd": 2.2, "reflectivity": 1.3, "solar_area": 9.5 }
    },
    {
      "id": "panel_l",
      "parent": "base",
      "mass": 42.0,
      "com": [0.0, 1.4, 0.0],
      "inertia": [[3.0, 0, 0], [0, 90.0, 0], [0, 0, 92.0]],
      "visual": { "mesh": "meshes/panel.glb", "node": "panel" }
    }
  ],
  "joints": [
    { "id": "panel_hinge_l", "type": "revolute", "parent": "base", "child": "panel_l",
      "axis": [0, 0, 1], "origin": { "r": [0, 0.6, 0], "q": [0, 0, 0, 1] },
      "limit": { "lower": 0.0, "upper": 1.5708 }, "damping": 0.05, "stiffness": 0.0 },
    { "id": "j1", "type": "revolute", "parent": "base", "child": "link1",
      "axis": [0, 0, 1], "origin": { "r": [0.3, 0, 0.4], "q": [0, 0, 0, 1] },
      "limit": { "lower": -3.1416, "upper": 3.1416 },
      "motor": { "gear_ratio": 120.0, "max_torque": 45.0 } }
  ],
  "actuators": [
    { "id": "rw_x", "type": "reaction_wheel", "body": "base", "axis": [1, 0, 0],
      "max_torque": 0.2, "max_momentum": 12.0, "inertia": 0.02 },
    { "id": "thr_px", "type": "thruster", "body": "base",
      "position": [0.6, 0.0, 0.0], "direction": [0, 0, -1],
      "max_thrust": 22.0, "isp": 220.0 }
  ],
  "sensors": [
    { "id": "imu0", "type": "imu", "body": "base",
      "mount": { "r": [0.1, 0, 0.2], "q": [0, 0, 0, 1] },
      "rate_hz": 200, "noise": { "arw": 0.0008, "bias_instability": 0.00002 } },
    { "id": "cam0", "type": "camera", "body": "link6",
      "mount": { "r": [0.05, 0, 0.1], "q": [0, 0, 0, 1] },
      "rate_hz": 10, "intrinsics": { "fx": 900, "fy": 900, "cx": 640, "cy": 480 },
      "resolution": [1280, 960] }
  ],
  "force_models": [
    { "type": "gravity_gradient" },
    { "type": "solar_radiation_pressure", "reflectivity": 1.3 },
    { "type": "aerodynamic_drag", "atmosphere": "nrlmsise00" }
  ],
  "flex": [
    { "id": "panel_l_flex", "body": "panel_l", "modes": 4,
      "frequencies_hz": [0.31, 0.42, 1.8, 2.4], "damping_ratio": 0.005 }
  ],
  "frames": [
    { "id": "ee", "parent": "link6", "pose": { "r": [0, 0, 0.25], "q": [0, 0, 0, 1] } }
  ]
}
```

> 示例省略了机械臂 `link1`–`link6`、网格与材质细节；拓扑由 `bodies[].parent` 与 `joints[]` 共同定义，二者必须一致。

### 3.4 语义要点

| 主题 | 规定 |
|---|---|
| 质量特性 | `mass` 为子体质量（不含子树）；`inertia` 为相对子体质心、子体坐标系的 3×3 对称矩阵 |
| 关节 | 树形拓扑（v1 不支持闭链）；`origin` 为父体系到关节系的变换；`axis` 在关节系中 |
| 浮基 | 根体 `parent: null` 即为浮基；动力学插件负责 6 自由度 |
| 执行器 | 轮/CMG 以动量交换方式进入动力学，不是"外力矩"；推力器产生力+力矩+质量流 |
| 传感器安装 | `mount` 为 body→sensor 的固定变换；量测噪声参数为 1σ |
| 柔性 | 模态频率/阻尼 + 模态参与因子（可选）；不支持时动力学插件须显式报 `unsupported`，不得静默忽略 |
| 单位 | 模型内不写单位后缀，统一 SI；参数可用 `unit` 覆盖 |

### 3.5 导入器

| 格式 | 支持级别 | 已知损失 |
|---|---|---|
| URDF | 完整（含 xacro 预处理） | 无浮基、无轮/推力器、无柔性 |
| SDF | 子集（model/link/joint/sensor） | 插件系统不映射 |
| MJCF | 子集（body/joint/actuator/geom） | 接触参数转为平台模型 |
| STEP/CAD | 仅几何（外部工具） | 质量特性需人工确认 |

导入器必须输出 `import_report.json`：成功项、降级项、丢失项、建议人工修正项。

### 3.6 校验规则（节选）

- ID 唯一；父子关系无环；每个非根体恰有一个入边。
- 惯量矩阵正定，且满足三角不等式（`Ixx+Iyy ≥ Izz` 等），否则警告。
- 关节限位 `lower < upper`；连续关节需显式 `type: continuous`。
- 传感器 `rate_hz` 必须是宏步频率的整数分频或整数倍。
- 所有网格路径存在、单位包围盒合理（警告阈值可配）。

### 3.7 热模型（预留，v1 只校验不计算）

对应 [07 §11](07-roadmap-risks.md) 的 C16 与能力 `srsp.dynamics/thermal@1`（1.2）。v1 解析并校验该段，不参与动力学。

```jsonc
"thermal": {
  "nodes": [
    { "id": "bus", "capacity_J_per_K": 12000, "temperature_K": 293.0,
      "heaters": [{ "power_W": 20, "max_power_W": 60 }] },
    { "id": "camera", "capacity_J_per_K": 400, "temperature_K": 293.0 }
  ],
  "conductances": [
    { "from": "bus", "to": "camera", "W_per_K": 1.5 }
  ],
  "radiators": [
    { "node": "bus", "area_m2": 0.4, "absorptivity": 0.2, "emissivity": 0.85 }
  ]
}
```

- 单位：`K`、`J/K`、`W`、`W/K`、`m²`；
- 辐射面与 `eclipse` 因子、太阳角系数耦合（见 [04 §4.2](04-subsystems-and-stack.md)）；
- 预留期只做 schema 校验与可视化展示，不出温度曲线。

---

## 4. 状态与 Arrow 表

### 4.1 状态表

状态按实体类别分表，均为不可变快照的一部分，随快照一起发布。

**`state.bodies`**

| 列 | 类型 | 语义 |
|---|---|---|
| `id` | `dictionary<int32, utf8>` | body id |
| `pos_w` | `FixedSizeList<double,3>` | 质心在 ECI 的位置 (m) |
| `quat_wb` | `FixedSizeList<double,4>` | `[x,y,z,w]`，body→world |
| `vel_w` | `FixedSizeList<double,3>` | 质心速度在 ECI (m/s) |
| `omega_b` | `FixedSizeList<double,3>` | 角速度在本体 (rad/s) |
| `acc_w` | `FixedSizeList<double,3>` | 可选，仅调试/传感器需要 |
| `alpha_b` | `FixedSizeList<double,3>` | 可选 |

**`state.joints`**：`id, q, qd, tau, motor_tau, limit_flag`
**`state.actuators`**：`id, kind, momentum, speed, thrust, mass_flow, enabled`
**`state.meta`**：单行，`t_tai_ns, step_index, dt, energy{kinetic,potential}, momentum_total`

> 混合约定：线速度在世界系、角速度在体系，可减少一次旋转；插件必须在 `ModelInfo` 中声明其采用的约定，内核负责在跨插件边界转换。

### 4.2 BlobRef 与共享内存

```proto
message BlobRef {
  string shm_name = 1;     // 空表示紧随消息的 inline 数据
  uint64 offset = 2;
  uint64 length = 3;
  string schema_id = 4;    // "srsp.state/1"
  string schema_hash = 5;  // xxh3-128 of canonical schema
  uint64 generation = 6;   // 单调递增，用于无锁读
}
```

- 共享内存段由内核创建，命名 `srsp-<pid>-<uuid>`；生产者在写完后 `release` 递增 `generation`（release 语义），消费者读取前后各校验一次（seqlock）。
- 进程内插件可直接拿到 `ArrowArray*` 指针，无需经过 `BlobRef`；这是唯一允许的"快速路径"差异，语义不变。
- 图像用 Arrow `FixedShapeTensor<uint8>[H,W,C]` + 扩展元数据（`format`、`stride`、`color_space`）。

### 4.3 状态哈希

每个宏步计算 `state_hash = xxh3_128(canonical_bytes(state tables))`，写入 `state.meta`。用途：确定性回归、录制校验、分布式一致性。

---

## 5. 数据总线

### 5.1 话题表（核心）

| 话题 | 方向 | 频率 | Schema | 说明 |
|---|---|---|---|---|
| `sim.state` | 内核 → 全体 | 宏步 | `srsp.state/1` | 状态快照（BlobRef） |
| `sim.time` | 内核 → 全体 | 宏步 | `srsp.time/1` | 时间、步号、倍速 |
| `sim.event` | 内核 → 全体 | 事件 | `srsp.event/1` | 碰撞、捕获、模式切换、故障 |
| `cmd.actuator` | 控制器 → 动力学 | 控制步 | `srsp.cmd/1` | 力矩/轮速/推力指令，带有效期 |
| `env.ephemeris` | 环境 → 全体 | 1–100 Hz | `srsp.ephem/1` | 天体状态、光照方向 |
| `sensor.<type>.<id>` | 传感器 → 消费者 | 按配置 | `srsp.sensor.*/1` | 量测帧 |
| `render.scene` | 内核 → 渲染器 | 变更时 | `srsp.scene/1` | 场景增量 |
| `diag.log` | 全体 → 内核 | 事件 | `srsp.diag/1` | 结构化日志 |
| `team.state` | 内核 → 协同层 | 宏步 | `srsp.team/1` | 成员摘要、链路状态、任务分配 |
| `link.message` | 成员 ↔ 成员 | 事件 | `srsp.link/1` | 链路消息与投递结果（延迟/丢包） |
| `ui.command` | UI → 内核 | 事件 | `srsp.cmd.ui/1` | 用户操作（暂停、注入故障） |

### 5.2 投递语义

- **进程内**：同步回调 + 类型化订阅；订阅顺序 = 注册序号。
- **跨进程**：至少一次；消费者幂等；消息带 `seq`。
- **背压**：每话题有界队列（默认 1024），满时按 `drop_oldest`（传感器/渲染）或 `block`（命令，仅限非实时模式）。
- **不重放历史**：新订阅者只收到订阅后的消息；需要历史时用录制回放。

### 5.3 Schema 注册表

内核启动时加载 `api/schema/registry.json`（由 `tools/codegen` 在构建时根据 `api/schema/` 内容生成并计算哈希；开发期可手写覆盖）：

```json
{
  "schemas": [
    { "id": "srsp.state/1", "file": "state/state_v1.schema.json", "hash": "xxh3:...", "encoding": "arrow" },
    { "id": "srsp.model/1", "file": "model/model_v1.json", "hash": "xxh3:...", "encoding": "json" }
  ]
}
```

插件在 `describe` 中报告其使用的 `schema_hash`；不匹配时拒绝握手，并在诊断中给出"期望/实际"两个哈希及升级建议。

---

## 6. 场景与渲染协议

### 6.1 场景包

```json
{
  "schema": "srsp.scene/1",
  "gltf": "scene.glb",
  "entity_map": {
    "nodes/base_visual": "base",
    "nodes/link1_visual": "link1",
    "nodes/earth": "@environment/earth",
    "nodes/sun": "@environment/sun"
  },
  "environment": {
    "earth": { "texture_set": "blue_marble_500m", "atmosphere": true, "night_lights": true },
    "stars": { "catalog": "hipparcos", "limiting_magnitude": 6.5 },
    "lights": [{ "type": "sun", "source": "ephemeris:sun", "intensity": 1361.0 }]
  },
  "materials_override": {
    "solar_panel": { "base_color": [0.02, 0.05, 0.18, 1.0], "metallic": 0.1, "roughness": 0.35 }
  }
}
```

- glTF 用 GLB（单文件，内嵌贴图），遵循 glTF 2.0；扩展放 `extras.srsp`。
- 坐标：glTF 为 Y-up、右手；平台世界为 ECI Z-up。加载时对根节点施加固定基变换，**不得**在运行时逐帧转换。
- 实体映射用节点路径而非节点索引（索引易变）。

### 6.2 帧增量

```proto
message SceneDelta {
  uint64 generation = 1;
  int64  t_tai_ns = 2;
  Vec3d  origin_eci = 3;          // 近端浮动原点
  Camera camera = 4;
  repeated EntityDelta entities = 5;
  repeated Overlay overlays = 6;
  repeated LightDelta lights = 7;
  Selection selection = 8;
}

message EntityDelta {
  string id = 1;
  Vec3f  pos_rel = 2;             // 相对 origin_eci
  Quatf  quat_wb = 3;
  bool   visible = 4;
  uint32 highlight = 5;           // 0=无 1=选中 2=悬停 3=告警
  string material_override = 6;
}
```

- **双原点渲染**：近端（机器人、目标、轨迹）以机器人为原点；远端（地球、月球、太阳、星空）以天体中心为原点单独一遍渲染，再用深度合成。理由：float32 在 7000 km 处的最小分辨率约 1 m，地球表面会抖动。
- **深度**：反转 Z + 无限远平面（数值上以 1e12 m 作为远平面）；近裁面 1 cm。
- **插值**：渲染帧与宏步不同步时，用 `pos_rel`/`vel_w` 线性外推（可配置关闭，默认开）；外推上限 1 帧。
- **覆盖层**：轨迹折线（抽稀后 ≤ 20k 点）、坐标系三轴、传感器视锥、标签、力/力矩箭头、捕获包络。

### 6.3 离屏成像

```proto
message CaptureRequest {
  string view_id = 1;             // 主视口或传感器相机
  uint32 width = 2;
  uint32 height = 3;
  CaptureFormat format = 4;       // RGBA8 / RGB8 / DEPTH32F / SEGMENTATION / ID
  CameraIntrinsics intrinsics = 5;
  uint32 frame_index = 6;
}
```

- 返回 `ImageRef{BlobRef, width, height, stride, format, t_tai_ns, frame_index}`；
- 允许 1–3 帧流水线延迟，但必须回带产生该图像的仿真时刻；
- 分割/ID 通道用于感知算法与数据标注，不经过色调映射；
- 深度为相机系线性深度（m），非非线性 Z。

### 6.4 渲染器能力声明

`RendererInfo` 必须报告：后端（D3D11/D3D12/Vulkan/GL/GLES/Metal/WebGPU）、最大视口数、是否支持多原点、阴影、后处理、计算着色、离屏、拾取、最大纹理尺寸、GPU 显存预算。

---

## 7. 传感器帧

统一信封：

```proto
message SensorFrame {
  string sensor_id = 1;
  int64  t_tai_ns = 2;
  uint64 seq = 3;
  bool   valid = 4;
  string status = 5;          // OK / SATURATED / ECLIPSE / DROPOUT
  string schema_id = 6;       // srsp.sensor.camera/1 等
  BlobRef data = 7;
  string meta_json = 8;       // 温度、增益、积分时间等
}
```

| 类型 | 数据 | 关键元数据 |
|---|---|---|
| IMU | `acc[3], gyro[3]` | 量程、带宽、温度 |
| 星敏 | `quat_bi[4], valid, n_stars` | 曝光、星数、残差 |
| 相机 | 图像张量 | 内参、曝光、增益、位深 |
| LiDAR | 点云 `(x,y,z,intensity,ring,t)` | 扫描模式、线数、量程 |
| GNSS | `pos[3], vel[3]` | 可见星数、几何因子 |
| 太阳敏感器 | `sun_b[3], eclipse` | 入射角、强度 |
| 力/力矩 | `wrench[6]` | 量程、温度 |
| 关节编码器 | `q[n]` | 分辨率、量化误差 |

所有噪声使用内核注入的确定性随机流；同一 `(seed, sensor_id, seq)` 必须复现同一噪声。

---

## 8. 项目与场景文件

**项目 `.srproj`**（人读、可 diff）：

```json
{
  "schema": "srsp.project/1",
  "name": "capture_demo",
  "scenario": "scenarios/capture.json",
  "requires": {
    "srsp.dynamics/solver": "^1.0",
    "srsp.env/ephemeris": "^1.0",
    "srsp.render/renderer": "^1.0"
  },
  "plugins": {
    "org.srsp.dynamics.rigid": { "step": 0.001, "integrator": "rk4" },
    "org.srsp.env.spice": { "kernels": ["data/kernels/de440.bsp"] }
  },
  "render": { "quality": "high", "shadows": true, "hdr": true },
  "recording": { "enabled": true, "path": "runs/${timestamp}.mcap", "topics": ["*"] },
  "seed": 20261020
}
```

**场景 `scenario.json`**（状态与事件）：

```json
{
  "schema": "srsp.scenario/1",
  "epoch_utc": "2026-10-20T12:00:00Z",
  "initial_state": {
    "orbit": { "type": "tle", "line1": "1 25544U ...", "line2": "2 25544 ..." },
    "attitude": { "mode": "ric", "yaw": 0.0 },
    "joints": { "j1": 0.0, "j2": -1.2 }
  },
  "environment": { "sun": true, "earth_albedo": 0.3, "atmosphere": "nrlmsise00" },
  "events": [
    { "t": 30.0, "type": "script", "hook": "scripts/approach.py" },
    { "t": 120.0, "type": "command", "topic": "cmd.actuator", "payload": { "j1": 0.4 } }
  ]
}
```

- 路径相对项目根；`${timestamp}` 等变量由内核展开。
- 事件按 `t` 排序，同一时刻按文件顺序；运行时插入的事件必须带 `seq` 保持确定性。

**多机器人团队 `teams[]`**（同一场景内 N 个机器人协同）：

```json
{
  "entities": [
    { "id": "robot_a", "model": "models/sat_a.srm", "initial_state": { } },
    { "id": "robot_b", "model": "models/sat_b.srm", "initial_state": { } }
  ],
  "teams": [
    {
      "id": "team_1",
      "members": ["robot_a", "robot_b"],
      "links": [
        { "from": "robot_a", "to": "robot_b", "delay_ns": 100000000,
          "bandwidth_bps": 9600, "loss_rate": 0.0, "queue_capacity": 64 }
      ],
      "coordinator": { "capability": "srsp.control/coordinator@1", "params": { } }
    }
  ]
}
```

- `robot_id` 即实体 `id`，全局唯一；链路为**有向边**，未声明即不可达；
- `delay_ns` 为整数纳秒；`loss_rate` ∈ [0,1]；
- 调度顺序：宏步内按 `robot_id` 字典序更新单体，协同层在单体之后运行一次，顺序写入 `state.meta.schedule_order`；
- `teams[]` 省略时等价于每个实体各自独立（旧场景文件保持兼容）。

---

## 9. 录制与回放

### 9.1 容器：MCAP

| 项 | 取值 |
|---|---|
| 文件 | `*.mcap`，每 4 MiB 一个 chunk，zstd 压缩（level 3） |
| 通道 | 每话题一个 channel；`message_encoding` = `protobuf` / `arrow` / `json` |
| Schema | 内嵌 FileDescriptorSet / Arrow schema / JSON Schema，离线可解析 |
| 索引 | 时间索引 + 每通道消息索引，支持按时间窗加载 |
| 元数据 | `run.json` 作为 attachment 写入：构建版本、git hash、插件版本、种子、硬件、模型哈希 |

### 9.2 回放

- 回放是**一种时钟源**：`ReplayClock` 按录制时间戳驱动调度器；
- 回放时禁用物理计算，状态由录制直接注入；控制器/传感器可重算（用于"同状态不同算法"对比）；
- 支持变速、跳转、逐帧、区间裁剪导出；
- 回放校验：逐宏步比较 `state_hash`，不一致时定位到首个偏离的表/实体。

### 9.3 分析

- `srsp-cli export --format parquet|csv` 按话题导出，直接用 DuckDB/Python/pandas 分析；
- 内置曲线视图订阅任意数值话题；
- 与 Foxglove Studio 兼容：MCAP + protobuf schema 可直接打开。

---

## 10. 版本演进

| 变更类型 | 允许 | 处理 |
|---|---|---|
| 新增可选字段 | 任意版本 | 旧消费者忽略 |
| 新增枚举值 | 次版本 | 未知值按 `UNKNOWN` 处理 |
| 字段语义变更 | ❌ | 提升主版本，新增消息类型 |
| 删除字段 | ❌ | 标记 deprecated，主版本移除 |
| 新增表/列 | 次版本 | 列可空，消费者按列名查找 |
| 修改单位 | ❌ | 视为语义变更 |

**迁移**：`srsp-cli migrate --from 0.9 --to 1.0 project.srproj`，迁移脚本随版本发布，过程写 `migration.log`。

---

## 11. 校验工具链

| 工具 | 作用 |
|---|---|
| `srsp-cli validate model x.srm` | JSON Schema + 语义规则（惯量、拓扑、限位） |
| `srsp-cli validate project x.srproj` | 引用完整性、插件可用性、能力版本 |
| `srsp-conformance --capability srsp.dynamics/solver` | 契约一致性 |
| `srsp-cli schema-hash` | 生成/比对 schema 哈希 |
| `srsp-cli replay-diff a.mcap b.mcap` | 回放一致性比对 |
| CI `validate-docs` | 文档中的 JSON/YAML 片段可解析 |

**约定**：模式文件是唯一真源；文档中的示例由 CI 抽取并校验，避免文档漂移。

---

## 12. 待定项（M0 前必须关闭）

1. 柔性附件是否进入 SRM 1.0，还是 1.1 再引入（影响动力学接口与验证范围）。
2. 关节坐标系定义与 Pinocchio/URDF 的精确对照表（需一组黄金模型做数值比对）。
3. `quat_wb` 与角速度约定的最终选择（当前：世界线速度 + 体系角速度）。
4. 图像色彩空间与 HDR 管线（线性 sRGB → 显示 sRGB 的转换位置）。
5. MCAP schema 编码对 Arrow 的支持方式（内嵌 IPC schema 还是自定义描述）。
