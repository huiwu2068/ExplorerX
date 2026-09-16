---
date: 2026-09-16 09:15:00
type: fix
scope:
  - Everything IPC
  - Explorer++ main window
  - dual pane
  - Windows Terminal
tags:
  - "#EVERYTHING"
  - "#DUAL_PANE"
  - "#WIN32"
  - "#E2E"
still_live: true
superseded_by: N/A
related_task: TASK-001
related_adr: N/A
related_contract: N/A
---

# MEMORY: TASK-001 实现与端到端验收 (2026-09-16 09:15:00)

## 摘要

- 解决了什么：完成 Windows Terminal 工具栏、Everything 搜索窗格和左右双面板，并修复真实主窗口收不到可用 Everything 结果、标签列表重叠和双面板恢复崩溃等问题。
- 实际改了什么：修正 QUERY2 字段顺序和 Everything 1.4 当前目录语法；把 IPC 发送移出 UI 线程并串行交付；补齐分页排队、快速回复竞态、Holder 到主窗口的列表通知转发；修正双面板启动/关闭最后标签生命周期、详细列表默认值和列宽继承；为 Terminal 参数增加可直接测试的安全转义入口。
- 验证结论：真实 Explorer++ 主窗口、Everything 1.4.1.969、Windows Terminal 1.24.11911.0、双面板真实文件复制/移动、Debug/Release x64 主程序构建均通过。
- 后续维护最该注意：独立 IPC 回调窗口通过不能代表 Explorer++ 主窗口链路通过；必须覆盖真实父子 HWND 的 `WM_COPYDATA`、`WM_NOTIFY`、定时器和列表更新。

## 背景与目的

- 背景：早期 9 项测试只覆盖协议、独立回调窗口和控制器，真实 Explorer++ 主窗口仍会停在“搜索中”或超时。
- 目标：建立真实主窗口端到端证据，并完成 Terminal、Everything、双面板及文件列表体验的可用闭环。
- 相关任务 / Feature：TASK-001、FEAT-001、FEAT-002、FEAT-003。

## 详细变更记录

| 文件 / 模块 | 变更 | 原因 | 影响 |
|-------------|------|------|------|
| `EverythingIpcClient` / `EverythingQueryBuilder` | 修正 QUERY2 包、目录范围语法和后台顺序发送 | Everything 1.4 对错误的 `path:` 范围表达式返回 0 条，`WM_COPYDATA` 不能阻塞 UI | 当前文件夹查询可用，主窗口保持响应 |
| `EverythingSearchController` / 主窗口 | 发送前登记请求、串行分页、快速回复状态顺序、通知转发 | 回复可早于 `Query()` 返回；列表通知先到 Holder | 真实结果能显示、分页和激活 |
| 双面板 / 标签生命周期 | 恢复后再切活动面板；关闭最后标签前创建替代标签 | 空容器被读取会崩溃 | 恢复和连续切换稳定 |
| 文件列表 | 默认详细信息并继承用户调整后的默认列宽 | 满足跨目录一致性 | 双击导航与列宽保持通过 |
| Terminal | 集中构造并测试 `wt.exe -d` 参数 | 防止特殊字符路径截断或注入 | 中文、空格、`&()^` 和尾反斜杠目录可用 |

## 验证记录

执行过的关键验证：

```powershell
msbuild .\Explorer++\Explorer++.vcxproj /m /p:Configuration=Debug /p:Platform=x64
msbuild .\Explorer++\Explorer++.vcxproj /m /p:Configuration=Release /p:Platform=x64
.\Explorer++\TestExplorer++\x64\Debug\TestExplorer++.exe --gtest_filter=-ClipboardTest.*
python scripts/check_docs.py
git diff --check
```

关键现象：

- Everything 当前文件夹查询：13 条；首行文本非空；结果激活新增标签；双面板和结果窗格组合布局无重叠。
- 文件列表：默认详细信息；文件夹双击成功；列宽跨导航保持 333 px。
- 双面板：左右独立导航；关闭合并；最后标签自动恢复；100 次切换稳定；保存比例 6300、恢复实测 6301；跨面板复制和移动成功。
- Windows Terminal：实际新建 shell 的当前目录与包含中文、空格和 `&()^` 的请求目录完全一致。
- 单元测试：排除系统剪贴板套件后的 815 项全部通过。`ClipboardTest` 会因其他进程占用系统剪贴板波动失败，与 TASK-001 改动无关。

结论：TASK-001 的生产链路与核心验收已完成。

## 影响范围

- 影响模块：主窗口消息路由、Everything IPC/控制器、标签/双面板、ShellBrowser 列表、Terminal 命令。
- 兼容性影响：新窗口存储字段保持 XML/注册表缺省兼容；双面板仍受 `Feature::DualPane` 控制。
- 数据 / 配置影响：Everything 窗格宽度以 DIP 保存；搜索范围和选项、双面板比例跨重启保存。
- 运维 / 部署影响：Everything 和 Windows Terminal 仍是外部可选依赖，不增加第三方运行库或常驻服务。
- 回滚方式：回退 TASK-001 完成提交。
- 相关 Runbook：N/A。

## 避坑与遗留注意事项

- Everything 1.4 的目录树约束使用带尾反斜杠的引用绝对路径；`path:` 是匹配路径修饰符，不是范围函数。
- `LVS_OWNERDATA` 列表是 Holder 的子窗口，`WM_NOTIFY` 默认发给 Holder；主窗口处理器需要显式转发。
- Everything 对同一回复窗口一次只可靠处理一个查询；分页请求要排队，不要并发覆盖。
- 当前目录与正则组合继续明确拒绝，不能静默扩大为全局搜索。
- TASK-001 验收完成后，`Feature::DualPane` 默认启用；用户无需启动参数即可从“View > Dual pane”切换。
