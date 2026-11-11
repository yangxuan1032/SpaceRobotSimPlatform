# 模块功能总览（代码分析）

- 状态：v0.1（2026-11-01）
- 依据：`src/`、`apps/`、`bench/`、`tests/`、`api/` 现有代码
- 关联：`docs/01-architecture.md`（分层与 ADR）、`docs/02`（插件契约）、`docs/03`（数据契约）

## 1. 总体分层

```text
apps/           可执行程序：studio（可视化）、cli（验证/soak）、sidecar（隔离宿主）
src/core        内核：C ABI 表面、插件宿主、调度、快照、事件、信任、Blob
src/model       SRM / URDF / MJCF 导入，模型 IR
src/dynamics    刚体动力学：FK、Jacobian、M、RNEA、积分、执行器、环境力
src/render      渲染抽象 + 后端：null / qt（QPainter）/ rhi（QRhi D3D11）
src/record      MCAP 子集：状态通道、Attachment、Summary、state_hash
src/bridge      gRPC 边车：控制面 protobuf、数据面共享内存
bench/          性能基准：dynamics、record、rhi
tests/          契约与 ABI 测试：布局、错误码、不支持能力
```

## 2. 逐模块职责

### 2.1 `src/core`（内核）

| 文件 | 职责 |
|---|---|
| `include/srsp/core_api.h` | **C ABI 表面**：`srsp_context`、`srsp_plugin_call`、`srsp_buffer`、错误码；所有跨边界结构体带 `struct_size` |
| `src/core_api.cpp` | ABI 实现转发到 C++ 内核；缓冲区经 `alloc/free` 由宿主管理 |
| `core/plugin_host.*` | 加载 `.dll/.so`、校验 manifest（api 主版本、`trust_level`、平台库路径）、`init/call/shutdown` 生命周期 |
| `core/scheduler.*` | 固定步长循环，回调注入；不持有时钟所有权 |
| `core/time_service.*` | TAI ns 单调时钟，供确定性与录制使用 |
| `core/snapshot.hpp` | `SnapshotId{step_index, generation}` + lease；旧代不可读新数据 |
| `core/event.hpp` | `EventKey(t, priority, source_id, seq)` 全序 + 有界 `CommandQueue`（满则拒绝，不静默丢） |
| `core/trust.hpp` | `TrustLevel T0–T3`；仅 T0/T1 允许进程内加载 |
| `core/blob.hpp` | `BlobStore`：generation 校验 + 共享 lease，供大载荷引用 |
| `core/registry.hpp` | 能力寻址（`srsp.dynamics/solver@1` 形式）与版本匹配 |
| `core/diagnostics.*` | 结构化诊断（code/message/detail），供宿主聚合 |

### 2.2 `src/model`

| 文件 | 职责 |
|---|---|
| `srm.*` | SRM 文档解析与校验（`schema: srsp.model/1`），产出 `Model`（体、关节、惯量、执行器、传感器） |
| `urdf_importer.*` | URDF → `Model`；零位与关节 frame 按 `docs/03` §4.1.1 |
| `mjcf_importer.*` | MJCF → `Model`（子集） |

### 2.3 `src/dynamics`

> 2026-11-03（S1）：实验性 `aba_accelerations` 已移除，`spatial.hpp` 降为 `src/` 内部头；`set_solver(kArticulated)` 返回 false。

| 文件 | 职责 |
|---|---|
| `rigid_body.*` | 浮动基刚体树：`forward_kinematics`、`jacobian`、`mass_matrix`（祖先列裁剪）、`mass_matrix_reference`（对照）、`gravity_force`、`bias_forces`（RNEA）、`step`（RK4 / 半隐式）、执行器限幅、`generalized_force` |
| `force_models.*` | 环境力：重力、气动阻力、太阳光压等可组合力模型 |
| `spatial.hpp` | 空间代数原语（惯量、动能、`v×*Iv`、`v×v`），ABA 的前置件 |

**已知**：`step` 的加速度仍由 `M⁻¹(τ−h)` 提供；ABA 设计见 `2026-11-01-aba-design.md`（未启用）。

### 2.4 `src/render`

