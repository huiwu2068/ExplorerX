---
status: active
owner: lzh
created: 2026-09-12 20:28:05
updated: 2026-09-16 09:15:00
last_verified: 2026-09-16 09:15:00
verified_commit: working-tree
---

# FEAT-002：主工具栏 Everything 搜索与右侧结果窗格

## 1. 文档说明

### 1.1 目的与范围

定义 Explorer++ 与本机 Everything 的搜索集成：主工具栏固定显示搜索入口，结果异步呈现在可长期保留的右侧窗格中。本文覆盖工具栏输入、Everything IPC、结果模型、右侧窗格、导航行为、持久化、隐私、错误处理和验收标准，不包含 Everything 本体安装、数据库管理或代码实现。

### 1.2 决策摘要

- Everything 是外部前置依赖，Explorer++ 不捆绑、不自动安装、不自动启动它。
- 首版采用 Everything 官方 `WM_COPYDATA` IPC，而不是绑定架构相关 DLL。
- 用户输入按 Everything 原生语法解释；Explorer++ 只在发送请求时附加隔离的目录范围与匹配选项，不改写用户输入本身。
- 搜索按钮带下拉菜单；首次使用默认搜索当前文件夹及其所有子文件夹，也可以切换为全局搜索。
- 区分大小写、全词匹配、正则表达式和匹配路径默认关闭；“忽略变音标记”默认开启。
- 用户停止输入 150 ms 后自动提交查询；Enter 或点击搜索按钮时立即提交。
- 结果窗格位于整个主窗口右侧，查询成功后保留，直到用户关闭或新查询成功替换。
- 激活结果时，目标是“激活动作发生时”的活动文件面板。

### 1.3 变更记录

| 日期 | 变更 |
|---|---|
| 2026-09-12 | 初始设计 |
| 2026-09-12 | 增加当前文件夹/全局范围及匹配选项下拉菜单 |
| 2026-09-16 | 同步已确认的 150 ms 防抖自动搜索，并记录真实主窗口验收结果 |

## 2. 用户体验

### 2.1 主工具栏搜索入口

- 在 Main Toolbar 右侧加入固定的组合控件：Everything 图标、文本框、带下拉箭头的搜索按钮和清除按钮。
- 该控件不进入可自定义按钮序列，不能被拖走或删除；已有可自定义工具栏按钮继续按原格式保存。
- Win32 实现上，在工具栏末尾保留一个内部占位槽，并把 `WC_EDIT` 等子控件定位到该槽；内部占位 ID 不写入 `MainToolbarButton` 存储。
- 窗口变窄时按顺序缩短输入框；低于最小宽度时折叠为搜索图标。点击图标展开临时输入框，不遮挡工具栏按钮。
- 默认占位文字为“Everything 搜索”；获得焦点后不把占位文字作为查询。

搜索按钮主体使用当前选定范围提交查询；箭头区域打开下列菜单：

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

- 两个搜索范围是互斥单选项；菜单始终用圆点标出当前范围。
- 五个匹配选项是可独立切换的复选项。
- `(&A)` 等是菜单助记键，只在菜单打开时生效，不占用应用级 `Alt+A` 等快捷键。
- 菜单关闭后，按钮工具提示显示当前范围和已启用选项；结果窗格标题也显示“当前文件夹”或“全局”。
- “当前文件夹下”包含当前目录的直接子项和所有后代，不包含当前文件夹自身。
- 范围目录在用户提交查询时从活动面板读取。之后即使该面板导航到别处，已经发出的查询范围也不改变。
- 当前面板位于“此电脑”、回收站或压缩包等非文件系统位置时，“当前文件夹下”禁用；如果它恰好是当前选择，提交被阻止并提示用户选择“全局”，不得静默扩大搜索范围。

### 2.2 键盘行为

| 操作 | 行为 |
|---|---|
| Enter | 提交当前查询 |
| Alt+↓（搜索框内） | 打开搜索选项下拉菜单 |
| Esc（输入框非空） | 清空输入 |
| Esc（输入框为空） | 把焦点返回活动文件列表 |
| Ctrl+A | 选择全部查询文本 |
| Alt+E | 聚焦 Everything 搜索框；实现时先确认快捷键未占用 |

空白查询不发送；只显示轻量状态提示。查询前后空格是否有意义由 Everything 语法决定，因此除“全为空白”的判断外不改写输入。

### 2.3 右侧结果窗格

- 第一次有效查询自动打开结果窗格。
- 窗格有独立关闭按钮和水平拖动分隔条。
- 窗格默认宽度 420 DIP，最小 260 DIP；持久化用户最后宽度。
- 列表列为：名称、路径、大小、修改时间。文件夹的大小显示为空。
- 列表使用 `LVS_OWNERDATA` 虚拟模式，避免大量结果造成大量 HWND/对象分配。
- 标题区域显示查询摘要、结果数量或错误状态，并提供重试。
- 新查询进行中保留旧结果并显示进度状态；只有当前查询成功后才整体替换旧结果。
- 成功返回零条结果时，旧结果被替换为空状态。
- 关闭窗格不清除最后一组结果；同一进程内再次打开可立即恢复。

