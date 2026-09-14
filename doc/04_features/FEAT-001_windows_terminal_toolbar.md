---
status: draft
owner: lzh
created: 2026-09-12 20:28:05
updated: 2026-09-12 20:28:05
last_verified: 2026-09-12 20:28:05
verified_commit: working-tree
---

# FEAT-001：从主工具栏在当前目录打开 Windows Terminal

## 1. 文档说明

### 1.1 目的与范围

定义一个可实施的 Windows Terminal 工具栏功能：用户点击主工具栏按钮后，在当前活动文件面板所显示的文件系统目录中启动 Windows Terminal。本文覆盖按钮、命令路由、路径解析、进程启动、双面板联动、持久化兼容、异常提示和测试，不包含代码实现。

### 1.2 目标读者

Explorer++ 维护者、实现者、测试者和发布负责人。

### 1.3 变更记录

| 日期 | 变更 |
|---|---|
| 2026-09-12 | 初始设计 |

## 2. 功能概览

### 2.1 问题

现有 `OpenCommandPrompt` 命令可以在当前文件夹打开命令提示符，但用户无法用一个独立、可发现的工具栏命令直接打开 Windows Terminal。

### 2.2 目标

- 主工具栏提供独立的 Windows Terminal 按钮。
- 从活动面板解析真实文件系统路径，并把它作为 Terminal 的启动目录。
- 与现有命令提示符按钮并存，不改变现有行为。
- 单面板和双面板下使用相同的“活动面板”语义。
- 路径含空格、中文和命令行元字符时仍安全、正确。

### 2.3 非目标

- 不内嵌终端窗口。
- 不自动安装 Windows Terminal。
- 不在虚拟 Shell 文件夹、搜索结果虚拟目录或压缩包内部启动。
- 不提供管理员 Terminal 按钮；后续可另行设计。
- Windows Terminal 缺失时不回退到命令提示符。

### 2.4 用户流程

1. 用户激活一个文件面板。
2. 用户点击主工具栏的 Windows Terminal 按钮。
3. Explorer++ 获取活动面板当前目录的文件系统路径。
4. Explorer++ 启动 `wt.exe -d <目录>`。
5. Windows Terminal 在该目录中打开；Explorer++ 保持原状态。

## 3. 详细设计

### 3.1 工具栏与命令标识

- 在 `MainToolbarButton` 的末尾追加 `WindowsTerminal = 45026`。已有枚举值是持久化格式的一部分，不得插入、重排或复用。
- 默认工具栏把该按钮放在现有 `OpenCommandPrompt` 附近。
- 按钮参与现有的工具栏自定义和 XML/注册表持久化。
- 存储字符串使用稳定名称 `Open Windows Terminal`；读取未知名称时继续沿用现有兼容策略。
- 菜单命令可命名为 `IDM_FILE_OPENWINDOWSTERMINAL`，具体数值在实现时从未占用资源 ID 中分配，不在设计阶段硬编码。
- 图标遵循现有浅色/深色和 DPI 资源策略；专用图标不可与命令提示符图标完全相同。

### 3.2 启用条件

按钮仅在以下条件全部满足时启用：

- 存在活动 `BrowserPane` 和活动 `ShellBrowser`。
- 当前目录具有 `SFGAO_FILESYSTEM`。
- 当前目录不具有 `SFGAO_STREAM`，即不是压缩包等流式命名空间。
- 可以定位 `wt.exe`。

`wt.exe` 的存在性检查应缓存，避免每次 UI 状态刷新都访问磁盘；启动失败后清除缓存并重新探测。首选通过 `SearchPathW` 查找 Windows App Execution Alias，不在代码中绑定用户目录下的 WindowsApps 路径。

### 3.3 路径解析与启动参数

- 复用现有 `CanStartCommandPrompt()` 的 Shell 属性判断，并将公共部分重构为语义更通用的“当前目录可作为外部终端工作目录”检查。
- 使用 `SHGetNameFromIDList(..., SIGDN_FILESYSPATH)` 获取路径。
- 启动参数的逻辑结构为：`wt.exe`, `-d`, `<current-directory>`。
- 使用专门的 Windows 命令行参数转义函数构造参数，不允许直接拼接未转义路径。
- 启动沿用项目已有的进程启动抽象，使错误码、父窗口和测试替身保持一致。
- 不使用 shell 字符串 `cd`，因此 `&`、`(`、`)`、`^` 等路径字符不应被解释为命令。

