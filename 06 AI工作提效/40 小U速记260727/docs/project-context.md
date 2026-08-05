# Project Context

> Updated: 2026-08-05 (Asia/Shanghai)

## Current Goal

推进 **UOS速记 v1.1** 开发：Phase 3 周报日程联动（P3-T1/IDE-191）与待办日程化（P3-T2/IDE-192）已完成，
Phase 4 体验增强仅剩 P4-T2 多紧凑窗口（IDE-194，实现完成待验收收口），完成后进入 Phase 5
（v1.1 回归 + deb/玲珑打包 + Release Notes + 发布）。

- 看板父任务：IDE-181，子任务 IDE-182 ~ IDE-203（stage 1~6 = Phase 0~5）
- 状态：**17 done / 3 in_progress / 2 todo**（2026-08-05）
  - done：P0-T1~T3、P1-T1/T3、P2-T1~T3、P3-T1/T2、P4-T1、P4-T3~T8（IDE-182~193、195~200）
  - in_progress：P1-T2（IDE-186 测试）、P4-T2（IDE-194）、P5-T1（IDE-202）
  - todo：P4-T9（IDE-201 i18n）、P5-T2（IDE-203）

## Active Branch

- 分支：`feature/phase2-utility-tools`（upstream 已建立，push 直接用 `git push`）
- 与 origin 同步：ahead **0** / behind **0**
- HEAD：`d928bfb`（feat IDE-192 待办日程化——日历网格视图+拖拽改期+标签徽章）
- Tag：`v1.0.0`（annotated，已推送 origin）
- Remote：`git@github.com:zdbai6688/vibecodeing.git`（git 仓库根 = `/home/ut005200@uos`）

## Working Tree

项目目录 `06 AI工作提效/40 小U速记260727/`（注意：git 根是 home 目录，**不要 `git add -A`**）：

- 跟踪文件改动：无（已干净）
- 未跟踪：`build-deb/` 等构建产物（不入库）
- WIP 保底提交 `78509d5` 已按 issue 拆分收口（IDE-195~200），不再有游离 WIP

## Key Artifacts

- `docs/dev-plan.md` — v1.1 计划 + Task→Issue 映射（含 Task Table 实时状态）
- `docs/remaining-work.md` — 剩余工作清单
- `docs/project-context.md` — 本文档（协作上下文锚点）
- `docs/product-spec.md`、`docs/todo-refactor-plan.md` — 功能与待办重构依据（IDE-191/192 用）
- `docs/RELEASE_NOTES.md` — 发布说明（v1.1.0 待 IDE-203 更新）
- `build-check/`、`build-tests/` — 最近验证构建（Release / BUILD_TESTS=ON）

## Next Recommended Step

**sdlc-build** — 按优先级推进：

1. 收尾 **IDE-194**（P4-T2 多紧凑窗口，in_progress 待验收）：代码审查 + 构建验证后置 done，GUI 行为交测试
2. P4-T9（IDE-201 i18n）——v1.1 唯一剩余功能项
3. P1-T2（IDE-186 测试报告）、P5-T1（IDE-202 回归+打包）、P5-T2（IDE-203 Release Notes）

## Useful Commands

```bash
# 构建（注意：~/bin/cmake 是损坏包装器，必须用 /usr/bin/cmake）
/usr/bin/cmake -B build-check -DCMAKE_BUILD_TYPE=Release
/usr/bin/cmake --build build-check -j$(nproc)

# 测试
/usr/bin/cmake -B build-tests -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
ctest --test-dir build-tests --output-on-failure

# 看板（multica CLI，工作目录勿写 /tmp）
multica issue get IDE-<id> --output json
multica issue status IDE-<id> <status>
multica issue comment add IDE-<id> --content-file <workdir路径>
```

## Open Risks

- **IDE-193/194 仅代码级验证**：贴边隐藏/多紧凑窗口依赖 GUI 交互，行为验收留给 P1-T2 测试（IDE-186）
- **IDE-191/192 复杂度 L**：周报日程联动涉及 storage 层统计 + 周视图 UI + AI 提示词，需分步实现并加单测
- **i18n 未启动**：IDE-201（P4-T9）涉及 ts/qm 管线，预计 Phase 4 收尾后单独做
- **构建环境陷阱**：`~/bin/cmake` 损坏（CMAKE_ROOT 找不到），误用会白跑一次；统一用 `/usr/bin/cmake`
- **目录名双轨**：git 历史中旧路径「40 UOS 速记260727」与新路径「40 小U速记260727」并存，diff 时需带 rename 检测
