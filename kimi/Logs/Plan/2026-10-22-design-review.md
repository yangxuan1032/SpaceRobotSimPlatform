# 设计文档评审：重点总结与优化建议（v0.1 基线）

- 日期：2026-10-22
- 状态：规划中（建议待评审采纳）
- 提出人：用户
- 关联文档：[README](../../../README.md)、[01 总体架构](../../../docs/01-architecture.md)、[02 插件契约](../../../docs/02-plugin-contract.md)、[03 数据契约](../../../docs/03-data-contracts.md)、[04 技术栈](../../../docs/04-subsystems-and-stack.md)、[05 MSYS2 环境](../../../docs/05-msys2-dev-environment.md)、[06 打包](../../../docs/06-packaging.md)、[07 路线图](../../../docs/07-roadmap-risks.md)、[M0 执行计划](../../../deepseek/Logs/Plan/2026-10-22-m0-execution-plan.md)

## 1. 背景与目标

对 `docs/01`–`07` 做一次完整重读，目标是：(a) 用一页内容把设计主线说清；(b) 在 M0 契约冻结前找出可以改进的点，按优先级分类，供评审取舍。评审基准是文档本身 + `api/` 现状（`plugin_api.h`、`plugin.proto`、manifest schema）。

## 2. 重点内容总结

### 2.1 设计主线

```
契约先行 ──▶ 平台+插件（能力间接寻址）──▶ 双传输同语义 ──▶ 确定性可重放 ──▶ 可离线交付
```

| 文档 | 重点 |
|---|---|
| 01 架构 | G1–G7 目标与非目标；L0–L3 分层；单进程 v1 + 可跨进程设计；宏步调度循环（事件→传感器→控制器→力模型→动力学→快照）；确定性六措施；故障矩阵；8 项质量指标；10 条 ADR |
| 02 插件契约 | 11 类能力；manifest（id/api/transport/capabilities/requires/permissions）；发现→校验→依赖解析→签名→加载；纯 C ABI 五条约束（struct_size、UTF-8 长度前缀、host->alloc 出参、异常边界、线程约定）；能力方法表（dynamics/force/ephemeris/renderer/sensor）；边车协议（gRPC `PluginRuntime`、token 握手、心跳、批量、shm）；签名+权限+热重载+一致性测试 |
| 03 数据契约 | 单位/坐标系/时间全局约定（TAI ns 主表示、四元数 `[x,y,z,w]` Hamilton）；SRM 自定义 IR（浮基/轮/CMG/推力器/柔性/传感器安装）；Arrow 状态表与 `BlobRef`（xxh3-128、generation seqlock）；话题表与投递语义；场景包 + 双原点 + 反转 Z + 离屏成像；传感器帧统一信封；`.srproj`/scenario；MCAP 录制回放；版本演进规则 |
| 04 子系统 | 动力学（CRBA/ABA、半隐式欧拉默认、四元数 Lie 群更新、与 Pinocchio 黄金对比）；力模型 9 类 + GJK 接触 + 两阶段捕获；环境（CSPICE/ERFA/SGP4/EGM2008/NRLMSISE-00/IGRF）；渲染 bgfx 双原点、帧预算 16.6 ms 分解；UI（Qt Widgets + 局部 QML，面板 8 类）；Python 边车脚本；跨语言接入矩阵；技术栈总表 |
| 05 开发环境 | MSYS2 UCRT64 引导；依赖优先级 pacman > superbuild > 系统；版本锁定与离线策略；仓库结构与 CMake 目标；5 组预设；测试/CI/排错 |
| 06 打包 | 产物矩阵（zip/msi/AppImage/deb/srpkg/whl/sbom）；可重定位布局；DLL 收集 + windeployqt；Inno Setup；minisign 签名与回滚；用户数据目录；体积预算 ≤250 MB；冒烟清单 |
| 07 路线图 | M0–M4 里程碑与出口判据；5–7 人团队模型；R1–R12 风险；质量门禁；性能/稳定性指标；前两周清单；5 项开放决策 |

### 2.2 值得肯定的设计

