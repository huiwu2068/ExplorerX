---
date: 2026-09-16 10:27:35
type: fix
scope:
  - Everything results
  - tab lifecycle
  - Shell context menu
tags:
  - "#EVERYTHING"
  - "#TABS"
  - "#WIN32"
still_live: true
superseded_by: N/A
related_task: TASK-002
related_adr: N/A
related_contract: N/A
---

# MEMORY: TASK-002 Everything 结果标签与交互修复

## 摘要

- 关闭主窗口可见面板的最后一个标签时，先创建默认替代标签，不再隐式退出整个窗口；显式关闭窗口仍按正常生命周期退出。
- Everything 结果改为普通标签承载的虚拟列表，不再占用右侧辅助窗格；该临时结果标签不写入窗口会话。
- 结果列表单击选择，双击或 Enter 打开；文件按默认程序打开，文件夹在新的前台标签打开；右键显示系统 Shell 菜单。
- 搜索下拉菜单新增“隔行底色”，默认开启，支持明暗主题并持久化到 XML/注册表。

## 关键实现约束

- 每个窗口只维护一个 Everything 结果标签，后续查询复用该标签，避免引入并行结果状态和额外后台资源。
- 结果列表直接归主窗口所有，并覆盖承载标签的 ShellBrowser 列表区域；切换、关闭或双面板合并时必须同步隐藏、迁移或清除结果标签身份。
- 右键菜单复用 `ShellItemContextMenu`；仅拦截 `open` 以保持与双击一致，其余 Shell 动词保持系统行为。
- 结果标签不序列化，避免重启后把其隐藏承载目录错误恢复为带 Everything 名称的普通标签。

## 验证记录

- Release x64 主程序与测试工程构建成功。
- 非剪贴板相关测试 797/797 通过；Everything/配置/Feature 定向测试 21/21 通过。
- 文档检查与 `git diff --check` 通过。
- Windows 应用控制接口未枚举 Explorer++，因此没有完成截图式鼠标交互验收；不得把上述自动化证据表述为完整 GUI 验收。

## 回滚

- 回退 TASK-002 提交即可恢复原右侧结果窗格和旧标签关闭语义。
