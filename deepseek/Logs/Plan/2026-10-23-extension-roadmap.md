# 方案：竞品优势能力 → 预留扩展点与演进目标

- 日期：2026-10-23
- 状态：已确认（文档已同步；不进入 v1 开发范围）
- 提出人：用户
- 关联文档：[仿真工具调研](2026-10-23-simulator-landscape.md)、[规划优化](2026-10-23-roadmap-optimization-plan.md)、[07 路线图 §11](../../../docs/07-roadmap-risks.md)、[02 插件契约 §13](../../../docs/02-plugin-contract.md)、[01 架构 §1.3](../../../docs/01-architecture.md)

## 1. 目标与边界

用户要求：把调研中竞品的**所有优点功能**纳入设计视野，作为**后续优化目标**；**当前不开发**，但架构中必须**保留接口与扩展点**，保证未来可平滑接入。

原则：

1. **只留接口，不写实现**：v1 不增加任何竞品专属代码；预留以「能力 ID + ABI 追加位 + core_api 保留签名 + 数据契约字段」的形式存在；
2. **预留必须可验证**：占位能力被调用时返回 `SRSP_E_UNSUPPORTED`，不得静默成功（现有一致性测试已覆盖该语义）；
3. **每条能力有触发条件**：不设日期承诺，设「需求信号 / 生态成熟度」触发；
4. **不与 v1 目标冲突**：GPU-RL 训练仍是非目标（01 §1.3），只保留互操作与批量接口。

## 2. 六类扩展机制（E1–E6）

| 编号 | 机制 | 位置 | 说明 |
|---|---|---|---|
| E1 | 能力 ID 命名空间预留 | `docs/02 §13` + manifest schema | 先占 ID 与版本语义（`srsp.<域>/<名>@major`），实现为空 |
| E2 | ABI 追加位 | `plugin_api.h` | 结构体只追加；`srsp_host_v1`/`srsp_plugin_v1` 的后续字段按此扩展 |
| E3 | 宿主 API 保留签名 | `core_api.h` | 批量步进、异步任务、事件流等以「draft 预留」注释登记，未实现前返回 `SRSP_E_UNSUPPORTED` |
| E4 | 数据契约预留 | `docs/03` | schema ID 与元数据字段（如 `meta_json`、`BlobRef.schema_id`）可承载未来载荷，不需要改消息结构 |
| E5 | 传输升级点 | `docs/02 §7` | 边车已留 `step_batch`、共享内存 `BlobRef`、`AttachShm` 角色；GPU/大流量走同一通道 |
| E6 | 分发与注册 | `docs/06` + `ai-models`/release 流程 | `.srpkg` 平台标签、插件仓库（预留 `srsp-registry`） |

## 3. 竞品优势 → 预留映射（15 项）