1. **契约是唯一耦合面**（02 开篇）+ 能力而非插件 ID 寻址（01 §6.1）——替换求解器不改场景，这是整个架构最有价值的决定。
2. **确定性是一等公民**：种子随机流、`state_hash` 回归、`-ffp-contract=off` 进预设、禁用无序遍历，形成闭环。
3. **双传输同一语义**（02 §7 的 `Invoke` 与 C ABI `call` 同构），Python 不内嵌的故障隔离论证充分。
4. **空间领域特化到位**：SRM 对轮/CMG/推力器/柔性的建模、双原点渲染解决 10⁷ m 抖动、eclipse 输出供电源/热/渲染共用（04 §4.2）。
5. **工程化没有拖后**：打包/CI/冒烟从 M0 就排（07 §4），repo 里 `api/` + hello 插件已可编译运行，不是纸面架构。

## 3. 优化建议

### P0：契约正确性（冻结前必须处理）

| # | 位置 | 问题 | 建议 |
|---|---|---|---|
| O1 | 03 §6.2 `EntityDelta` | 文档说插值用 `pos_rel`/`vel_w` 线性外推，但 `EntityDelta` 里**没有速度字段**，外推无数据可用（文档级缺陷） | 补 `Vec3f vel_rel`（与 `pos_rel` 同原点），或明确外推用相邻两帧差分 |
| O2 | 03 §4.3 `state_hash` | 「`xxh3_128(canonical_bytes(state tables))`」未定义 canonical bytes：Arrow 的字典编码顺序、null 位图、padding 都会影响内存字节 | 定义为「逻辑值规范化序列」的哈希（按列名排序、字典按值排序、忽略 padding），并在 `srsp-cli schema-hash` 里实现唯一算法 |
| O3 | `plugin_api.h` `srsp_host_v1.time_tai` vs 03 §2.3 | ABI 返回 `double` 秒，但 03 规定主表示是 `int64` TAI 纳秒；double 在 J2000+26 年只剩 ~0.1 µs 分辨率，与主表示矛盾 | `srsp_host_v1` 增加 `int64 (SRSP_CALL* time_tai_ns)(void*)`，double 版标注 deprecated 或仅作辅助 |
| O4 | 03 §9.1/9.3 vs §12.5 | MCAP `message_encoding="arrow"` 不是 MCAP 官方值，与 §9.3「Foxglove 直开」冲突（Foxglove 只认 protobuf/json/flatbuffer/ros） | 决策 D-5 补充影响面：要么 Arrow 包一层 protobuf bytes（保 Foxglove），要么接受 Foxglove 只能看 protobuf 通道并写进文档 |
| O5 | 01 §6.3 / 04 §2.4 / 07 §3 | 「跨平台相对误差 < 1e-9」「与 Pinocchio 对比 < 1e-10」过强：不同 libm 的超越函数实现差异就会超过这个量级，FMA/向量宽度也会分叉 | 分层承诺：同平台同构建位级一致（回归）；跨平台按量给容差表（位置 1e-6 相对、四元数 1e-9、能量 1e-8）；1e-10 仅作「同编译器同 flags」下的对比目标 |

### P1：契约完备性（建议在 M0 一并定）

