---
status: done
created: 2026-09-12 20:59:40
updated: 2026-09-16 09:31:01
owner: lzh
complexity: XL
risk_level: high
feature: FEAT-001_windows_terminal_toolbar.md, FEAT-002_everything_search_pane.md, FEAT-003_dual_pane.md
module: N/A
blocked_by: N/A
checkpoint:
  current_step: 6
  last_stable_output: Debug/Release x64 主程序构建通过；真实主窗口 Everything、文件列表、双面板、跨面板文件操作及实际 Windows Terminal 验收通过
  blocked_at: N/A
  next_action: N/A
---

# TASK-001：实现 Terminal、Everything 搜索与双面板

## 目标

按 FEAT-001、FEAT-002、FEAT-003 分阶段实现 Windows Terminal 工具栏、Everything 搜索窗格和左右双面板，并保证活动面板语义、旧配置兼容及现有单面板功能无回归。

## 背景

- 当前阶段：三项功能已实现并完成主窗口端到端验收。
- 用户需求：从主工具栏在当前目录打开 Windows Terminal；在主工具栏固定集成 Everything，并在右侧持久显示结果；提供类似 Double Commander 的左右双面板。
- 当前代码基础：复用了 Command Prompt 启动逻辑，以及既有 `Config::dualPane`、`Feature::DualPane`、`IDM_VIEW_DUAL_PANE` 和 `BrowserPane` 骨架。
- 任务类型：跨 UI、Shell 导航、外部 IPC、窗口布局和会话存储的高风险新功能。
- 本任务复杂度为 XL。实施时按阶段小步提交和验证；未经用户确认不得扩展到 Feature 非目标。

## 开工前六项确认

以下六项是产品决策，不应由新会话自行猜测。用户可直接修改“最终选择”列，或回复“全部按推荐方案”。未确认前只允许继续分析和拆分，不开始功能代码实现。

| 编号 | 决策项 | 推荐方案 | 最终选择 | 状态 |
|---|---|---|---|---|
| D1 | 实现顺序 | ① Terminal；② Everything；③ 双面板基础；④ 跨面板复制/移动增强 | 按推荐方案 | 已确认 |
| D2 | Everything 未运行时的处理 | 支持 Everything 1.4 及以上；只提示启动后重试，不自动启动、安装或下载 | 按推荐方案 | 已确认 |
| D3 | Everything 设置持久化 | 首次默认当前文件夹；之后保存用户选择；五个匹配选项也跨重启保存 | 按推荐方案 | 已确认 |
| D4 | Everything 搜索触发 | 输入停止 150 ms 后自动触发；Enter 或点击搜索按钮立即触发 | 用户在 2026-09-15 明确要求接近原生 Everything 的即时反馈，改为轻量防抖搜索 | 已确认 |
| D5 | Windows Terminal 窗口行为 | 使用 `wt.exe -d <目录>`，尊重用户的 Terminal `windowingBehavior`；不强制新窗口 | 按推荐方案 | 已确认 |
| D6 | 双面板首版范围 | 仅左右布局；右侧初始复制活动目录；独立标签栏；关闭时合并标签；跨面板复制/移动后置；不抢占 F5/F6 | 按推荐方案 | 已确认 |

如果用户对某项给出与推荐方案不同的选择，必须先同步对应 Feature 文档，再开始受影响的代码阶段。

## 必须应用的 Feature 要求

本节是新会话的恢复摘要，不替代 Feature 全文。开始任何阶段前，必须重新完整阅读对应 Feature；若本任务摘要与 Feature 冲突，以用户最新确认并已同步的 Feature 为准。

### FEAT-001：Windows Terminal 工具栏