| # | 竞品优势 | 代表 | v1 现状 | 预留接口（E#） | 目标版本 | 触发条件 |
|---|---|---|---|---|---|---|
| C1 | GPU 批量并行仿真 | MJX、Isaac Lab、Genesis | 无 | E1 `srsp.dynamics/batch_solver@1`；E3 批量步进签名；E5 `step_batch` 已有 | 2.0 | 用户提出 RL/参数扫描吞吐需求，或 MJX 生态稳定 |
| C2 | 可微分物理 | Genesis、Brax | 无 | E1 `srsp.dynamics/differentiable@1`；E4 状态表预留梯度元数据 | 2.x | 研究合作或优化类需求 |
| C3 | 高保真接触（软接触/椭圆摩擦锥/抓持） | MuJoCo | M3 自研基础接触 | E1 `srsp.dynamics/contact@1` 扩展；`dynamics.mujoco` 边车（M1 原型） | 1.1 正式 | M1 边车原型达标 |
| C4 | 柔性/绳索/流体 | MuJoCo、Isaac | 柔性 1.1 规划 | E4 SRM `flex` 段已预留；E1 `flex` 能力 | 1.1 / 2.0 | 柔性附件需求（已在 03 §12.1） |
| C5 | 传感器高保真（RTX LiDAR、运动模糊、噪声模型） | Isaac | 相机/IMU 基础 | E1 `srsp.sensor/device@1` 多类型；E4 `meta_json` 承载扩展元数据 | 1.3 | 感知算法验证需求 |
| C6 | 合成数据管线（自动标注、域随机化） | Isaac | 无 | E1 `srsp.io/dataset@1`；E4 MCAP attachment 存标注 | 1.3 | 数据/标注团队需求 |
| C7 | RL 训练环境（Gym API） | Isaac Lab、MJX | 无 | E3 Python SDK 预留 `srsp.gym` 适配层 | 1.3 / 2.0 | 与 C1 同步 |
| C8 | 策略部署（ONNX/TorchScript） | Isaac | 无 | E1 `srsp.control/policy@1`（控制器能力的一种） | 1.3 | 用户需要在环部署策略 |
| C9 | 任务级分析（覆盖/可见性/机动规划/碰撞预警） | STK | 无 | E1 `srsp.analysis/{coverage,access,conjunction,maneuver}@1` | 1.2 | 任务分析需求 |
| C10 | 高精度星历/时间/大地测量 | Orekit、SPICE | M2 `env.spice` | 现有 `env.*` 能力 + E4 | v1（M2） | 已在计划内 |
| C11 | 多航天器/星座 | STK、Isaac | 多实体场景已支持 | E4 场景 `entities[]`、`sim.state` 多实体 | 1.1 | 星座/编队需求 |
| C12 | FSW 对齐与 HIL | Space ROS | 无 | E1 `srsp.io/fsw_bridge@1`、`srsp.io/hil@1`；TimeService 外部时钟模式已设计 | 1.2 | 与飞控团队合作 |
| C13 | 地球 3D Tiles / 影像地形 | Cesium | v1 8K 贴图 + 解析 eclipse | E1 `renderer` 能力扩展字段（terrain provider）；E6 数据包 | 1.2 | 可视化质量要求 |
| C14 | 影视级渲染 / 远程 Web 查看 | UE、CesiumJS | bgfx + Qt | E1 `renderer` 已是插件；E1 `srsp.ui/panel@1` Web 面板 | 2.0 | 市场/演示需求 |
| C15 | 生态：Python-first / ROS 2 / 插件市场 / 云端 | 各家 | Python SDK 规划中 | E1 `srsp.io/rosbridge@1`；E6 `srsp-registry`；E3 远程 `srspd` | 1.2 / 2.0 | 社区或企业集成需求 |

## 4. 与 v1 非目标的关系

- 01 §1.3 已声明「不做 GPU 并行 RL 训练平台」；本方案在其后追加一句：**保留互操作与批量接口（C1/C7），训练本身属外部生态**；
- 其余能力均为「未来目标」，不进入 M0–M3 范围，但占位 ID 与扩展位在契约中固定，避免将来改契约。

## 5. 落地方式（本次已做）

| 文档 | 改动 |
|---|---|
| 07 | 新增 §11「对标增强与预留接口」：E1–E6 机制表 + C1–C15 映射表 + 触发条件 |
| 02 | 新增 §13「预留能力与扩展点」：占位能力清单、调用约定（未实现返回 `SRSP_E_UNSUPPORTED`） |
| 01 | §1.3 GPU-RL 非目标补一句「保留互操作与批量接口」 |
| 04 | §14.3 指向 07 §11（差异化与增强路线） |

## 6. 风险

| 风险 | 说明 | 应对 |
|---|---|---|
| 预留变成"空头承诺" | 只占 ID 不实现，用户误以为可用 | manifest 与文档统一标注 `reserved`；调用返回 UNSUPPORTED 并提示版本 |
| 预留设计错位 | 未来实现时发现接口不合适 | 预留只用「能力 ID + 追加位 + 既有字段承载」，不改现有结构 |
| 范围蔓延 | 15 项看起来都想要 | 每项有触发条件；未触发不排期 |

## 7. 执行记录

| 日期 | 动作 | 结果 |
|---|---|---|
| 2026-10-23 | 汇总竞品优势 15 项，定义 E1–E6 扩展机制并写入文档 | 07 §11、02 §13、01 §1.3、04 §14.3 已同步 |

## 8. 修订记录

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-10-23 | V1 | 初稿：15 项能力映射、6 类扩展机制、触发条件 |
