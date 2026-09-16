---
status: done
created: 2026-09-16 10:20:00
updated: 2026-09-16 10:27:35
owner: lzh
complexity: M
risk_level: medium
feature: FEAT-002_everything_search_pane.md
module: N/A
blocked_by: N/A
checkpoint:
  current_step: 6
  last_stable_output: Release x64 主程序及测试工程构建成功；非剪贴板相关测试 797/797、Everything/配置定向测试 21/21 通过
  blocked_at: N/A
  next_action: N/A
---

# TASK-002: Everything 结果标签与交互修复

## 目标

关闭最后一个标签不再退出窗口，Everything 结果改为标签页，并具备标准打开、Shell 右键菜单和可配置隔行底色。

## 背景

- 用户反馈双击关闭唯一标签会关闭整个窗口。
- Everything 结果目前占用右侧辅助窗格，不符合标签化工作流。
- 结果需要与 Everything 主程序一致：单击选择、双击/Enter 打开、右键系统菜单。
- 用户截图要求列表可开启浅色隔行底色。

## 范围

### 包含

- 主生命周期内关闭任一面板最后标签时先创建默认替代标签。
- 创建/复用 Everything 结果承载标签，并在其内容区显示虚拟结果列表。
- 文件直接打开、文件夹新标签打开、Shell 上下文菜单。
- 下拉菜单中的隔行底色开关及 XML/注册表持久化。

### 排除

- 多个并存的 Everything 结果标签。
- 结果标签跨进程恢复。
- Everything 当前目录与正则表达式组合支持。

## 风险与回滚

- 主要风险：承载标签仍含一个隐藏 ShellBrowser，切换/关闭时必须正确恢复底层列表和焦点。
- 回滚方案：回退本任务提交即可恢复右侧结果窗格及原最后标签行为。
- 降级策略：隔行底色可从搜索下拉菜单关闭。

## 行动路线

- [x] 修复最后标签关闭导致窗口退出。
- [x] 将结果列表迁移到标签内容区。
- [x] 增加直接打开、右键菜单与隔行底色。
- [x] 完成构建、测试、文档检查和可执行范围内的真实进程验证。

## 完成标准 (Definition of Done)

- [x] 相关代码和持久化已实现。
- [x] Release x64 主程序构建成功。
- [x] 非剪贴板相关测试全部通过。
- [x] 文档、项目状态和 Memory 已更新。
- [x] 变更已提交并完成项目备份。

## 验证方式

```powershell
msbuild Explorer++\Explorer++\Explorer++.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:SolutionDir=<repo>\Explorer++\
Explorer++\TestExplorer++\x64\Release\TestExplorer++.exe --gtest_filter=-ClipboardTest.*
python scripts/check_docs.py
```

## 进度与决策

| 日期 | 决策 | 原因 | 影响 |
|---|---|---|---|
| 2026-09-16 | 结果使用普通 Shell 标签承载并覆盖其内容区 | 复用现有标签生命周期，避免引入第二套异构标签架构 | 结果标签仍维护一个隐藏的轻量 ShellBrowser |
| 2026-09-16 | 左键遵循 Everything 标准：单击选择、双击打开 | 避免单击误启动文件，并与原生 Everything 一致 | Enter 与双击均直接打开文件 |

## 验证结论

- Release x64 主程序和 `TestExplorer++.exe` 均构建成功。
- 排除依赖系统剪贴板状态的测试后，797/797 通过；Everything IPC、控制器、查询构造、配置存储和特性开关定向测试 21/21 通过。
- `python scripts/check_docs.py` 与 `git diff --check` 通过。
- 新产物可创建 Explorer++ 主窗口且进程保持响应。
- 当前 Windows 应用控制接口没有枚举 Explorer++ 原生窗口，因此无法产出双击、右键菜单、结果标签及隔行底色的截图式 GUI 证据；该限制已作为未执行验证明确记录，不以构建或进程启动代替。

## 相关文档

- Feature: `../04_features/FEAT-002_everything_search_pane.md`
- Dual pane: `../04_features/FEAT-003_dual_pane.md`
- Memory: `../../memory/MEM-002_task002_everything_results_tab_interactions.md`