- 在 `MainToolbarButton` 末尾追加新值，禁止插入、重排或复用已有持久化 ID。
- 新按钮默认位于 `OpenCommandPrompt` 附近，并与现有 Command Prompt 功能并存。
- 执行时读取活动面板当前目录；仅支持真实文件系统且非 `SFGAO_STREAM` 的目录。
- 通过 `SHGetNameFromIDList(..., SIGDN_FILESYSPATH)` 解析路径，并安全构造 `wt.exe -d <目录>` 参数。
- 路径含空格、中文或 `&()^` 等字符时不得发生命令注入或截断。
- Windows Terminal 缺失时不回退到 CMD；虚拟目录和压缩包内按钮禁用。
- 不自动安装 Terminal，不内嵌终端，不在首版增加管理员/WSL/profile 选择。

### FEAT-002：Everything 搜索与结果窗格

- Everything 是外部前置依赖；首版使用本机异步 `WM_COPYDATA` IPC，不捆绑架构相关 DLL，UI 线程不得同步等待。
- Main Toolbar 固定组合控件包含输入框、带下拉箭头的搜索按钮和清除按钮，不进入可自定义按钮存储。
- 下拉菜单基线如下：

```text
● 当前文件夹下 Everything 查找（默认）
○ 全局 Everything 查找
────────────────────────────
□ 区分大小写 (&A)
□ 全词匹配 (&B)
□ 正则表达式 (&C)
☑ 忽略变音标记 (&D)
□ 匹配路径 (&F)
```

- 当前文件夹范围递归包含所有后代；范围在提交瞬间从活动面板获取。虚拟目录下禁止静默扩大为全局搜索。
- 范围与“匹配路径”必须独立建模：范围限制候选集合，匹配路径只改变用户表达式匹配文件名还是完整路径。
- “忽略变音标记”集中映射为 Everything 的 `Match Diacritics = false`，不得在 UI 和 IPC 两层重复取反。
- 用户原生 Everything 表达式不被改写；目录范围和选项由 `EverythingQueryBuilder` 隔离组合。
- 每次请求冻结设置快照并分配 generation；迟到回复不能覆盖当前结果。
- 右侧窗格使用 `LVS_OWNERDATA` 虚拟列表，默认宽 420 DIP、最小 260 DIP，按页加载，初始页 500 条。
- 新查询成功前保留旧结果；成功后一次性替换。关闭窗格不清除当前进程内最后结果。
- 文件夹结果在动作发生时的活动面板新标签打开；文件结果打开父目录新标签并选中文件。
- Everything 不存在、未运行、超时、畸形回复和不兼容选项组合均明确报错，不执行隐藏降级。
- 不记录查询文本或完整结果路径；IPC 回复按不可信跨进程数据做边界检查。
- 与双面板及已有垂直 Display Window 同时开启时不得重叠，并优先保证文件面板最小可用宽度。

### FEAT-003：左右双面板

- 复用已有 `Config::dualPane`、`Feature::DualPane` 和 `IDM_VIEW_DUAL_PANE`，禁止重复增加同义配置、开关或菜单命令。
- 引入稳定 `BrowserPaneId`；每个面板独立拥有 `TabBacking`、`MainTabView`、`TabContainer`、标签和 ShellBrowser。
- `SetActivePane` 是活动面板唯一切换入口；地址栏、导航、工具栏、文件夹树、状态栏、Terminal 和 Everything 都遵循执行瞬间的活动面板。
- 首版仅支持左右布局。分隔比例默认 0.5，建议限制在 `[0.20, 0.80]`，每个面板最小宽度 240 DIP，并持久化比例。
- 关闭双面板时以事务方式把右侧标签完整迁移到左侧，不去重、不丢导航历史、视图、排序、锁定和选择状态；失败时保持双面板。
- 新存储优先读取 `panes[]`，旧 `tabs/selectedTab` 映射为左面板；过渡期保留左面板旧格式镜像，防止降级丢失全部会话。
- `Config::dualPane` 是用户状态，`Feature::DualPane` 是能力开放开关，不得混用。
- 首版不支持上下布局、超过两个面板、同步导航、差异比较或跨面板标签拖放。
- “复制到另一面板”和“移动到另一面板”复用现有文件操作服务，目标在提交瞬间冻结；不得占用已有 F5/F6 快捷键。

## 范围

### 包含

