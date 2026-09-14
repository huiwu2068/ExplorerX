---
status: todo
# todo | in-progress | done | blocked
created: YYYY-MM-DD HH:MM:SS
updated: YYYY-MM-DD HH:MM:SS
owner: TODO
complexity: M
# S=半天内 | M=1~3天 | L=3天~1周 | XL=需要拆分
risk_level: low
# low | medium | high
feature: 对应的 feature 文档名（无则填 N/A）
module: 对应的 module 文档名（无则填 N/A）
blocked_by: N/A
# 填写前置依赖的 TASK-XXX 编号，无则填 N/A
checkpoint:
  current_step: 0
  last_stable_output: N/A
  blocked_at: N/A
  next_action: N/A
---

# TASK-000: [任务名称]

## 目标

> 一句话说明这个任务完成后，系统达到什么状态。

TODO

## 背景
- 现在项目处于什么阶段：TODO
- 为什么要做：TODO
- 当前现象 / 问题：TODO
- 相关证据：TODO，例如日志、截图、测试失败、用户反馈。
- 这个任务属于什么类型：TODO

## 要解决什么需求和问题

## 范围

### 包含

- TODO

### 排除

- TODO，明确本任务不处理的模块、场景或优化点。
- TODO，哪些功能不能动，哪些接口不能改，哪些数据不能破坏。

## 约束

- TODO: 最不能破坏的行为、接口、数据或兼容性要求。
- TODO: 不允许修改或需要谨慎修改的目录 / 文件。
- TODO: 必须遵守的规范、设计文档或 ADR。

## 风险与回滚

- 主要风险：TODO
- 回滚方案：TODO（如：回退 commit XXX，或执行 `TODO 命令`）
- 降级策略：TODO（如：功能开关、灰度控制）

## 行动路线

- [ ] 步骤 1：TODO
- [ ] 步骤 2：TODO
- [ ] 步骤 3：TODO

## 完成标准 (Definition of Done)

- [ ] 功能或修复达到目标描述的状态。
- [ ] 相关测试 / 构建 / 静态检查已执行，或说明无法执行原因。
- [ ] 日志、指标、截图、抓包或手工流程已验证关键行为（如适用）。
- [ ] 未引入无关重构、无关依赖或无关行为变化。
- [ ] 需要同步的文档、ADR、Memory 或配置示例已更新。

## 代码审查状态
### LLM自审


## 验证方式

```bash
TODO: 填写可复制执行的验证命令
```

预期结果：

- TODO

## 相关文档

- Feature: `../04_features/FEAT-XXX_feature_name.md`
- Module: `../03_modules/xxx_module_name.md`
- LLD: `../06_lld/LLD-XXX_xxx_lld.md`
- ADR: `../07_decisions/ADR-XXX_xxx_adr.md`
- API Contract: `../02_contracts/API-XXX_xxx_api.md`
- Runbook: `../08_runbooks/RUNBOOK-XXX_xxx_runbook.md`
- Memory: `../../memory/MEM-XXX_xxx_change_title.md`

## 相关代码


## 进度与决策

> 记录任务级恢复断点，替代会话草稿。只写对恢复任务有帮助的稳定信息，不记录一次性思考流水。

当前进度：

- TODO

当前阻塞：

- 无 / TODO

关键决策：

| 日期 | 决策 | 原因 | 影响 |
|------|------|------|------|
| TODO | TODO | TODO | TODO |