### 2.4 结果激活

| 结果类型/操作 | 行为 |
|---|---|
| 双击或 Enter：文件夹 | 在活动面板的新标签页打开该文件夹 |
| 双击或 Enter：文件 | 在活动面板的新标签页打开父目录，并选中该文件 |
| Ctrl+Enter：文件 | 使用系统默认程序直接打开文件 |
| 右键 | 尽可能复用 Shell 上下文菜单 |

“活动面板”在用户激活结果的瞬间解析；查询开始时所在面板不锁定结果的目标。若目标路径已消失，保留结果行并显示系统错误，不导航到近似路径。

## 3. 架构设计

### 3.1 组件

| 组件 | 职责 |
|---|---|
| `EverythingSearchBar` | 工具栏组合控件、下拉菜单、输入和提交事件 |
| `EverythingSearchController` | 查询代次、状态机、结果替换和导航命令 |
| `EverythingQueryBuilder` | 把范围、用户表达式和匹配选项编译成不互相污染的 IPC 查询 |
| `IEverythingIpcClient` | 可测试的异步搜索接口 |
| `EverythingIpcClientWin32` | Everything IPC 版本协商、请求和消息解析 |
| `EverythingSearchModel` | 当前成功结果、分页缓存和选中状态 |
| `EverythingSearchPane` | 右侧标题、错误横幅、虚拟列表和分隔条 |
| `EverythingSearchStorage` | 窗格宽度、可见性及可选的最后查询持久化 |

`EverythingSearchController` 不直接依赖窗口全局单例；通过接口获取当前活动 `BrowserPane`，以便双面板测试。

### 3.2 查询设置模型

```text
EverythingSearchSettings
├─ scope: CurrentFolder | Global
├─ matchCase: false
├─ matchWholeWord: false
├─ regularExpression: false
├─ ignoreDiacritics: true
└─ matchPath: false
```

选项映射如下：

| 菜单选项 | Everything 语义 | 默认值 |
|---|---|---|
| 区分大小写 | Match Case | 关闭 |
| 全词匹配 | Match Whole Words | 关闭 |
| 正则表达式 | Regular Expressions | 关闭 |
| 忽略变音标记 | `Match Diacritics = false` / `no-diacritics:` | 开启 |
| 匹配路径 | Match Path，即用户表达式同时匹配完整路径 | 关闭 |

“忽略变音标记”是 Everything 官方“Match Diacritics”的反向 UI 表达：菜单勾选时协议值为“不匹配变音标记”。这一反向映射必须集中在 `EverythingQueryBuilder`，不能分散在菜单和 IPC 层。

范围与“匹配路径”彼此独立：当前文件夹范围始终限制候选集合；“匹配路径”只决定用户输入匹配文件名还是完整路径，关闭它不会解除目录限制。

### 3.3 IPC 选择

首版使用 Everything 官方 IPC 的异步查询协议：

- 通过本地窗口消息发现 Everything 并发送 `WM_COPYDATA` 请求。
- 请求包含接收回复的 HWND、唯一 reply ID、排序、返回字段、offset 和 max results。
- 每次提交分配单调递增的 64 位内部 generation；reply ID 映射到 generation。
- 仅处理当前 generation 的回复；迟到回复解析后立即丢弃。
- UI 线程不等待 Everything，不使用同步轮询。
- 客户端在读取任何变长结构前验证消息长度、项目数、字符串偏移和终止符，拒绝越界或畸形回复。
- 每个请求携带不可变的 `EverythingSearchSettings` 快照和范围路径，菜单在请求期间改变只影响下一次提交。

