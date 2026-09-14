# 项目知识导航 (Project Knowledge Navigation)

本文件不是完整文档清单，也不要求手工维护每个文档链接。真实索引以 `doc/`、`memory/` 的目录结构、文件名和 frontmatter 为准。

AI 协作时优先使用文件检索工具，例如：

```bash
rg --files doc memory
rg "scope:|type:|status:|stability:|review_date:" doc memory
rg "#AUTH|#API|#BUILD|#DEPLOY" doc memory
```

---

## 查找规则

| 目标 | 优先位置 | 说明 |
|------|----------|------|
| 当前状态、临时约束、近期坑 | `.ai/project_state.md` | 每次会话优先读取 |
| 项目概览、AI 上下文、目标、术语、非目标 | `doc/00_overview/` | 项目理解入口 |
| 架构、边界、部署视图 | `doc/01_architecture/` | 系统级设计 |
| API、事件、数据 Schema | `doc/02_contracts/` | 契约唯一规范来源 |
| 模块职责、依赖、测试策略 | `doc/03_modules/` | 代码边界和模块理解 |
| 用户功能、系统功能 | `doc/04_features/` | 特性设计、行为边界和验收标准 |
| 可执行工作 | `doc/05_tasks/` | 任务范围、验收和验证 |
| 复杂实现细节 | `doc/06_lld/` | 接口、状态机、异常路径、容量 |
| 技术决策、架构取舍 | `doc/07_decisions/` | ADR |
| 启动、测试、部署、回滚、排障 | `doc/08_runbooks/` | 操作手册 |
| 历史变更、维护注意事项 | `memory/` | 动态结构化 Memory |
| 项目内知识笔记、机制沉淀 | `doc/09_note/` | 项目消化后的长期笔记 |
| 外部资料、规范链接 | `doc/10_references/` | 外部资料和链接索引 |

---

## 标签建议

常用标签示例：

- 模块：`#AUTH`、`#PAYMENT`、`#BUILD`、`#DEPLOY`
- 行为：`#FIX`、`#FEAT`、`#REFACTOR`、`#TEST`
- 风险：`#API`、`#DATA`、`#SECURITY`、`#BREAKING`

标签应写入具体文档 frontmatter 或正文，不在本文件维护集中清单。

---

## 生成索引

需要查看当前文件树时，运行：

```bash
python scripts/check_docs.py
```

脚本会基于真实文件树输出文档分布，而不是依赖手工索引。
