# AI Context Loading Rules

本文件说明 AI 在不同任务中应该优先读取哪些资料。原则是按需加载，不一次性读取全部文档。

## Priority Levels

- P0：必须先读。缺失时不能开始执行，需先确认或补齐。
- P1：按条件读取。只有命中 `when` 条件时才读。
- P2：背景资料。只有 P0/P1 不足以判断时再读。

## Always P0

每次会话优先读取：

1. `AGENTS.md`
2. `.ai/project_state.md`
3. `.ai/project.yml`
4. 与当前任务直接相关的用户输入、代码、测试、日志或错误信息

## Global P2

以下资料不绑定具体任务类型。只有 P0/P1 不足以判断，且命中条件时才读取：

- `doc/09_note/`：需要项目内机制、排障方法或长期知识沉淀时读取。

## Routes

### 新功能

P0:
- `doc/04_features/` 对应 Feature

P1:
- `doc/02_contracts/`：涉及 API、事件、Schema 或兼容性时读取。
- `doc/03_modules/`：涉及具体模块实现或边界时读取。
- `doc/06_lld/`：逻辑复杂、状态机、并发、异常路径较多时读取。

P2:
- `doc/07_decisions/`：需要理解历史技术取舍时读取。
- `doc/10_references/`：需要外部规范或机制背景时读取。

### Bug 修复

P0:
- `MEMORY.md`
- `memory/` 中与 scope、标签或错误现象匹配的记录
- 真实日志、错误信息、失败测试或最小复现

P1:
- `doc/03_modules/`：定位到相关模块后读取。
- `doc/08_runbooks/`：涉及启动、环境、部署、回滚、诊断命令时读取。
- `doc/05_tasks/` 对应 TASK：长排障或多步骤任务需要记录断点时读取。

P2:
- `doc/07_decisions/`：修复可能触碰历史架构取舍时读取。
- `doc/10_references/`：需要外部机制或规范背景时读取。

### API / 事件 / Schema 变更

P0:
- `doc/02_contracts/` 对应契约

P1:
- `doc/07_decisions/`：涉及兼容性承诺、版本策略或重大取舍时读取。
- `doc/04_features/`：变更影响用户功能或系统行为时读取。
- `doc/06_lld/`：变更影响状态机、数据结构、异常路径或迁移时读取。

P2:
- `doc/10_references/`：需要外部协议、SDK 或规范背景时读取。

### 架构 / 技术选型

P0:
- `doc/01_architecture/`
- `doc/07_decisions/` 相关 ADR

P1:
- `doc/02_contracts/`：架构变化影响接口、事件或数据 Schema 时读取。
- `doc/03_modules/`：架构变化影响模块边界时读取。

P2:
- `doc/10_references/`：需要外部资料、基准、规范或选型背景时读取。

### 本地启动 / 测试 / 部署 / 回滚

P0:
- `doc/08_runbooks/` 对应 Runbook

P1:
- `.ai/verification.md`：需要确认验证层级、证据或高风险验证要求时读取。
- `.ai/project.yml` 的 commands 和 paths：需要执行命令或定位路径时读取。

P2:
- `memory/`：排查历史环境坑或发布事故时读取。

### 复杂实现 / LLD

P0:
- `doc/06_lld/` 对应 LLD

P1:
- `doc/04_features/`：需要确认功能目标、非目标或用户流程时读取。
- `doc/02_contracts/`：涉及接口、事件或数据格式时读取。
- `doc/03_modules/`：涉及模块职责、边界或依赖时读取。

P2:
- `doc/07_decisions/`：需要理解历史技术取舍时读取。

### 代码审查

P0:
- 变更 diff
- 相关测试
- 相关契约或模块文档

P1:
- `prompts/code_review.md`：需要标准审查清单时读取。
- `memory/`：审查区域有历史踩坑时读取。

P2:
- `doc/07_decisions/`：变更可能违背既有 ADR 时读取。

## Stop Conditions

以下情况应停止扩大上下文，先执行判断或向用户确认：

- P0 资料已经足够完成当前小步验证。
- P1/P2 资料与当前文件、模块、错误或任务没有明确关联。
- 继续加载会引入大量旧背景但不能改变当前决策。

以下情况必须先确认或补充：

- 任务目标不清楚。
- 不知道哪些路径不能修改。
- 无法判断完成标准。
- 变更会影响兼容性、安全、权限、数据或部署流程。
