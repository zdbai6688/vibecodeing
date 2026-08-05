# IDE-163 待办模块重构 — 实施方案

> 关联 Issue：IDE-163「待办模块重构：简化逻辑，对标备忘录/企业微信日程」
> 日期：2026-08-02
> 状态：规划完成，待实施

---

## 一、现有代码与数据模型梳理

### 1.1 数据模型现状

**`notes_todos` 表**（笔记/待办共用）：

| 字段 | 类型 | 说明 |
|------|------|------|
| id | INTEGER PK | 主键 |
| title | TEXT | 标题 |
| content | TEXT | 内容 |
| content_type | TEXT | markdown/richtext |
| **is_todo** | INTEGER | 0=笔记，1=待办 |
| **priority** | INTEGER | 0=无，1=低，2=中，3=高 |
| **is_completed** | INTEGER | 完成状态 |
| **tag** | TEXT | **单个标签字符串**（不支持多标签） |
| **due_datetime** | INTEGER | 截止日期时间戳 |
| completed_datetime | INTEGER | 完成时间 |
| is_deleted | INTEGER | 软删除 |

**`tags` 表**：`id, name(UNIQUE), color, created_at`

**问题**：
- 待办与笔记共用一张表，`is_todo` 区分，概念混淆
- `tag` 是**单个字符串**，不支持多标签
- 无待办-标签多对多关联表

### 1.2 代码结构现状

| 层 | 文件 | 现状 |
|----|------|------|
| 数据结构 | `TodoData`（todostorage.h） | id, noteId, title, priority, isCompleted, tag(单), dueDatetime... |
| 数据访问 | `TodoStorage` | 完整 CRUD + 查询（Pending/Completed/Deleted/Today/Overdue/Week/ByTag/Search）+ 批量操作 + 排序 |
| 业务层 | `TodoManager` | 完整封装，带信号（created/updated/deleted/toggled/dataChanged） |
| UI 层 | `TodoWidget` | 顶部输入框 + 4 分区（逾期/今日/本周/已完成）+ 排序 + 批量 + 右键菜单 + 日期选择器 |
| 周报 | `WeeklyReportWidget` | 横向日历（周一~周五）+ 统计 + AI 周报 |

---

## 二、分阶段实施方案

### Phase A：数据模型增强（多标签支持）

**目标**：待办支持多标签，为周报按标签统计打基础。

1. **新增关联表** `todo_tags`：
   ```sql
   CREATE TABLE IF NOT EXISTS todo_tags (
       todo_id INTEGER NOT NULL,
       tag_id  INTEGER NOT NULL,
       PRIMARY KEY (todo_id, tag_id),
       FOREIGN KEY (todo_id) REFERENCES notes_todos(id),
       FOREIGN KEY (tag_id) REFERENCES tags(id)
   );
   ```

2. **TodoData 增加多标签**：
   - 新增 `QStringList tags` 字段（替代单 `tag`）
   - 保留 `tag` 兼容旧数据

3. **TodoStorage 扩展**：
   - `createTodo` 支持多标签关联
   - 新增 `setTags(int id, const QStringList &tags)`
   - `getTodosByTags(const QStringList &tags)` 多标签筛选
   - 旧数据迁移：`tag` 字符串 → `todo_tags` 关联

4. **TodoManager 扩展**：对应方法透传

**验收**：待办可挂多个标签，标签 CRUD 完整，旧数据可迁移。

---

### Phase B：待办创建逻辑简化（统一入口）

**目标**：新建待办 ≤2 步，逻辑清晰。

1. **统一新建入口**：
   - 待办页顶部输入框（已有）+ Enter 立即创建（默认今天截止）
   - 快速录入 `Alt+Shift+S` 支持 `#标签` `!高` 语法
   - 笔记转待办（保留）

2. **输入语法增强**：
   - `#标签` → 自动创建/关联标签（多标签）
   - `!高/中/低` → 设置优先级
   - 自然语言日期（"明天"、"周五"、"7月15日"）

3. **空状态优化**：无待办时始终显示输入框，列表区显示引导（已部分实现）

**验收**：输入 "买牛奶 #生活 !中 明天" 创建带标签、中优先级、明天截止的待办。

---

### Phase C：周报日程联动（对标企业微信）

**目标**：周报 = 每周待办日程，按标签+日期统计。

1. **周视图展示每日待办**：
   - 横向日历每个日期格子显示当日待办数量/标题
   - 点击日期查看当日待办详情

2. **按标签统计**：
   - 周报统计区按标签分组展示完成情况
   - 每个标签：本周待办数、已完成数、完成率

3. **AI 周报增强**：
   - 提示词加入标签维度数据
   - 生成内容包含标签维度的总结

**验收**：周视图展示每日待办，周报按标签统计，AI 周报含标签维度。

---

### Phase D：UI 对齐（备忘录/企业微信风格）

**目标**：交互对标主流产品。

1. **日历网格视图**：待办按日期分布展示（日程化）
2. **拖拽调整日期**：拖拽待办到不同日期格子
3. **标签筛选**：侧边栏标签点击筛选待办（已有，增强多标签）
4. **视觉统一**：简单待办卡片 + 彩色标签徽章

**验收**：待办像日程一样按日期分布，可拖拽调整，标签筛选清晰。

---

## 三、优先级与工时

| 阶段 | 内容 | 优先级 | 预估 |
|------|------|--------|------|
| **A** | 数据模型多标签 | P0（基础） | 2天 |
| **B** | 创建逻辑简化 | P0 | 2天 |
| **C** | 周报日程联动 | P1 | 3天 |
| **D** | UI 对齐主流 | P2 | 3天 |

---

## 四、风险

| 风险 | 应对 |
|------|------|
| 数据迁移失败导致旧待办丢失 | 迁移前备份，`tag` 字符串解析为关联 |
| 多标签改动影响现有 UI | 保留单标签兼容，逐步迁移 |
| 周报重构工作量大 | 分阶段，先按标签统计，再日历化 |

---

## 五、建议实施顺序

1. **先做 Phase A**（数据模型），所有后续依赖它
2. **Phase B**（创建逻辑）独立可交付，用户立即受益
3. **Phase C**（周报联动）基于 A
4. **Phase D**（UI 对齐）最后，视觉打磨

**当前建议**：立即开始 Phase A（新增 todo_tags 表 + 多标签数据层）。