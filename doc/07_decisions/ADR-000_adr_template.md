---
status: proposed
# proposed | accepted | deprecated | superseded
decider: 决策人/团队
decision_summary: "TODO：一句话结论，例如：选用 PostgreSQL，放弃 MySQL，原因是 JSONB 支持更好"
affects:
  - TODO  # 受影响的模块或接口名称，供 AI 快速判断是否相关
related_adr: N/A
# 填写关联的 ADR-XXX 编号，无则填 N/A
related_task: TASK-XXX
superseded_by: N/A
# 若已被新 ADR 替代，填写新 ADR 文件名
review_date: YYYY-MM-DD HH:MM:SS
# 计划重新评估的日期或里程碑
last_verified: YYYY-MM-DD HH:MM:SS
---

# ADR-000: [决策标题]

## 背景

> 描述当时的处境：遇到了什么问题、有哪些约束、为什么现在必须做这个决策。

**问题描述：**

TODO

**约束条件：**

- TODO

**触发原因：**

TODO

---

## 决策

> 我们决定：[一句话明确的决策结论]

范围和边界：

- 影响哪些模块/功能：TODO
- 不涉及哪些部分：TODO

---

## 方案对比

### 方案 A：[方案名称]（已采用）

核心思路：TODO

优点：

- TODO

缺点/代价：

- TODO

实现复杂度：低 / 中 / 高

### 方案 B：[方案名称]（已放弃）

核心思路：TODO

优点：

- TODO

缺点/代价：

- TODO

放弃的关键原因：

> 不能只写"太复杂"，要写清楚哪个约束导致该方案不可接受。

---

## 被否决的小决策 / 不采用项

> 记录与本 ADR 相关、但不值得单独写 ADR 的轻量否决项，避免后续 AI 或开发者反复提出已被排除的建议。非常临时、只影响当前任务的判断先记在对应 TASK 的“进度与决策”；如果未来仍会影响维护，再沉淀到这里。

| 方案 / 建议 | 否决原因 | 时间 | 何时重新考虑 |
|-------------|----------|------|--------------|
| TODO | TODO | YYYY-MM-DD HH:MM:SS | TODO / N/A |

---

## 采用方案 A 的理由

- 理由 1：TODO
- 理由 2：TODO
- 权衡取舍：TODO

---

## 后果与影响

### 正面影响

- TODO

### 负面影响 / 已知风险

- TODO

### 需要跟进的事项

- [ ] TODO

---

## 验证方式

- 验证指标：TODO
- 重新评估触发条件：TODO（如：性能下降超过 X%、团队规模超过 N 人、出现 Y 类故障）
- 计划回顾时间：YYYY-MM-DD HH:MM:SS / 某个里程碑