| # | 位置 | 问题 | 建议 |
|---|---|---|---|
| O6 | `plugin_api.h` `srsp_host_v1` | host 只有 `publish`，**没有订阅**：插件想听 `sim.event`/`env.ephemeris` 没有 ABI 通道，只能靠内核按调度回调，后期必加字段破坏冻结 | manifest 增加 `subscriptions` 声明，内核按声明路由；或 host 增加 `subscribe/unsubscribe`（v1 结构体内追加，struct_size 机制允许） |
| O7 | 02 `core.checkpoint` vs `dynamics.get_state/set_state` | 两套状态序列化路径并存：checkpoint 的自定义 blob 与 ArrowState 语义重叠，插件作者要维护两份 | 统一：规定 `core.checkpoint` 的载荷**就是** Arrow 状态快照的 IPC bytes；`set_state` 是其逆操作 |
| O8 | 02 §4.5 `srsp_error` | `message`「有效期至下一次调用」在 `thread_safe` 并发调用下无法判定「下一次」，是悬垂指针温床 | 改为：`message` 必须是静态字符串或 `host->alloc` 分配（调用方 `host->free`）；二选一写死 |
| O9 | 02 §3.3 + 03 §8/9 | 场景按能力解析、实现可换，但**没有 lockfile**：两台机器装不同版本插件，同一 `.srproj` 结果不同，回放 `state_hash` 回归会假阳性 | `.srproj` 同级生成 `*.lock.json`：锁实现 ID + 精确版本 + schema 哈希；回放时强制校验，不一致则拒绝或警告 |
| O10 | 02 §8 | 「permissions 运行时强制」对**进程内**插件不可实施（同地址空间无法拦截文件打开） | 明确：permissions 只对边车强制；进程内插件的信任模型 = 签名 + 评审 + `--allow-unsigned` 拒绝，写进 §8 |
| O11 | 03 §3.2 SRM `visual.mesh` vs §6.1 场景 `gltf` | 两份网格来源（模型 GLB 与场景 GLB + entity_map），同步责任不清，会出现「动力学 body 无渲染实体」的漂移 | 规定单向派生：场景包由 SRM 在构建期生成（或 SRM 只引用场景节点路径）；选一种写进 §3/§6 |

### P2：工程与排期（可在实施中调整）

| # | 位置 | 问题 | 建议 |
|---|---|---|---|
| O12 | 01 §7 vs 07 §7 | 「进程内调用 p99 ≤ 20 µs」与「100 刚体单步 ≤ 300 µs」互相挤兑：一个宏步若有 ~14 次插件调用（6 传感器+3 控制器+4 力模型+动力学），开销就逼近 300 µs | 指标分级：区分「单次空调用开销」与「单宏步总开销预算」；M0 就建 `tests/bench` 骨架记录基线，否则 R9 无早期信号 |
| O13 | 07 §2/§3 vs 现状 | M0 4 周是按 5 人团队估的；当前单人 + AI 协作，照抄排期必然滑期 | 二选一：M0 砍范围（建议 Qt 空壳挪到 M1，M0 聚焦契约+内核+双传输+CI），或按 8–10 周重排 M0 |
| O14 | 07 §3 M0「Qt 空壳」 | 用 QOpenGLWidget 占位与 A06「渲染器为插件」方向不一致，M1 要重写 | M0 空壳就按「Qt 窗口 + 原生句柄 + `renderer.null` 插件」设计，与最终形态同构 |
| O15 | 01 §8 / 03 §5.3 / 01 §6.5 | ADR 只有索引无实体；`tools/codegen` 与 `srsp-cli schema-hash` 两个 schema 工具入口；Tracy 与 OpenTelemetry 并列 | ADR 落到 `docs/adr/`；registry 生成统一用 `srsp-cli schema-hash`；M0–M1 只接 Tracy，OTel 移到 1.x |

## 4. 风险与待确认

| 项 | 说明 |
|---|---|
| O1/O2/O3 涉及 `api/` 与 03 修改 | 属于契约文本修正，改动小但必须在冻结前做 |
| O5 涉及多处数字 | 需要各子系统 owner 认可容差表，否则回归阈值形同虚设 |
| O13 排期基线 | 与用户确认 M0 是砍范围还是延期 |
| 采纳方式 | 建议逐条评审；采纳的项直接改对应文档 + `api/`，并在本计划「执行记录」登记 |

## 5. 执行记录

| 日期 | 动作 | 结果 |
|---|---|---|
| 2026-10-22 | 重读 docs/01–07、README、api/ 现状，完成评审 | 总结 5 条主线优点；提出 P0×5、P1×6、P2×4 优化建议 |
| 2026-10-22 | 随多语言/工具链方案实施（用户指令"按照规划进行执行"） | O3 已落地（`srsp_host_v1` 追加 `time_tai_ns`，布局断言同步）；O5 方向部分体现（MSVC 预设注明 FP 策略待定）；其余 O1/O2/O4、O6–O15 待逐项评审 |

## 6. 修订记录

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-10-22 | V1 | 初稿 |
