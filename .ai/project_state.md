---
updated: 2026-09-12 21:34:30
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

- TASK-001 正在按已确认的 D1～D6 推荐方案实施：
  [FEAT-001](../doc/04_features/FEAT-001_windows_terminal_toolbar.md)、
  [FEAT-002](../doc/04_features/FEAT-002_everything_search_pane.md)、
  [FEAT-003](../doc/04_features/FEAT-003_dual_pane.md)。
- FEAT-002 已明确搜索下拉菜单：默认当前文件夹范围、可切换全局，以及五项匹配选项。
- Terminal 工具栏已构建验证；Everything 查询构造器与双面板基础已实现，IPC/结果窗格、分隔条、会话迁移及跨面板文件操作仍待完成。
- `AGENTS.md` 已加入“快速与轻量”原则，后续设计与实现必须优先保证响应、资源占用和最小依赖。

## 当前风险提示

> 最多 3 条。只记录当前仍会影响问题定位、架构设计、模块设计或特性设计的风险；历史变化移入 `memory/`。

- 双面板当前仍缺可拖动分隔条、完整会话迁移模型和跨面板文件操作；不得将临时左右布局视为功能完成。

## 当前需避开的坑

> 最多 3 条。只记录当前仍容易误导实现、设计或排障的坑；长期经验移入 `memory/` 或 `doc/09_note/`。

- 不应为双面板重复增加 `Config::dualPane`、`Feature::DualPane` 或 `IDM_VIEW_DUAL_PANE`。

## 当前临时约束

- Everything 当前目录与正则表达式的组合必须明确拒绝，直到 IPC 协议能安全表达独立范围约束；不得静默退化为全局搜索。


## 最近一次完整验证

- 2026-09-12 21:34:30: Debug x64 应用与测试工程构建通过；Everything 查询构造器及 XML/注册表配置往返测试共 6 项通过。

## 当前不可用验证

- N/A

## 下次会话提醒

- 先读取 TASK-001 和三份 Feature；继续 Everything IPC/窗格及双面板存储/分隔条，完成前不可标记任务完成。
