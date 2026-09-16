# Memory 检索说明与核心记忆

本文件用于统一说明项目 Memory（记忆）的检索与维护规范。会话开始时 AI 应优先读取本文件，以获取当前有效、高价值的核心记忆，而无需加载全部历史文件。

真实且不篡改的历史记忆详情以根目录 `memory/` 下的各 Memory 文件（如 `MEM-001_xxx.md`）为准。

---

## 核心记忆

> [!NOTE]
> - 仅记录会在未来多个任务中持续有用且可能影响后续维护的的偏好、项目约束、关键路径、构建配置、架构决策、Skill 变更、验证结论和避坑经验。
> - 小范围格式调整、注释修正、无行为变化等无需新增 Memory。
> - 不要把临时任务、一次性错误日志、当天状态写入记忆。

### 1. 偏好与项目约束
- TASK 级任务完成后必须创建 Git commit，并运行 `tools/backup_project.py` 完成项目备份。

### 2. 关键路径与构建配置
- Visual Studio 2022 Build Tools 位于 `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`。
- TASK-001 的真实主窗口排障与验收经验见 `memory/MEM-001_task001_terminal_everything_dual_pane.md`。

---

## 历史记忆保存

### 写入规则

每条 Memory 应使用 `memory/MEM-000_memory_template.md` 的结构，并尽量填写 frontmatter：

```yaml
date: YYYY-MM-DD HH:MM:SS
type: fix
scope:
  - auth
still_live: true
related_task: TASK-XXX
```

关键字段：

- `scope`：受影响模块、功能、接口或部署域。
- `type`：`fix | feat | refactor | breaking | hotfix | docs | test | build | deploy | debug`。
- `still_live`：是否仍然影响当前代码或排障判断。
- `superseded_by`：如果已失效，指向替代记录。

---

### 检索方式

```bash
# 列出所有 Memory
rg --files memory

# 找仍然有效的记录
rg "still_live:\\s*true" memory

# 按模块或标签查找
rg "auth|#AUTH|scope:" memory

# 查找 breaking / hotfix / deploy 记录
rg "type:\\s*(breaking|hotfix|deploy)" memory
```

Bug 修复或排障时，先按 scope 和 still_live 过滤，再决定是否阅读正文。
