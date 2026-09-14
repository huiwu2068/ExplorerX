# AI Guardrails

## Hard Prohibitions
- 不要用 mock、硬编码、特殊路由、隐藏重试来掩盖真实错误。
- 不要为了通过测试引入与生产行为不一致的分支。
- 每个 `doc/05_tasks/TASK-xxx_*.md` 级任务整体完成后，应 git commit + `tools/backup_project.py` 双备份；任务内子项、阶段性拆分或 checklist item 默认不单独提交/备份，除非用户明确要求，除非用户明确要求或有重大重构变更。
- 优先最小变更，不做无关重构、无关格式化或批量重排。
- 不要擅自删除已有的注释，除非注释是错误的，和文档或代码不匹配。
- 不要编造不存在的命令，不确定的信息要明确标注。

## Confirm First
- 任务存在多个互斥解释，且代码/测试/文档无法判断。
- 验证结果与任务目标明显冲突，需要改变目标或范围。
- 重大技术路线抉择，例如核心架构、数据存储、通信协议、部署平台等。

## Prefer

- 读真实代码、测试和日志后再动手。
- 复用项目已有配置、日志、错误处理、测试和部署机制。
- 小步变更，小范围验证。
- 把长期有效的踩坑和验证结论写入 `memory/`；通用知识沉淀写入 `doc/09_note/`。

## Project Skills

- 对重复出现，或首次即高风险、高排查成本的项目问题与工作流，确认可长期复用后再提炼为 Skill，可在根目录 `skills/<skill_name>/` 创建项目级 Skill。
- 仅在任务现象、模块或操作与其触发条件匹配时读取，不加入每次会话必读内容。
- 失效时标记 `deprecated` 并说明替代方式。

## Architecture Migration Checks

- 做目录、包名、任务编号等批量替换后，必须执行语义复查：用 `rg` 搜索旧路径、旧任务入口、以及明显错误的新句子，例如“目标目录已删除”这类由机械替换造成的表述。
- 废弃任务时必须维护完整 supersede 链：旧任务 front matter、正文提示、`.ai/project_state.md` 当前焦点和下次提醒都要指向新的唯一执行入口。
- 迁移目录时不要保留兼容壳，除非任务明确要求；如果删除兼容壳，必须同步 capability 检测、边界检查、golden/metadata 和文档路径。

## Memory
会话开始：
- 优先读取 `MEMORY.md` 的"核心记忆沉淀"部分，真实记录以根目录 `memory/` 下的 Memory 文件为准。


## Experimental Work

适用范围：临时脚本、新功能 PoC、技术验证、调试工具等一次性或探索性代码。

目录规范：

- 统一在项目根目录的 `tmp/` 下创建子目录，命名建议体现目的，如 `tmp/validate-auth-flow/`。
- 禁止将实验代码直接放置于 `tmp/` 根目录或项目业务模块中。
- 禁止使用系统级 `/tmp` 目录保存项目实验代码。

完成后必须在对应实验子目录内提交 `README.md`，例如 `tmp/validate-auth-flow/README.md`。这里的 `README.md` 指工具或实验子目录的说明文件，不是项目根目录的 `README.md`。

实验子目录的 `README.md` 必须包含：

- 功能说明：脚本/模块的用途与目标。
- 验证目的：本次探索要验证的假设或问题。
- 运行方式：执行命令与前置依赖。
- 结论与效果：验证结果及关键发现。


## Binary Document Handling

- PDF、Word、PPT 等二进制文档如需转换或抽取，应将 LLM 可复用的中间产物缓存到 `.ai/cache/doc_extracts/`。
- 缓存文件需记录 `source_file`、`converted_at`、`converter` 和产物格式；后续默认读缓存，不重复解析原文件。
- 仅当缓存缺失、源文件更新、需核对版式/图片/表格，或用户明确要求时，才重新读取原文件。
- 长期有效的内容整理进 `doc/` 或 `memory/`；缓存本身不是正式项目文档。


## Documentation Health Rules

### Required Markers

- `REQUIRED_TODO`：复制到真实项目后必须填写。
- `OPTIONAL_TODO`：可以后补，但应在稳定后清理。
- `N/A`：明确不适用。

### Freshness

长期文档建议在 frontmatter 中包含：

```yaml
status: draft
owner: REQUIRED_TODO
created: YYYY-MM-DD HH:MM:SS
updated: YYYY-MM-DD HH:MM:SS
last_verified: YYYY-MM-DD HH:MM:SS
verified_against_commit: OPTIONAL_TODO
```

### Maintenance Rules

- 新增 Feature、Task、ADR、Contract、Runbook、Memory 后，确保文件名、目录位置和 frontmatter 可检索；不要手工维护重复索引。
- 过期文档先标记 `deprecated` 或 `superseded`，不要直接删除历史原因。
- `doc/` 下所有 Markdown 文档必须有编号；模板文件使用 `TYPE-000_*_template.md`，例如 `TASK-000_task_template.md`、`ADR-000_adr_template.md`。
- 正式文档使用稳定编号或时间戳编号，例如 `TASK-001_fix_login.md`、`ADR-001_choose_postgres.md`、`2026-06-18_143000_fix_login_timeout.md`。
- 时间相关记录必须精确到秒，统一使用 `YYYY-MM-DD HH:MM:SS`。
