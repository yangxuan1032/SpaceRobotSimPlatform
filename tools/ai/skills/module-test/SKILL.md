# Skill：模块测试（module-test）

> 职责：为模块补单元测试与集成测试，并接入 ctest。
> 目录规则：单元测试放 `<model>/ModuleTest/unit/`，集成测试放 `<model>/ModuleTest/integration/`。

## 1. 触发

- 新模块完成后；
- 修改公共接口或错误路径后；
- 修复缺陷（先补失败用例，再修）。

## 2. 目录结构（强制）

```text
<model>/ModuleTest/
├─ unit/
│  └─ <module>/
│     ├─ test_<feature>.cpp
│     └─ CMakeLists.txt
└─ integration/
   └─ <scenario>/
      ├─ test_<flow>.cpp
      └─ CMakeLists.txt
```

说明：

- 位于模型工作区（不是 `Temp/`，会随仓库提交作为验证证据）；
- 与工程级测试 `tests/`（契约/ABI/schema/双传输）职责不同，**不要混放**。

## 3. 用例要求

| 类型 | 范围 | 最低要求 |
|---|---|---|
| 单元 | 单函数/类，依赖打桩 | 每个公共函数 ≥1 正例 + 1 反例；覆盖 `SRSP_E_*` 错误路径 |
| 集成 | 跨模块（内核+插件、CLI+内核） | 1 条主流程 + 1 条失败路径 + 确定性检查 |

通用规则：

1. 固定随机种子，禁止依赖墙钟与网络；
2. 单用例超时 ≤ 30 s；
3. 断言消息含上下文（输入、期望、实际）；
4. 必须 `add_test` 注册到 ctest；
5. 测试代码同样遵守「注释英文」。

## 4. CMake 接入示例

```cmake
# <model>/ModuleTest/unit/plugin_host/CMakeLists.txt
add_executable(test_plugin_host test_plugin_host.cpp)
target_link_libraries(test_plugin_host PRIVATE srsp_core Catch2::Catch2WithMain)
add_test(NAME unit.plugin_host COMMAND test_plugin_host)
set_tests_properties(unit.plugin_host PROPERTIES TIMEOUT 30)
```

父级接入：

```cmake
# <model>/ModuleTest/CMakeLists.txt 由工程根按需 add_subdirectory
add_subdirectory(unit)
add_subdirectory(integration)
```

## 5. 验收命令

```powershell
cmake --build --preset win-ucrt64-debug
ctest --preset win-ucrt64-debug --output-on-failure
# 只跑本模块
ctest --preset win-ucrt64-debug -R "unit.plugin_host"
```

要求：新用例全部通过；失败信息能定位到用例名与断言。

## 6. 覆盖率与归类

- 核心模块目标 ≥ 70%（与 `docs/07` 质量门禁一致）；
- 单元测试进 CI；集成测试默认 nightly（决策 F-3）；
- 覆盖率不达标的模块，在日志「验证」中写明原因与后续计划。

## 7. 产物

- `ModuleTest/unit|integration` 下的测试与 CMake 接入；
- ctest 结果（贴进日志「验证」字段）。