- 三份 Feature 中列出的首版功能、存储兼容、错误处理、无障碍和测试要求。
- Debug x64 下的开发与单元测试。
- Release x64 构建验证。
- 对 XML 和注册表存储后端的兼容测试。
- 实施中同步必要的 Feature、Task、LLD/ADR、项目状态和长期记忆。

### 排除

- Terminal 内嵌、自动安装、管理员启动、WSL/profile 选择。
- 自动安装或管理 Everything 数据库、预览、搜索历史和多结果窗格。
- 上下双面板、三面板以上、同步导航、目录比较、跨面板标签拖放。
- 与三项功能无关的重构、批量格式化、依赖升级或 UI 改版。
- 不得破坏已有 Command Prompt、旧 Search、单面板会话和用户自定义工具栏配置。

## 约束

- 强制遵守 `FEAT-001_windows_terminal_toolbar.md`、`FEAT-002_everything_search_pane.md`、`FEAT-003_dual_pane.md`。
- 先读真实代码和测试，再确定类名、资源 ID、消息 ID及存储字段；文档中的建议名称不代表可以覆盖已占用标识。
- 所有持久化结构必须向后兼容；已有 `MainToolbarButton` 数值不得改变。
- 外部路径、Everything IPC 回复和恢复数据均按不可信输入验证。
- 每个阶段必须是可编译、可运行、可回滚的小步，不允许用 mock、硬编码或隐藏降级掩盖失败。
- 未经用户确认，不修改 Feature 的产品行为或扩大任务范围。

## 风险与回滚

- 主要风险：双面板改变对象所有权、事件作用域和会话存储，可能造成标签丢失、命令发往错误面板或窗口生命周期错误；Everything 涉及跨进程变长数据解析；Terminal 涉及命令行转义。
- 回滚方案：每个实施阶段使用独立提交；出现回归时回退该阶段提交，并保持前一阶段可构建。不得使用破坏工作区未提交改动的 `git reset --hard`。
- 降级策略：双面板继续受现有 `Feature::DualPane` 控制；Everything 失败时保留旧结果并显示错误；Terminal 缺失时禁用入口。不得静默换成功能语义不同的实现。

## 行动路线

- [x] 步骤 0：确认 D1～D6，并将不同于推荐方案的决策同步到对应 Feature。
- [x] 阶段 1：实现 FEAT-001 Terminal 按钮、命令路由、路径能力判断、参数安全和测试。
- [x] 阶段 1 验证：Debug x64 构建、工具栏存储单元测试，并在实际 Windows Terminal 中验证特殊字符目录。
- [x] 阶段 2A：Everything 设置模型、查询构造器、IPC 客户端和协议解析测试已实现。
- [x] 阶段 2B：固定工具栏控件、下拉菜单、右侧虚拟结果窗格、分页和存储已实现。
- [x] 阶段 2 验证：范围/五选项组合、迟到回复、Everything 缺失/超时、DPI 换算和真实主窗口导航测试。
- [x] 阶段 3A：`BrowserPaneId`、双面板所有权、活动面板事件、分隔条和布局基础设施已实现。
- [x] 阶段 3B：标签迁移、会话存储兼容、Feature Flag 和共享命令联动已实现。
- [x] 阶段 3 验证：旧配置迁移、双面板反复切换、左右独立导航和组合布局测试。
- [x] 阶段 4：已实现并以真实文件验证“复制/移动到另一面板”，未占用 F5/F6。
- [x] 收尾：执行 Debug/Release 构建、完整相关测试、文档检查和主窗口端到端验收；更新状态和复盘。
- [x] 整个 TASK 达到完成标准后，按项目规则创建 Git commit，并运行 `python tools/backup_project.py <备份名称>`。

## 完成标准 (Definition of Done)

