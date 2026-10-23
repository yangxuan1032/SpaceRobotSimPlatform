# 调研：机器人 / 空间环境 / 航天器仿真软件现状（2026-10）

- 日期：2026-10-23
- 状态：已完成（调研报告，供 SRSP 定位参考）
- 提出人：用户
- 关联文档：[01 架构](../../../docs/01-architecture.md)、[04 技术栈](../../../docs/04-subsystems-and-stack.md)、[07 路线图](../../../docs/07-roadmap-risks.md)、[架构与数据交互总览](2026-10-23-architecture-summary.md)

## 1. 调研范围与方法

按「通用机器人仿真 / 空间机器人与在轨服务 / 轨道与航天器 GNC / 可视化与地球渲染」四类检索公开资料（官方文档、论文、工具站），重点看三件事：**领域覆盖**（机器人多体+接触 vs 轨道+环境）、**物理与数值可信度**、**可视化与工程交付能力**。版本与许可细节以各官方发布为准。

主要来源：[mjorbit 论文（ISPARO 2026）](https://arxiv.org/abs/2609.08010)、[Space ROS（NASA NTRS）](https://ntrs.nasa.gov/api/citations/20220017761/downloads/Space_ROS_SciTech.pdf)、[Isaac Lab 文档](https://isaac-sim.github.io/IsaacLab/main/)、[Gazebo 发布页](https://gazebosim.org/docs/acropolis/releases/)、[Basilisk 与 GMAT/Orekit 精度对比](https://avslab.github.io/basilisk/develop/Support/User/accuracyComparison.html)、[Nyx Space 论文](https://nyxspace.com/assets/pdf/2021.07-ieee_nyx.pdf)、[OnOrbitROS（Cranfield）](https://dspace.lib.cranfield.ac.uk/server/api/core/bitstreams/9b21d895-7f94-48f4-be9c-56032fcaa5a5/content)、[SAOV 用户指南](https://ascent.cysun.org/project/resource/get/733?inline=true)、[OpenGNC](https://pypi.org/project/OpenGNC/)、[ADCSim 论文](https://www.sciencedirect.com/science/article/pii/S2352711026001093)。

## 2. 分类总览

| 类别 | 代表工具 | 强项 | 短板（相对空间机器人任务） |
|---|---|---|---|
| A 通用机器人仿真 | MuJoCo/MJX、Isaac Sim + Isaac Lab、Gazebo、PyBullet、Drake、Webots、CoppeliaSim、Genesis | 接触/摩擦/多体、GPU 并行、RL 生态 | 无轨道/空间环境；尺度在米级，轨道尺度与光照/星历缺失 |
| B 空间机器人与在轨服务 | Space ROS、mjorbit、OnOrbitROS、Astrobee 仿真、AGILESim 类任务仿真 | 面向 OOS/装配任务、与飞行软件对齐 | 多为研究原型，可视化与工程交付弱；环境模型简 |
| C 轨道与航天器 GNC | STK（Ansys）、GMAT、Orekit、Basilisk、Tudat、Nyx、FreeFlyer、Simulink Aerospace Blockset、SPICE | 高精度轨道/姿态/环境，验证方法论成熟 | 机器人本体多体与接触弱，通常无实时可视化 |
| D 可视化与地球渲染 | Cesium（CesiumJS / Cesium for Unreal·Unity）、STK 可视化、UE/Unity 自建 | 地球/轨道尺度渲染、影像地形 | 不含动力学，需与 A/C 类拼接 |

## 3. A 类：通用机器人仿真

| 工具 | 定位与要点 |
|---|---|
| **MuJoCo / MJX** | 接触与刚体动力学的事实标准之一，2021 年开源后生态爆发；MJX 提供 GPU/JAX 路径；模型格式 MJCF，Python 绑定成熟。**mjorbit 正是基于 MuJoCo 扩展空间动力学**（见 §4） |
| **Isaac Sim / Isaac Lab** | NVIDIA Omniverse 上的机器人仿真与训练平台，PhysX 5 + RTX 渲染；Isaac Lab 提供任务/环境抽象与并行训练；近年加入具身智能数据管线（[Isaac Lab 文档](https://isaac-sim.github.io/IsaacLab/main/)）。强项是 GPU 吞吐与传感器渲染，弱项是轨道尺度与空间环境 |
| **Gazebo（Harmonic/Ionic 代）** | ROS 生态默认仿真器，插件化世界/传感器；有 Space ROS 在其上构建飞行软件栈（[Gazebo 发布页](https://gazebosim.org/docs/acropolis/releases/)） |
| **PyBullet** | 轻量、易嵌入，教学/快速原型仍常见；维护活跃度下降，接触质量弱于 MuJoCo |
| **Drake** | MIT 系，优化控制与多体建模见长，适合 GNC 算法原型，但可视化与资产管线弱 |
| **Webots / CoppeliaSim** | 教学与工业原型友好，内置传感器模型；CoppeliaSim 商业授权 |
| **Genesis / Newton 等新引擎** | 2024–2025 年出现的生成式/新一代物理引擎（Genesis；NVIDIA+DeepMind+Disney 的 Newton），强调 GPU 与可微/可并行，**成熟度与长期维护仍需观察** |

## 4. B 类：空间机器人与在轨服务

| 工具 | 定位与要点 |
|---|---|
| **mjorbit**（CMU，ISPARO 2026） | 在 MuJoCo 上补齐**航天器动力学、执行器与传感器**，给出 CPU 低延迟与 GPU 高吞吐两套后端，Python API；用 MPC 与 RL 完成多个在轨任务案例（[论文](https://arxiv.org/abs/2609.08010)）。代表「复用成熟机器人引擎 + 空间扩展」的路线 |
| **Space ROS**（NASA + Blue Origin） | 以 ROS 2 为基座、按飞行软件标准裁剪的开源框架，目标是把地面验证代码平滑迁移到飞行（[NASA NTRS](https://ntrs.nasa.gov/api/citations/20220017761/downloads/Space_ROS_SciTech.pdf)）；仿真侧依赖 Gazebo/自身插件 |
| **OnOrbitROS**（Cranfield） | 面向在轨服务（OOS）的 ROS 2 平台，覆盖接近、捕获等任务流程（[论文](https://dspace.lib.cranfield.ac.uk/server/api/core/bitstreams/9b21d895-7f94-48f4-be9c-56032fcaa5a5/content)） |
| **Astrobee / 空间站机器人仿真** | NASA 舱内自由飞行机器人，配套 Gazebo 仿真与 ROS 接口，偏任务验证 |
| **AGILESim 类任务仿真器** | 面向整星任务级仿真（姿态、轨道、载荷），多为机构内部工具，通用性与可扩展性有限（[会议材料](https://indico.esa.int/event/160/contributions/1142/attachments/1103/1305/fioretti_G4SUW2017_AGILESim_final.pdf)） |

**共性判断**：B 类工具「任务像」但「工程化弱」——多体+接触与轨道环境往往只覆盖一侧；可视化多为调试级；缺少统一的模型/场景/录制标准。

## 5. C 类：轨道与航天器 GNC

| 工具 | 语言/许可（以官方为准） | 特点 |
|---|---|---|
| **Ansys STK** | 商业 | 任务级事实标准：轨道、覆盖、可见性、传感器、可视化一体；贵、封闭、插件化有限 |
| **GMAT**（NASA） | 开源 | 任务设计与轨道优化，脚本化强；可视化一般 |
| **Orekit**（CS GROUP） | Java，Apache-2.0 | 高精度轨道/时间/坐标/事件检测库，被大量工具当参考实现 |
| **Basilisk**（CU Boulder AVS Lab） | C++/Python，开源 | 模块化航天器仿真（动力学+环境+FSW 模块），配套 Vizard 可视化；**公开与 GMAT/Orekit 的精度对比方法**（[对比页](https://avslab.github.io/basilisk/develop/Support/User/accuracyComparison.html)） |
| **Tudat**（TU Delft） | C++，开源 | 天体力学/轨道确定，研究向 |
| **Nyx** | Rust，开源 | 高性能、确定性友好的航天动力学与任务设计（[论文](https://nyxspace.com/assets/pdf/2021.07-ieee_nyx.pdf)） |
| **FreeFlyer**（a.i. solutions） | 商业 | 任务分析与飞行动力学，航天界常用 |
| **Simulink Aerospace Blockset** | 商业 | 控制律设计/DQ 验证的标准链路，与 MATLAB 生态绑定 |
| **SPICE（NAIF）** | 免费 | 星历/时间/坐标内核事实标准，几乎所有高精度工具都读 SPK/PCK/LSK |

**精度参考**（Basilisk 官方对比，Orekit 13.1 / GMAT R2026a）：30 天传播，无阻力情形三者在**米级**一致（LEO 两体 0.02–0.3 m，EGM96 8×8 约 0.6–1.1 m，SRP 约 0.5–1.2 m）；Molniya 全摄动出现 7.3–39 m 分歧；400 km 阻力情形因离散化差 17–72 m。结论：**高精度轨道工具之间也是「米级一致 + 特定工况分歧」**，SRSP 的容差策略应以此为准。

## 6. D 类：可视化与地球渲染

- **Cesium**（CesiumJS / Cesium for Unreal·Unity）：地球瓦片、地形、影像、轨道可视化的事实标准，适合「轨道尺度 + 真实地球」；不含动力学。
- **STK 可视化**：与轨道计算一体，商业。
- **UE/Unity 自建**：影视级画面，工程量与实时性权衡。
- **SAOV 类轻量工具**：卫星姿态与轨道可视化，教学/演示向（[用户指南](https://ascent.cysun.org/project/resource/get/733?inline=true)）。

## 7. 横向对比

| 维度 | MuJoCo/Isaac | Gazebo+Space ROS | STK/GMAT/Orekit | Basilisk | mjorbit | **SRSP（目标）** |
|---|---|---|---|---|---|---|
| 多体+接触 | ★★★ | ★★ | ☆ | ★★ | ★★★ | ★★★（规划） |
| 轨道/空间环境 | ☆ | ★ | ★★★ | ★★★ | ★★ | ★★★（规划） |
| 传感器/相机成像 | ★★★（Isaac） | ★★ | ★ | ★ | ★★ | ★★（规划） |
| 实时可视化 | ★★ | ★★ | ★★★ | ★★（Vizard） | ★ | ★★★（规划，双原点） |
| 插件/扩展 | ★★ | ★★★ | ★ | ★★ | ★ | ★★★（能力寻址） |
| 确定性回放 | ★ | ★ | ★★ | ★★ | ★★ | ★★★（state_hash） |
| 工程交付（安装包/离线） | ★ | ★★ | ★★★ | ★ | ★ | ★★（规划） |
| 许可开放性 | 开源为主 | 开源 | 商业为主 | 开源 | 开源 | 自研开源 |

## 8. 对 SRSP 的判断

1. **空白点确实存在**：同时具备「多体+接触动力学 + 空间环境 + 实时可视化 + 插件生态」的开源平台很少。A 类无轨道，C 类无机器人，B 类是研究原型。
2. **最大竞争不是 STK，而是 mjorbit + Isaac/MJX 路线**：它用成熟引擎换来了接触与 GPU 吞吐，2026 年已发论文并在做 RL/MPC 案例。SRSP 不应在「GPU 并行 RL 吞吐」上竞争。
3. **SRSP 的差异化应压在三点**：① 工程级确定性（`state_hash` 逐宏步回放）与可交付（离线安装包、`.srpkg`）；② 空间环境与轨道尺度可视化的原生整合（双原点、eclipse、星历）；③ 能力寻址插件 + 双传输，让院所把既有算法（Python/MATLAB/C++）接进来而不改平台。
4. **可借鉴的做法**：
   - Basilisk 的**精度对比方法论**（同工况、同模型参数、公开偏差表）——SRSP 应把「与 Pinocchio/Orekit 对比」做成可复现脚本而非一次性报告；
   - mjorbit 的**引擎复用策略**——SRSP 的 `dynamics.mujoco` 边车插件方向正确，不必自研所有接触；
   - Space ROS 的**飞行软件对齐**——SRM/场景/MCAP 已具备，下一步可定义「FSW 接口映射」；
   - Nyx 的 **Rust + 确定性** 思路——与 SRSP 的 Rust CLI 决策一致。
5. **风险**：Isaac/Newton 生态迭代极快，若 SRSP 的可视化与传感器成像在 1.0 前跟不上，用户会倾向「Isaac 做感知 + STK 做轨道」的组合；因此 M1 的「能看、能录、能回放」必须按时兑现。

## 9. 执行记录

| 日期 | 动作 | 结果 |
|---|---|---|
| 2026-10-23 | 检索四类仿真软件公开资料并整理对比 | 完成；结论已用于定位判断 |

## 10. 修订记录

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-10-23 | V1 | 初稿：四类工具清单、横向对比、与 SRSP 的差异化判断与可借鉴点 |
