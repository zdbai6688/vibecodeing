# Project Context

> Updated: 2026-08-08 (Asia/Shanghai)

## Current Goal

推进 **UOS速记 v1.1** 收口与 **v1.2 规划**：Phase 0~5 开发任务 19/20 done（P4-T9/IDE-201 国际化已 done），
Phase 5 已完成：v1.1 回归通过、deb/玲珑包构建成功、v1.1.0 Release Notes 与 tag 就绪。
2026-08-08 收口：dev-plan P1-T2 单测 QWARN 已全部消除（数据库 v2→v3 迁移建表顺序 + BackupService 连接释放），
仅剩 **IDE-186（P1-T2）GUI 功能验收** 待真实桌面人工复核（23 项 ⚠️ 用例，清单见 docs/gui-acceptance-checklist.md）。
v1.2 已建卡规划（父任务 IDE-209，子任务 IDE-210~214，5 项功能增强）。

- 看板父任务：IDE-181，子任务 IDE-182 ~ IDE-203（stage 1~6 = Phase 0~5）
- 状态：**19/20 done / 1 in_progress**（IDE-182 ~ IDE-203；IDE-186 待 GUI 验收收口）
  - done：P0-T1~T3、P1-T1/T3、P2-T1~T3、P3-T1/T2、P4-T1~T9、P5-T1/T2（IDE-182~203 除 IDE-186 外全部 done）
  - in_progress：IDE-186（P1-T2）36 项 GUI 用例中 23 项 ⚠️ 待真实桌面人工复核

## Active Branch

- 分支：`feature/phase2-utility-tools`（upstream 已建立，push 直接用 `git push`）
- 与 origin 同步：ahead **0** / behind **0**
- HEAD：`bd2ab37`（docs(IDE-186)：新增 v1.1.0 功能验证测试报告；构建/单测/打包全绿，GUI 用例标注环境受限项）
- Tag：`v1.0.0`、`v1.1.0`（annotated，均已推送 origin）
- Remote：`git@github.com:zdbai6688/vibecodeing.git`（git 仓库根 = `/home/ut005200@uos`）

## Working Tree

项目目录 `06 AI工作提效/40 小U速记260727/`（注意：git 根是 home 目录，**不要 `git add -A`**）：

- 跟踪文件改动：无（已干净；IDE-201 提交 `cadfccf` 已推送）
- 未跟踪：`build-deb/` 等构建产物（不入库）
- WIP 保底提交 `78509d5` 已按 issue 拆分收口（IDE-195~200），不再有游离 WIP

## Key Artifacts

- `docs/dev-plan.md` — v1.1 计划 + Task→Issue 映射（含 Task Table 实时状态）
- `docs/remaining-work.md` — 剩余工作清单
- `docs/project-context.md` — 本文档（协作上下文锚点）
- `docs/product-spec.md`、`docs/todo-refactor-plan.md` — 功能与待办重构依据（IDE-191/192 用）
- `docs/RELEASE_NOTES.md` — 发布说明（v1.1.0 已就绪）
- `docs/gui-acceptance-checklist.md` — IDE-186 遗留 23 项 GUI 用例的人工验收清单（操作步骤+预期结果）
- `docs/remaining-work.md` — v1.2 及以后候选清单（20/25 完成，5 项待开发）
- `build-check/`、`build-tests/` — 最近验证构建（Release / BUILD_TESTS=ON）

## Next Recommended Step

1. **用户最终验收**：按 `docs/gui-acceptance-checklist.md` 在真实桌面逐项执行 23 项 ⚠️ 用例（约 30~60 分钟），勾选结果 → 更新 docs/test-report-v1.1.0.md → 置 IDE-186 done。
2. **v1.2 开发**：看板已建卡 IDE-209~214（说话人识别/启动优化/标签颜色/周报模板/备份恢复），待开工时从 IDE-210 开始，走 sdlc-plan → sdlc-build。
3. **sdlc-release**：商店上架材料已更新为 v1.1.0（store-listing.md），需真实桌面截图后补齐截图位并提交商店审核、发布 GitHub Release。

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

- **IDE-186 GUI 验收未收口**：23 项 ⚠️ 用例需真实桌面人工复核（Markdown 编辑/预览、待办看板 UI、快捷键、会议录音、AI 类需 API Key 等），agent 环境无显示服务无法代跑
- **Wayland 全局快捷键**：Alt+Space 自定义在 Wayland 下有已知兼容问题，验收时需覆盖 X11/Wayland 两种会话
- **AI 类用例依赖 API Key**：TC21~26/32 需配置真实 API Key 才能验收
- **构建环境陷阱**：`~/bin/cmake` 损坏（CMAKE_ROOT 找不到），误用会白跑一次；统一用 `/usr/bin/cmake`
- **目录名双轨**：git 历史中旧路径「40 UOS 速记260727」与新路径「40 小U速记260727」并存，diff 时需带 rename 检测
