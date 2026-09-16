---
updated: 2026-09-16 10:27:35
owner: lzh
review_after_days: 14
max_lines: 200
---

# Project State

本文件记录跨会话仍然有效的当前项目状态。它是 AI 每次会话优先读取的“当前世界状态”，也供人快速同步上下文。

本文件必须保持短小，只保留“当前仍有效”的状态，不保存历史流水。历史变更、验证结论和踩坑记录沉淀到 `memory/`；长期复用知识沉淀到 `doc/09_note/`。

维护建议：

- 每个章节保留 0 到 3 条最重要信息。
- 失效信息应删除或迁移到 `memory/`，不要在本文件累积。
- 文件超过 `max_lines` 时应立即瘦身。
- 需要更多历史线索时，运行 `python scripts/check_docs.py --suggest-context`，从 TASK、Memory、ADR、LLD 等文档中提取候选上下文。

## 当前迭代焦点

- TASK-002 已完成：关闭最后标签不再退出；Everything 结果已迁移为结果标签，并支持双击/Enter 打开、Shell 右键菜单和可配置隔行底色。
- 双面板默认开放，无需启动参数；普通启动后使用“View > Dual pane”切换并持久化用户选择。
- `AGENTS.md` 的“快速与轻量”原则继续有效，后续设计与实现必须优先保证响应、资源占用和最小依赖。

## 当前风险提示

> 最多 3 条。只记录当前仍会影响问题定位、架构设计、模块设计或特性设计的风险；历史变化移入 `memory/`。

- 系统剪贴板可能被其他进程短暂占用，使未改动的 `ClipboardTest` 波动失败；TASK-001 相关及其余 815 项测试均通过。

## 当前需避开的坑

> 最多 3 条。只记录当前仍容易误导实现、设计或排障的坑；长期经验移入 `memory/` 或 `doc/09_note/`。

- 独立 Everything IPC 回调窗口通过不能代表主窗口链路通过；回归必须覆盖真实主窗口的 `WM_COPYDATA`、主窗口 `WM_NOTIFY`、定时器、结果标签和虚拟列表更新。
- 不应为双面板重复增加 `Config::dualPane`、`Feature::DualPane` 或 `IDM_VIEW_DUAL_PANE`。

## 当前临时约束

- Everything 当前目录与正则表达式的组合必须明确拒绝，直到 IPC 协议能安全表达独立范围约束；不得静默退化为全局搜索。


## 最近一次完整验证

- 2026-09-16 10:27:35：TASK-002 Release x64 主程序与测试工程构建通过；非剪贴板相关测试 797/797、Everything/配置定向测试 21/21 通过；文档和差异检查通过。新产物真实进程成功创建主窗口并保持响应。

## 当前不可用验证

- Windows 应用控制接口仍未枚举出 Explorer++；TASK-002 无法生成结果标签、右键菜单和隔行底色的截图式验收证据，不得把构建或 IPC 测试称为完整 GUI 验收。
- 解决方案级 Release 的安装器项目需要本机未安装的 WiX Toolset 3.11；Release x64 Explorer++ 主程序本身构建成功。

## 下次会话提醒

- 后续修改 Everything 结果交互前读取 `doc/05_tasks/TASK-002_everything_results_tab_interactions.md` 和 `memory/MEM-002_task002_everything_results_tab_interactions.md`。