| 文件 | 职责 |
|---|---|
| `include/srsp/render/renderer.hpp` | 渲染器接口：`initialize/resize/set_camera/render_frame/shutdown` + `RendererCapabilities` |
| `include/srsp/render/mesh_source.hpp` | `mesh_uri` → `MeshSource{sphere,box,gltf}`，Qt/RHI 共用 |
| `include/srsp/render/scene.hpp` | 场景描述：`Sphere{position, radius, color, label, mesh_uri, material_id, orientation}`、地球、星空、日照 |
| `include/srsp/render/projection.hpp` + `projection.cpp` | 双原点投影（`project(camera, viewport, world, origin)`） |
| `src/null_renderer.cpp` | 无操作后端，供无显示环境与测试 |
| `backends/qt/` | QPainter 软件后端：地球昼夜、星表、球体、程序化 box、文字 |
| `backends/factory/` | `make_renderer(RendererKind)`：auto/qt/rhi/null 的唯一映射点 |
| `backends/rhi/` | QRhi 后端：D3D11 设备、MVP uniform、球体/box/地球/星空、离屏读回；`--renderer rhi` |

### 2.5 `src/record`

| 文件 | 职责 |
|---|---|
| `mcap.*` | MCAP 子集写入/读取：Schema、Channel（metadata 含 `schema_hash`/`abi`）、Message、Attachment、Metadata、Summary/SummaryOffset |
| `blob_recorder.hpp` | `BlobStore` → Attachment + Metadata，写后释放，超限拒绝 |
| `blob_ref_codec.hpp` | `sim.blobref` 28 字节小端编解码 |
| `state_hash.*` | `state_hash` = XXH3-128，用于回放确定性比对 |
| `selection.hpp` | 档位标签：`IntegratorTag`/`SolverTag`/`EncodingTag` 与 `selection_metadata()` |
| `run_metadata.hpp` | 插件锁规范化 + 运行元数据（compiler/build_flags/锁哈希）+ `check_selection()` 回放校验 |

### 2.6 `src/bridge`

| 文件 | 职责 |
|---|---|
| `sidecar_host.*` / `sidecar_process.*` | 边车进程托管与 gRPC 控制面；数据面共享内存 |
| `proto/*.proto` | 控制面消息定义（`api/proto`） |

### 2.7 `apps/`

| 程序 | 职责 |
|---|---|
| `studio` | 四面板 UI（场景树/属性/时间轴/视口）；`--selftest`、`--selftest-ui`、`--renderer rhi`；程序化 box 显示机器人 |
| `cli` | `version`、`demo`（inproc/sidecar）、`soak`（ping/step/record 三种负载） |
| `sidecar` | 独立进程，承载 T2 插件 |

### 2.8 其它

| 目录 | 职责 |
|---|---|
| `bench/` | `bench_dynamics`（100 体链，mass/bias/step）、`bench_record`、`bench_rhi`（1080p 帧时间） |
| `tests/abi/` | ABI 布局静态断言、错误码表、不支持能力 |
| `deepseek/ModuleTest/` | 单元与集成测试（45 项） |
| `tools/` | 环境校验、AI 工作流脚本、soak、schema 哈希、互操作脚本 |

## 3. 数据流（一次仿真步）

```text
scheduler.tick(t) → 采集事件（event.hpp 全序）
  → 插件调用（core_api → plugin_host）→ 力矩/传感器
  → dynamics.step(q, v, tau, dt)      // 当前 M⁻¹(τ−h)
  → snapshot.publish(step_index, state)
  → render.render_frame(scene)         // qt 或 rhi
  → record.write_message(channel, t, state)  // 需要时 + BlobRecorder
```

## 4. 依赖方向（禁止反向）

```text
core ← model ← dynamics ← apps
core ← render ← apps
core ← record ← apps
core ← bridge ← apps
```

`core` 不得依赖任何上层；`render`/`record` 互不依赖；插件的 C ABI 只经由 `core_api.h`。

## 5. 测试覆盖（当前 79 项）

| 模块 | 代表测试 |
|---|---|
| core | `unit.event`、`unit.snapshot`、`unit.trust`、`unit.blob`、`unit.plugin_host`、`unit.registry`、`unit.scheduler` |
| model | `unit.srm`、`unit.urdf`、`unit.mjcf` |
| dynamics | `unit.dynamics`、`unit.dynamics_golden`、`unit.dynamics_spatial` |
| render | `unit.render`、`unit.render_qt_box`、`unit.render_rhi`（D3D11 或 Null） |
| record | `unit.mcap`（含 Attachment/Summary）、`unit.state_hash` |
| apps | `integration.studio_selftest`、`integration.studio_ui`、`integration.studio_ui_rhi` |
| 契约 | `abi.*`、`docs_check` |
## 6. 渲染能力矩阵（P3，2026-11-03）