选择 IPC 的理由是避免随应用分发 `Everything32/64.dll`、避免 DLL 位数匹配问题，并保持 Everything 为清晰的外部依赖。协议依据见 [Everything SDK](https://www.voidtools.com/en-uk/support/everything/sdk/)、[Everything IPC](https://www.voidtools.com/support/everything/sdk/ipc/) 和 [Everything_Query](https://www.voidtools.com/support/everything/sdk/everything_query/)。

### 3.4 查询构造

- 全局范围不附加目录约束。
- 当前文件夹范围使用提交瞬间解析出的绝对文件系统路径，并生成独立的目录约束；路径包含空格、引号或 Everything 运算符时必须按官方搜索语法转义。
- 普通搜索可使用官方“带结尾反斜杠的文件夹路径”限制到该文件夹树。正则模式不得把目录路径当作用户正则的一部分；query builder 应使用显式的非正则范围子表达式，再把用户文本放入正则子表达式。
- 如果当前 IPC/Everything 版本无法安全组合“目录范围 + 正则表达式”，本次查询应明确报“不支持此组合”，不能退化为全局正则搜索。兼容性测试通过前不得宣称支持该组合。
- 目录范围表达式和用户表达式采用逻辑 AND。用户表达式内部的空格、`|`、`!`、函数和宏仍保持 Everything 原生含义。
- query builder 返回结构化诊断信息，但日志只记录范围类型和选项位，不记录用户文本或目录。

Everything 官方说明：带结尾反斜杠的路径可限制到指定文件夹树，完整路径选项、大小写、全词、正则和变音标记均是独立搜索选项，参见 [Everything 搜索说明](https://www.voidtools.com/support/everything/searching/) 和 [搜索修饰符](https://www.voidtools.com/support/everything/search_modifiers/)。

### 3.5 查询与分页

- 首次请求最多 500 条，返回名称、完整路径所需字段、大小、修改时间和文件夹标志。
- 若协议返回总数大于已加载数，模型保留总数并按页加载后续结果。
- 虚拟列表接近未加载区域时请求相应 offset；同一页只允许一个在途请求。
- 新主查询取消逻辑上的所有旧分页请求；协议无法取消的旧回复由 generation 过滤。
- 排序默认使用 Everything 的名称升序；用户点击列标题后发起新的服务端排序查询，不在 UI 线程对全量结果排序。
- 模型设置可配置硬上限，初始建议 100,000 条；到达上限时明确显示“结果已截断”。

### 3.6 状态机

```text
Hidden ──提交查询──> Searching ──成功──> ShowingResults
  │                     │  └─失败──> ErrorWithPreviousResults
  └─打开窗格──> Idle    └─新查询──> Searching(new generation)

ShowingResults ──新查询──> Searching ──成功──> ShowingResults(replaced)
ShowingResults/Error/Idle ──关闭──> Hidden（模型仍保留）
```

关键不变量：

- 一个窗口只有一个“当前主查询 generation”。
- 旧回复不能改变可见结果、标题或错误状态。
- 搜索失败不删除上一次成功结果。
- 新查询成功后一次性替换结果，避免新旧列表混合。

### 3.7 与现有搜索功能的关系

- 现有 `MainToolbarButton::Search` 和 `OnSearch()` 保持不变。
- Everything 输入框是新的固定控件，不复用或重定义旧 Search 按钮。
- 两套功能可以同时存在；未来是否淘汰旧搜索需单独决策和迁移设计。

### 3.8 与窗口布局的关系

主窗口从外到内按以下逻辑布局：

```text
[文件夹树] [单/双文件面板工作区] [Everything 结果] [已有的垂直 Display Window]
```

- Everything 窗格属于整个窗口，不属于右侧文件面板。
- 若已有 Display Window 使用底部布局，Everything 仅占工作区右侧。
- 若 Display Window 也设为垂直布局，两者可以相邻存在，各自保留宽度。
- 文件面板工作区至少保留 320 DIP。空间不足时只临时压缩 Everything 的实际宽度，不覆盖其持久化首选宽度；仍不足时折叠 Everything 窗格并显示可恢复图标。
- 双面板的最小宽度约束优先于辅助窗格的首选宽度。

### 3.9 存储

向 `WindowStorageData` 增加版本化字段：

- `everythingSearchPaneVisible`
- `everythingSearchPaneWidthDips`
- `everythingSearchLastQuery`（默认不跨进程保存；仅用户显式开启“恢复搜索”时写入）
- `everythingSearchScope`（首次默认为 `CurrentFolder`）
- `everythingMatchCase`、`everythingMatchWholeWord`、`everythingRegex`、`everythingIgnoreDiacritics`、`everythingMatchPath`

搜索范围和五个选项作为应用级用户偏好持久化，新窗口继承它们；已经提交的请求保留自己的设置快照。XML 和注册表读取必须允许字段缺失；缺失时采用“窗格隐藏、默认宽度 420 DIP、当前文件夹范围、仅忽略变音标记开启”。宽度以 DIP 保存，加载后按当前显示器 DPI 换算并限制在合法范围。

### 3.10 安全与隐私

- 查询通过本机窗口 IPC 发送，不连接互联网。
- 不在常规日志、崩溃附加信息或遥测中记录查询文本、完整结果路径。
- 可记录 generation、耗时、结果计数、分页号、IPC 版本和错误码。
- IPC 回复视为不可信的跨进程输入，必须完整做边界检查。
- Shell 打开和上下文菜单继续使用项目已有的 PIDL/路径验证机制。

## 4. 失败与恢复

| 情况 | 用户可见行为 | 数据行为 |
|---|---|---|
| Everything 未运行 | 窗格显示“Everything 未运行”和“重试” | 保留旧结果 |
| Everything 未安装 | 显示说明及官方下载安装链接 | 不自动下载或启动安装程序 |
| IPC 超时 | 显示超时和重试 | 当前请求失效，保留旧结果 |
| 回复格式非法 | 显示协议错误 | 丢弃整条回复，不部分更新 |
| 新查询覆盖旧查询 | 仅显示新查询的进度 | 迟到旧回复被丢弃 |
| 结果路径已删除 | 激活时显示系统错误 | 结果仍保留到下次成功查询 |
| 当前范围不是文件系统目录 | 提示改用全局搜索 | 不发送查询，不自动扩大范围 |
| 当前 Everything 版本不支持安全的选项组合 | 显示具体不兼容选项 | 不发送降级查询 |

可设置 5 秒无回复超时；超时只结束本次 UI 等待，不假设 Everything 进程已经退出。

## 5. 代码影响点

| 位置 | 设计变更 |
|---|---|
| `MainToolbar.*` | 固定搜索组合控件及响应式布局 |
| `MainWndSwitch.cpp` | 搜索提交、窗格显隐和结果激活消息 |
| 新增 `EverythingSearch*` 文件 | 设置、查询构造器、控制器、IPC、模型和窗格 |
| `MsgHandler.cpp` | 右侧窗格和分隔条布局 |
| `WindowStorageData` 及 XML/Registry 存储 | 可见性和 DIP 宽度兼容字段 |
| Shell 导航/上下文菜单入口 | 打开父目录并选中文件、打开文件夹 |

第三方协议常量或结构若复制自官方 SDK，必须在源文件和第三方声明中保留来源与许可证信息；实现前由维护者确认 Everything SDK 当前许可证与再分发要求。

## 6. 验收标准

- 搜索框固定在主工具栏，工具栏自定义和旧配置不会删除或序列化该输入框。
- 下拉菜单默认选中“当前文件夹下”和“忽略变音标记”，其余四项默认关闭。
- 当前文件夹与全局范围互斥，五个匹配选项可以独立组合并跨重启恢复。
- 当前文件夹搜索包含全部后代，目录在提交瞬间从活动面板获取。
- “匹配路径”关闭时仍严格限制在当前目录树；它只改变用户表达式的匹配字段。
- 虚拟目录下不会静默执行全局搜索。
- 菜单助记键 A/B/C/D/F 只在菜单打开时触发对应选项。
- 原生 Everything 查询语法可以直接使用，不被 Explorer++ 改写。
- 搜索在 UI 线程上无同步等待；Everything 延迟或无响应时主窗口仍可操作。
- 新查询成功前旧结果保持可见；成功后一次性替换。
- 迟到的旧查询和旧分页回复不能覆盖当前状态。
- 一万条以上结果滚动时不为每条结果创建窗口对象，内存增长受控。
- 双击文件会在动作发生时的活动面板中打开父目录的新标签并选中文件。
- 双击文件夹会在活动面板新标签中打开文件夹。
- Everything 不存在、未运行、超时和协议异常均有明确且可恢复的状态。
- 窗格显隐和宽度跨重启恢复，并在 DPI 变化后保持合理尺寸。
- 与垂直 Display Window 和双面板同时开启时，布局不重叠且中央工作区遵守最小宽度。

## 7. 非功能要求

- 提交查询到显示“搜索中”状态不超过一个 UI 消息循环。
- 在 Everything 正常响应的基准机上，首批 500 条结果目标为 250 ms 内显示；该值是性能目标，不是对外部进程的硬保证。
- 解析 IPC 使用整数溢出安全的长度校验，并有畸形消息单元测试。
- 查询构造器覆盖路径含空格、引号、中文、尾反斜杠及 Everything 运算符的测试，并覆盖五个选项的全部单项映射。
- 所有新增 UI 支持高 DPI、键盘导航、屏幕阅读器名称和高对比度主题。

## 8. 已知限制与后续扩展

- 首版依赖桌面版 Everything 正在运行。
- 首版不提供数据库状态、索引规则或服务管理界面。
- 首版使用固定 150 ms 防抖自动查询，不在每次按键时直接发送 IPC 请求。
- 若最低支持的 Everything 版本无法安全组合当前目录与正则表达式，应在实现前确定最低版本或把该组合列为暂不支持。
- 预览、内容搜索模板、查询历史和多结果窗格不在首版范围内。

## 9. 相关文档

- [FEAT-003：双面板](FEAT-003_dual_pane.md)
- [Everything SDK](https://www.voidtools.com/en-uk/support/everything/sdk/)
- [Everything IPC](https://www.voidtools.com/support/everything/sdk/ipc/)
- [Everything_Query](https://www.voidtools.com/support/everything/sdk/everything_query/)
