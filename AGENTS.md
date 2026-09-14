# AGENTS.md

除非用户明确要求，否则优先遵守本文件和 `.ai/guardrails.md` 的规则。

---

## Mission

- 项目目标：REQUIRED_TODO，用一句话说明本项目真正要达成的业务或技术闭环。
---

## Hard Rules

## 重要原则：快速与轻量

- 设计和实现必须优先保持快速启动、快速响应和低资源占用；不要为可能的未来需求预先引入复杂架构、常驻服务或重型依赖。
- 优先复用现有 Win32、Explorer++ 模块和系统能力；新增依赖、后台线程、缓存或长期运行任务必须有明确的性能收益和必要性。
- UI 交互和外部依赖调用不得阻塞主线程；失败应快速、明确地反馈，不能用隐藏重试、静默降级或模拟结果掩盖问题。
- 大功能先交付可编译、可验证的轻量版本，再按真实需求迭代扩展；避免无关重构、批量改造和一次性扩大范围。

## Context Loading

- 详细 P0/P1/P2 加载优先级见 `.ai/context_rules.md`。
- 在修改文件、运行脚本、触碰数据/接口/部署/依赖或创建实验代码前，先读 `.ai/guardrails.md`。
- 不要一次性加载全部 `doc/`。
- 长期文档按 `KNOWLEDGE.md` 和 `MEMORY.md` 的检索说明读取真实文件树。

## 每次会话先读取

- 按 `.ai/context_rules.md` 的 Always P0 执行；不要在本文件重复维护完整加载清单。
- **必须读取 `doc/00_overview/` 下除模板（`*_template.md`）以外的全部文件内容，**
  再根据任务类型继续加载其他 P0/P1/P2 文档；不得只凭文件名或旧会话摘要代替读取。

## Source Of Truth

- 项目地图、命令、路径：`.ai/project.yml`
- 项目级防护栏：`.ai/guardrails.md`
- 验证规则：`.ai/verification.md`
- 文档健康检查：`python scripts/check_docs.py`
- 当前状态：`.ai/project_state.md`

## Done Means

- 相关代码、配置、脚本或文档已按目标更新。
- 必要验证已执行；无法执行时说明原因和风险。
- 无关重构、无关格式化、无关依赖和无关文件 churn 已避免。
- 当前状态变化已同步到 `.ai/project_state.md`；长期价值已沉淀到 `doc/`，历史变已记录在`./memory/`。