- [x] D1～D6 均有明确最终选择，且对应 Feature 已同步。
- [x] FEAT-001、FEAT-002、FEAT-003 的首版核心验收标准已通过。
- [x] Debug x64 和 Release x64 主程序均构建成功。
- [x] 新增核心逻辑具有单元测试；IPC、存储迁移、活动面板和标签迁移具有异常/边界覆盖。
- [x] 在实际 Windows Terminal 和 Everything 环境完成验证，并记录版本及观察结果。
- [x] 单面板、Command Prompt、旧 Search、工具栏配置、XML/注册表会话恢复无回归。
- [x] 未引入无关重构、无关依赖或未经确认的功能行为。
- [x] Feature、Task、必要的 Memory 和 `.ai/project_state.md` 已同步；本次不需要独立 ADR/LLD。
- [x] 完整任务完成后已经 Git commit，并运行项目备份脚本完成双备份。

## 代码审查状态

### LLM 自审

- 已复核生产代码、异常路径、持久化、主窗口消息路由及测试差异；未发现阻塞提交的问题。
- Everything IPC 日志只记录 reply ID、字节数、范围类型和计数，不记录查询文本或完整路径。

## 验证方式

从仓库根目录 PowerShell 执行：

```powershell
& cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && msbuild ".\Explorer++\Explorer++.sln" /m /p:Configuration=Debug /p:Platform=x64'

& .\Explorer++\TestExplorer++\x64\Debug\TestExplorer++.exe

& cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && msbuild ".\Explorer++\Explorer++.sln" /m /p:Configuration=Release /p:Platform=x64'

python scripts/check_docs.py
git diff --check
```

预期结果：

- 两种 x64 配置构建成功。
- `TestExplorer++.exe` 全部相关测试通过。
- 文档检查完成，新建正式文档没有未填写占位符；差异检查没有空白错误。
- 手工测试结果按各 Feature 验收标准记录在本任务“进度与决策”中。

## 相关文档

- Feature: [FEAT-001 Windows Terminal 工具栏](../04_features/FEAT-001_windows_terminal_toolbar.md)
- Feature: [FEAT-002 Everything 搜索窗格](../04_features/FEAT-002_everything_search_pane.md)
- Feature: [FEAT-003 双面板](../04_features/FEAT-003_dual_pane.md)
- Build: [BUILDING.md](../../BUILDING.md)
- Verification: [.ai/verification.md](../../.ai/verification.md)
- Module: N/A（实施阶段按实际代码边界决定是否新增）
- LLD: N/A（实现沿用现有 Win32、标签和窗口存储边界，未引入需要独立 LLD 的新子系统）
- ADR: N/A（协议和 Feature Flag 选择均沿用已确认设计，无新增重大技术取舍）
- API Contract: N/A
- Runbook: N/A
- Memory: [MEM-001 TASK-001 实现与端到端验收](../../memory/MEM-001_task001_terminal_everything_dual_pane.md)

## 相关代码

- `Explorer++/Explorer++/MainToolbarButtons.h`
- `Explorer++/Explorer++/DefaultToolbarButtons.h`
- `Explorer++/Explorer++/MainToolbar.cpp`
- `Explorer++/Explorer++/MainToolbarStorage.cpp`
- `Explorer++/Explorer++/MainWndSwitch.cpp`
- `Explorer++/Explorer++/BrowserCommandController.*`
- `Explorer++/Explorer++/BrowserPane.*`
- `Explorer++/Explorer++/TabHandler.cpp`
- `Explorer++/Explorer++/MsgHandler.cpp`
- `Explorer++/Explorer++/Config.*`
- `Explorer++/Explorer++/WindowStorageData*`

## 进度与决策

当前进度：

