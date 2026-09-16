---
updated: 2026-09-16 09:31:01
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

- TASK-001 已完成：Windows Terminal 工具栏、Everything 搜索窗格、左右双面板、跨面板复制/移动、默认详细列表和列宽继承均已实现并完成真实主窗口验收。
- 双面板默认开放，无需启动参数；普通启动后使用“View > Dual pane”切换并持久化用户选择。
- `AGENTS.md` 的“快速与轻量”原则继续有效，后续设计与实现必须优先保证响应、资源占用和最小依赖。

## 当前风险提示

> 最多 3 条。只记录当前仍会影响问题定位、架构设计、模块设计或特性设计的风险；历史变化移入 `memory/`。

- 系统剪贴板可能被其他进程短暂占用，使未改动的 `ClipboardTest` 波动失败；TASK-001 相关及其余 815 项测试均通过。

## 当前需避开的坑

> 最多 3 条。只记录当前仍容易误导实现、设计或排障的坑；长期经验移入 `memory/` 或 `doc/09_note/`。

- 独立 Everything IPC 回调窗口通过不能代表主窗口链路通过；回归必须覆盖真实主窗口的 `WM_COPYDATA`、Holder `WM_NOTIFY` 转发、定时器和虚拟列表更新。
- 不应为双面板重复增加 `Config::dualPane`、`Feature::DualPane` 或 `IDM_VIEW_DUAL_PANE`。

## 当前临时约束

- Everything 当前目录与正则表达式的组合必须明确拒绝，直到 IPC 协议能安全表达独立范围约束；不得静默退化为全局搜索。


## 最近一次完整验证

- 2026-09-16 09:15:00：Debug/Release x64 主程序构建通过；815 项非剪贴板测试全部通过；Everything 1.4.1.969 真实主窗口返回 13 条且可导航；文件双击/列宽、双面板 100 次切换与恢复、跨面板真实复制/移动，以及 Windows Terminal 1.24.11911.0 特殊字符目录均通过。

## 当前不可用验证

- Windows 应用控制接口未枚举出 Explorer++，无法生成截图式验收证据；已用直接驱动真实 `Explorer++.exe` 主窗口和读取真实进程状态的端到端程序替代。
- 解决方案级 Release 的安装器项目需要本机未安装的 WiX Toolset 3.11；Release x64 Explorer++ 主程序本身构建成功。

## 下次会话提醒

- TASK-001 已完成；后续若改动 Everything 或双面板，先读 `memory/MEM-001_task001_terminal_everything_dual_pane.md`。