Windows Terminal 官方支持使用 `-d`/`--startingDirectory` 指定启动目录，参见 [Windows Terminal 命令行参数](https://learn.microsoft.com/en-us/windows/terminal/command-line-arguments)。

### 3.4 双面板联动

- 命令执行时读取活动面板，而不是工具栏最后刷新时缓存的面板。
- 用户在左面板工作时打开左侧目录；在右面板工作时打开右侧目录。
- 若命令触发期间活动面板被销毁，命令安全失败，不回退到另一面板。

### 3.5 错误处理

| 情况 | 行为 |
|---|---|
| 当前目录不是文件系统目录 | 按钮禁用；快捷命令被调用时不启动并给出状态提示 |
| Windows Terminal 未安装或执行别名关闭 | 按钮禁用；菜单/快捷命令显示“找不到 Windows Terminal” |
| 进程启动失败 | 显示含 Win32 错误信息的非模态错误提示 |
| 路径在检查后失效 | 尝试启动并报告系统错误，不静默改到其他目录 |

错误提示不得建议用户执行不可信的下载命令；如需帮助链接，应指向 Microsoft 官方安装说明。

### 3.6 代码影响点

| 位置 | 设计变更 |
|---|---|
| `MainToolbarButtons.h` | 末尾追加稳定按钮枚举 |
| `DefaultToolbarButtons.h` | 把按钮加入默认布局 |
| `MainToolbar.cpp` | 文本、图标、启用状态和点击路由 |
| `MainToolbarStorage.cpp` | 稳定名称双向映射 |
| `MainWndSwitch.cpp` | 命令分发 |
| `BrowserCommandController.*` | 能力判断、路径解析与启动 |
| `resource.h` / `.rc` | 命令资源、字符串和图标资源 |

### 3.7 可测试性

将从活动面板选择目录、Shell 属性判断、参数构造、可执行文件定位和进程调用彼此分离。单元测试使用假 `ProcessLauncher` 和假可执行文件定位器，不实际打开终端。

## 4. 兼容性与发布

- 新枚举只能追加，确保已有工具栏配置仍可读取。
- 老配置没有该按钮时仍使用其保存布局；“恢复默认工具栏”或新安装才自动出现按钮，避免篡改用户自定义工具栏。
- 功能支持 Windows 10/11；最终最低系统要求以 Explorer++ 当前平台策略为准。
- 不增加第三方运行库和网络权限。
- 功能无需单独 Feature Flag；若发布风险需要隔离，可在实现阶段加入临时实验开关，稳定后移除。

## 5. 验收标准

- 在普通本地目录点击按钮，Terminal 的首个窗口/标签位于该目录。
- 路径包含空格、中文及 `&()^` 字符时，启动目录完整且不执行额外命令。
- 在“此电脑”、回收站和压缩包内部按钮禁用。
- Windows Terminal 不可用时不会打开命令提示符，并给出明确提示。
- 双面板模式下始终使用触发命令时的活动面板目录。
- 现有命令提示符按钮及其持久化行为无回归。
- 工具栏自定义保存、重启和恢复默认均正确处理新按钮。

## 6. 非功能要求

- 工具栏启用状态刷新不得产生可感知的 I/O 卡顿。
- 不记录完整用户路径到常规日志；诊断日志仅记录命令类型、成功/失败和系统错误码。
- 新增逻辑通过现有格式化、静态检查、单元测试和文档检查。

## 7. 已知限制与后续扩展

- Windows Terminal 的配置文件/profile 选择不在首版范围内。
- 管理员启动、WSL profile、PowerShell profile 快捷入口可作为后续独立功能。
- 若未来 Terminal 改变执行别名机制，需要更新定位策略。

## 8. 相关文档

- [FEAT-003：双面板](FEAT-003_dual_pane.md)
- [Windows Terminal 命令行参数](https://learn.microsoft.com/en-us/windows/terminal/command-line-arguments)