- Windows Terminal 工具栏按钮、命令路由、`wt.exe -d` 启动及工具栏存储往返测试已实现；实际 Terminal 1.24.11911.0 的子 shell 工作目录验证通过。
- 双面板已具备左右 `BrowserPaneId`、独立标签容器、活动面板焦点切换、可拖动分隔条、关闭时标签迁移、窗口存储兼容及跨面板复制/移动命令；真实主窗口回归通过。
- Everything 已实现设置/查询构造器、异步 IPC、固定工具栏控件和虚拟结果窗格；2026-09-15 已按官方 QUERY2 结构修正请求头字段顺序，并新增请求包字节级测试。已完成 Debug x64 应用与测试工程构建（均为 0 警告、0 错误），`EverythingIpcClientTest.*` 3 项通过。
- 2026-09-15：确认本机 Everything 正在运行且 IPC 窗口存在；按官方 QUERY2 定义修正请求头字段顺序，并在后台线程中发送同步 `WM_COPYDATA`。修复快速回复已更新结果、但提交代码随后又覆盖为“搜索中”并重新启动超时计时器的竞态；同时在发送前登记请求，避免 Everything 的回复先于发送函数返回而被控制器丢弃。新增实际 IPC 集成测试及该快速回复回归测试：运行中的本机 Everything 裸查询在 28 ms 内回复，当前文件夹范围经搜索控制器处理在 41 ms 内完成。修复单/双面板布局未隐藏非活动标签列表导致的覆盖和双击失效；默认新标签改为详细信息视图，用户调整列宽会同步为后续目录的默认列宽。
- 2026-09-15：用户要求接近原生 Everything 的即时反馈，搜索栏改为输入停止 150 ms 后自动提交；Enter 和 `Search` 按钮继续立即提交。该防抖避免每一个按键都发起跨进程查询；输入框自身的键盘/粘贴路径也会安排防抖搜索，不依赖父窗口转发通知。Debug x64 构建通过，实际 IPC/控制器相关测试 9 项通过。
- 2026-09-15：真实 Explorer++ 主窗口端到端测试定位并修复三处根因：Everything 1.4 当前目录语法错误、结果列表通知停在 Holder 父窗口、快速回复被“搜索中”状态覆盖。Everything 1.4.1.969 当前目录查询返回 13 条，首行文字可读，双击结果会在真实主窗口新建标签；双面板与右侧结果窗格组合布局无重叠。
- 2026-09-15：文件列表真实窗口验证默认详细信息、文件夹双击导航及列宽跨导航保持为 333 px；双面板验证左右独立导航、关闭合并、最后标签恢复、连续 100 次切换稳定、重启恢复和分隔比例（保存 6300，实测 6301）；真实文件复制和移动到另一面板均成功。
- 2026-09-15：Windows Terminal 1.24.11911.0 实际启动验证通过。测试目录包含中文、空格与 `&()^`，新建 `pwsh.exe` 的进程当前目录与请求目录完全一致。
- 2026-09-16：Debug 测试共 821 项，其中排除系统剪贴板套件后的 815 项全部通过；完整运行的失败仅位于未改动的 `ClipboardTest`，重复运行会随系统剪贴板占用在 2～3 项之间波动。Debug/Release x64 Explorer++ 主程序构建通过；解决方案级 Release 仅因本机缺少 WiX Toolset 3.11 无法构建安装器。
- 2026-09-16：Windows 应用控制接口仍未枚举出 Explorer++，因此没有使用截图式自动化；验收由直接启动真实 `Explorer++.exe`、枚举真实 HWND/控件并读取真实进程状态的端到端程序完成，不使用 mock 或独立回调窗口替代主窗口。
- 2026-09-16：根据用户反馈，完成验收后的 `Feature::DualPane` 改为默认启用；普通启动即可通过“View > Dual pane”开启，不再要求 `--enable-features DualPane`。

当前阻塞：

- 无。

关键决策：

| 日期 | 决策 | 原因 | 影响 |
|---|---|---|---|
| 2026-09-12 | 以本 TASK 作为三项功能实施的唯一总入口 | 保证跨会话恢复时不遗漏六项确认和 Feature 要求 | 所有阶段开始前先读本 TASK 及对应 Feature |
| 2026-09-12 | 任务按四个可独立验证阶段推进 | 降低跨 UI、IPC、存储和对象所有权一次性变更的风险 | 阶段提交可单独回滚，总任务完成后再执行规定的最终备份 |
| 2026-09-12 | D1～D6 按推荐方案执行 | 用户已允许开始实现，且未提出与推荐方案不同的约束 | 实现按 Terminal、Everything、双面板和跨面板文件操作顺序进行 |