| 能力 | Qt（软件） | RHI（D3D11） | 说明 |
|---|---|---|---|
| 地球 + 星空 | ✅ | ✅ | 两者均有 |
| 程序化 box（`mesh_uri="box"`） | ✅ | ✅ | 按 `MeshSource` 分发 |
| 程序化球体（空 URI） | ✅ | ✅ | RHI 逐体球体 + 日照着色 |
| 双原点投影 | ✅ | ✅ | 相机原点扣除 |
| 离屏捕获 | ✅ | ✅ | `bench_rhi` 1080p ≈ 5.97 ms |
| glTF 网格 | ✅ | ✅ | 两后端均接入 `load_glb` + 三角面片/平面着色（B5 完成） |
| 文字/标签叠加 | ✅（studio 窗口层） | ✅ **两条路径**：studio 用混合叠加（B11，A4 契约）；`renderer.rhi` 另有管线内文字（B12，进入 capture） | 字形覆盖 ASCII 32–90；小写与符号待补 |
| 截图 / 传感器 capture | renderer-only | renderer-only | **A4 契约**：`capture.hpp` 定义「capture 只含 3D，overlay 由窗口合成」；`unit.capture_scope` 断言 |

规则：表中标记 ❌/⚠️ 的能力不得在验收中按「已支持」计分；新增能力先补本表再加测试。
### 6.1 覆盖测试映射（A2）

矩阵每一行都必须有对应断言；没有测试的行不得留在表里。

| 能力 | 覆盖测试 |
|---|---|
| 后端能力声明（`gpu`/`compute`/`max_viewports`） | `unit.capability_matrix` |
| 组件清单/实体 ID/参数优先级 | `unit.component_manifest`、`integration.cli_component_valid`、`integration.cli_component_invalid` |
| 装配编译/挂载校验 | `unit.assembly` |
| World 多实体/原子提交 | `unit.world` |
| 环境采样/遮挡/二体传播/天体清单/SRP/梯度力矩 | `unit.environment` |
| 传感器采样/相机位姿/拾取 | `unit.sensing` |
| 相机成像（RGBA 帧） | `unit.camera_capture` |
| World 每步性能（16 实体 + J2/三体/SRP/阻力） | `bench.world`（Debug 50.9 µs，p95 54 µs；性能门用 Release） |
| World→订阅→录制链路 | `integration.cli_world`、`integration.cli_world_record`、`integration.cli_world_record_inspect` |
| 地球 + 星空 | `visual.snapshot_qt`、`unit.render_rhi_gltf`（星空：`unit.render_rhi_gltf` 空场景用例） |
| box / 球体分发 | `unit.render_qt_mesh`、`unit.render_rhi_gltf` |
| 双原点投影 | `unit.render_qt_box` |
| 离屏捕获 | `unit.capture_scope`、`visual.snapshot_{qt,rhi}` |
| glTF 网格 | `unit.mesh_loader`、`unit.render_qt_gltf`、`unit.render_rhi_gltf` |
| 管线内文字（RHI） | `unit.render_rhi_text` |
| capture 语义（不含 overlay） | `unit.capture_scope` |
---

## 7. 组件清单（目标形态，2026-11-09）

| 组件 | 状态 | 说明 |
|---|---|---|
| `env.celestial` | 规划 | 天体星历/引力/大气/光照/遮挡 |
| `dynamics.body` | 现有能力 | 由 SRM `Body` 提供，未见独立组件 |
| `dynamics.joint` | 现有能力 | 由 SRM `Joint` 提供 |
| `dynamics.force` | 函数已存在、未接入 | 二体/三体/SRP/阻力 |
| `dynamics.actuator` | 数学已实现、未接入 | `actuator_force()` 无调用者 |
| `dynamics.propulsion` | 规划 | 推进剂、质量/质心/惯量时变 |
| `sensor.camera` | 规划 | 挂载位姿 + 渲染器成像 |
| `sensor.lidar` | 规划 | 方向采样 + 碰撞几何求交 |
| `sensor.radar` | 规划 | 距离-多普勒近似 |
| `control.controller` | 现有（插件） | C hello 与 Python PD 两个实现 |
| `visual.renderer` | 现有（策略） | null/qt/rhi 后端 |

组件规范见 [12 组件标准](12-component-standard.md)。