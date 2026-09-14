---
date: YYYY-MM-DD HH:MM:SS
type: fix
# fix | feat | refactor | breaking | hotfix | docs | test | build | deploy | debug
scope:
  - TODO
  # 受影响的模块、功能、接口或部署域，供 AI 快速过滤
tags:
  - "#MODULE_OR_FEATURE"
  - "#ACTION_TYPE"
  # 示例：#AUTH #FIX #REFACTOR #BUILD #TEST #API #DEPLOY
still_live: true
  # true = 仍然影响当前代码或排障判断；false = 已被后续变更覆盖
superseded_by: N/A
  # 若 still_live=false，填写替代它的 Memory 文件名
related_task: TASK-XXX
related_adr: ADR-XXX / N/A
related_contract: API-XXX / N/A
---

# MEMORY: [变更标题] (YYYY-MM-DD HH:MM:SS)

## 摘要

- 解决了什么：TODO
- 实际改了什么：TODO
- 验证结论：TODO
- 后续维护最该注意：TODO

## 背景与目的

- 背景：说明为什么需要进行此项变更，解决了什么痛点或实现了什么新能力。
- 目标：达到的具体技术指标、行为变化或设计要求。
- 相关任务 / ADR / Feature：TODO

## 详细变更记录

| 文件 / 模块 | 变更 | 原因 | 影响 |
|-------------|------|------|------|
| TODO | TODO | TODO | TODO |

## 验证记录

执行过的命令：

```bash
TODO
```

关键现象 / 日志 / 截图：

- TODO

结论：

- TODO: 是否已解决问题，是否已合并、发布或仍在观察。

## 影响范围

- 影响模块：TODO
- 兼容性影响：TODO
- 数据 / 配置影响：TODO
- 运维 / 部署影响：TODO
- 回滚方式：TODO
- 相关 Runbook：TODO / N/A

## 避坑与遗留注意事项

- TODO: 特别需要注意的陷阱、调试方法或未来待办。
