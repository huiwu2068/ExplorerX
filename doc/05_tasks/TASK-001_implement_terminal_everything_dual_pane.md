---
status: in_progress
created: 2026-09-12 20:59:40
updated: 2026-09-12 21:34:30
owner: lzh
complexity: XL
risk_level: high
feature: FEAT-001_windows_terminal_toolbar.md, FEAT-002_everything_search_pane.md, FEAT-003_dual_pane.md
module: N/A
blocked_by: N/A
checkpoint:
  current_step: 2
  last_stable_output: Debug x64 应用与测试工程构建成功；Everything 查询构造器和配置持久化测试通过
  blocked_at: N/A
  next_action: 实现 Everything IPC 客户端、固定工具栏控件和结果窗格；继续完善双面板的存储和分隔条
---

# TASK-001：实现 Terminal、Everything 搜索与双面板

## 目标

按 FEAT-001、FEAT-002、FEAT-003 分阶段实现 Windows Terminal 工具栏、Everything 搜索窗格和左右双面板，并保证活动面板语义、旧配置兼容及现有单面板功能无回归。

## 背景

- 当前阶段：三项功能已完成详细设计，代码实现尚未开始。
- 用户需求：从主工具栏在当前目录打开 Windows Terminal；在主工具栏固定集成 Everything，并在右侧持久显示结果；提供类似 Double Commander 的左右双面板。
- 当前代码基础：已有 Command Prompt 启动逻辑；双面板已有 `Config::dualPane`、`Feature::DualPane`、`IDM_VIEW_DUAL_PANE` 和 `BrowserPane` 骨架，但尚未创建第二面板及其布局、事件和存储。
- 任务类型：跨 UI、Shell 导航、外部 IPC、窗口布局和会话存储的高风险新功能。
- 本任务复杂度为 XL。实施时按阶段小步提交和验证；未经用户确认不得扩展到 Feature 非目标。

## 开工前六项确认

以下六项是产品决策，不应由新会话自行猜测。用户可直接修改“最终选择”列，或回复“全部按推荐方案”。未确认前只允许继续分析和拆分，不开始功能代码实现。

| 编号 | 决策项 | 推荐方案 | 最终选择 | 状态 |
|---|---|---|---|---|
| D1 | 实现顺序 | ① Terminal；② Everything；③ 双面板基础；④ 跨面板复制/移动增强 | 按推荐方案 | 已确认 |
| D2 | Everything 未运行时的处理 | 支持 Everything 1.4 及以上；只提示启动后重试，不自动启动、安装或下载 | 按推荐方案 | 已确认 |
| D3 | Everything 设置持久化 | 首次默认当前文件夹；之后保存用户选择；五个匹配选项也跨重启保存 | 按推荐方案 | 已确认 |
| D4 | Everything 搜索触发 | 仅按 Enter 或点击搜索按钮触发；首版不做逐键实时搜索 | 按推荐方案 | 已确认 |
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
- 自动安装或管理 Everything 数据库、逐键实时搜索、预览、搜索历史和多结果窗格。
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
- [x] 阶段 1 验证：Debug x64 构建、工具栏存储单元测试；实际 Terminal 手工测试待最终验收。
- [ ] 阶段 2A：已实现 Everything 设置模型和查询构造器；待实现异步 IPC 客户端和协议解析测试。
- [ ] 阶段 2B：实现固定工具栏控件、下拉菜单、右侧虚拟结果窗格、分页和存储。
- [ ] 阶段 2 验证：范围/五选项组合、迟到回复、Everything 缺失/超时、DPI 和手工导航测试。
- [ ] 阶段 3A：完成 `BrowserPaneId`、双面板所有权、活动面板事件和布局基础设施。
- [ ] 阶段 3B：完成标签迁移、会话存储兼容、Feature Flag 和共享命令联动。
- [ ] 阶段 3 验证：旧配置迁移、双面板反复切换、左右独立导航和组合布局测试。
- [ ] 阶段 4：实现并验证“复制/移动到另一面板”，不改变 F5/F6。
- [ ] 收尾：执行 Debug/Release 构建、完整相关测试、文档检查和手工验收；更新状态和复盘。
- [ ] 整个 TASK 达到完成标准后，按项目规则创建 Git commit，并运行 `python tools/backup_project.py <备份名称>`。

## 完成标准 (Definition of Done)

- [ ] D1～D6 均有明确最终选择，且对应 Feature 已同步。
- [ ] FEAT-001、FEAT-002、FEAT-003 的所有首版验收标准逐项通过或有用户批准的范围变更。
- [ ] Debug x64 和 Release x64 均构建成功。
- [ ] 新增核心逻辑具有单元测试；IPC、存储迁移、活动面板和标签迁移具有异常/边界覆盖。
- [ ] 在实际 Windows Terminal 和 Everything 环境完成手工验证，并记录版本及观察结果。
- [ ] 单面板、Command Prompt、旧 Search、工具栏配置、XML/注册表会话恢复无回归。
- [ ] 未引入无关重构、无关依赖或未经确认的功能行为。
- [ ] Feature、Task、必要的 ADR/LLD/Memory 和 `.ai/project_state.md` 已同步。
- [ ] 完整任务完成后已经 Git commit，并运行项目备份脚本完成双备份。

## 代码审查状态

### LLM 自审

- 尚未开始代码实现。

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
- LLD: N/A（双面板和 Everything IPC 开工前应补充或从本任务拆分）
- ADR: N/A（若最低 Everything 版本或存储迁移方案发生重大取舍再新增）
- API Contract: N/A
- Runbook: N/A
- Memory: N/A

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

- Windows Terminal 工具栏按钮、命令路由、`wt.exe -d` 启动及工具栏存储往返测试已实现；Debug x64 构建通过。
- 双面板已有左右 `BrowserPaneId`、第二标签容器、活动面板焦点切换、初始目录复制与配置持久化；仍缺分隔条、完整会话存储及跨面板文件操作。
- Everything 已实现设置/查询构造器及边界单元测试；IPC、固定工具栏控件和结果窗格尚未开始。

当前阻塞：

- 无外部阻塞；按已确认的推荐方案继续实现。

关键决策：

| 日期 | 决策 | 原因 | 影响 |
|---|---|---|---|
| 2026-09-12 | 以本 TASK 作为三项功能实施的唯一总入口 | 保证跨会话恢复时不遗漏六项确认和 Feature 要求 | 所有阶段开始前先读本 TASK 及对应 Feature |
| 2026-09-12 | 任务按四个可独立验证阶段推进 | 降低跨 UI、IPC、存储和对象所有权一次性变更的风险 | 阶段提交可单独回滚，总任务完成后再执行规定的最终备份 |
| 2026-09-12 | D1～D6 按推荐方案执行 | 用户已允许开始实现，且未提出与推荐方案不同的约束 | 实现按 Terminal、Everything、双面板和跨面板文件操作顺序进行 |
