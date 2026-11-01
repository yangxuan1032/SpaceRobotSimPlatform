# 代码与文件编写规范

- 状态：v0.1（2026-11-01）
- 适用范围：`src/`、`apps/`、`examples/`、`bench/`、`tests/`、`tools/`、`api/`、`docs/`

## 1. 文件组织

| 目录 | 放什么 | 不放什么 |
|---|---|---|
| `src/<module>/include/srsp/<module>/` | 公共头（`#pragma once` 或 guard 二选一，本项目用 guard） | 实现细节 |
| `src/<module>/src/` | `.cpp` 实现 | 对外可见类型 |
| `apps/` | 可执行程序（studio、cli、sidecar） | 可复用逻辑（放 `src/`） |
| `tests/` | 契约与 ABI 测试 | 业务逻辑 |
| `bench/` | 基准程序 | 断言 |
| `api/` | 契约真源（schema、proto、C ABI） | 派生代码 |
| `docs/` | 设计文档（中文） | 代码注释（放代码里） |

新增模块时同时提供：头文件、实现、`CMakeLists.txt`、至少一个测试、Doxygen 注释。

## 2. 命名

| 元素 | 规则 | 例 |
|---|---|---|
| 类型 | PascalCase | `RigidBodyDynamics`、`BlobStore` |
| 函数/变量 | snake_case | `mass_matrix()`、`step_index` |
| 成员 | 尾部下划线 | `vertex_buffer_` |
| 常量 | `k` 前缀 | `kMinViewport` |
| 枚举值 | `k` 前缀 | `TrustLevel::kIsolated` |
| 宏 | `SRSP_` 前缀，全大写 | `SRSP_HAVE_SOLVER` |
| 文件名 | snake_case | `rigid_body.cpp` |

## 3. C++ 约定

1. 标准：**C++20**；不用异常跨越 ABI 边界，不用 RTTI 做插件识别；
2. 头文件保护：`SRSP_<DIR>_<NAME>_HPP`；
3. 只用 RAII 管理资源；禁止裸 `new/delete`（第三方接口除外并注释说明）；
4. 公共 API 用 `[[nodiscard]]`；不修改的成员函数标 `const`；
5. 确定性路径内禁止：`std::random_device`、墙钟、无序遍历容器、未固定 seed；
6. 数值：SI 单位、`double`、四元数 `[x,y,z,w]`；
7. 错误：返回 `srsp_status` / `bool` + 出参，不用异常做常规控制流。

## 4. 注释

1. **所有注释与提交信息用英文**（文档用中文）；
2. 公共类型、函数、参数、返回值必须有 Doxygen：
   ```cpp
   /**
    * @brief Integrates one step with classical RK4.
    * @param q Configuration (in/out).
    * @param v Velocity (in/out).
    * @param tau Joint torques.
    * @param dt Step size, s.
    */
   ```
3. 注释解释**为什么**，不复述代码；不写「increment i」这类；
4. 临时绕行必须写 `TODO(name): reason` 或 `HACK:` 并在 ISSUES 登记。

## 5. 格式

- `clang-format`（`.clang-format`）统一；提交前运行；
- **只对 C/C++ 扩展名运行**；CMake/YAML/JSON 用各自格式化器（见 P36 教训）；
- 行宽 100；缩进 2 空格；指针 `T* p`。

## 6. 测试

| 类型 | 位置 | 要求 |
|---|---|---|
| 单元 | `<model>/ModuleTest/unit/<module>/` | 每个公共函数至少一个用例 |
| 集成 | `<model>/ModuleTest/integration/` | 跨模块链路 |
| ABI | `tests/abi/` | `struct_size`、错误码、布局 |

- 测试名用英文句子：`TEST_CASE("mass matrix is symmetric", "[dynamics]")`；
- 数值断言给容差与理由；
- 失败路径与成功路径同等重要（见 D4 的附件缺失/超限用例）。

## 7. 文档

1. 每份文档头部写状态与关联文档；
2. 决策记录（ADR）追加到 `docs/01` 表格，不新建碎片文档；
3. 支持等级必须与 CI 证据一致（`docs/01` §7.1）。

## 8. 提交

1. 一次任务一个提交；多个不相关改动分开；
2. 提交信息：`type(scope): summary` + 正文说明**为什么** + `Verification:` 行；
3. 提交前必须：构建通过 + 相关测试通过 + `clang-format`；
4. 提交与推送需用户同意；不得伪造验证结果。

## 9. 禁止事项速查

1. 中文注释、中文提交信息；
2. 在 `docs/` 放代码、在 `src/` 放长文档；
3. 把缓存/构建产物写进仓库根（统一 `<model>/Temp/`）；
4. 用异常跨插件边界；
5. 未验证就写「已通过」。
