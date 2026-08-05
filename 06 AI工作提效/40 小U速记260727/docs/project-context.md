# Project Context

> Updated: 2026-08-05 (Asia/Shanghai)

## Current Goal

推进 **UOS速记 v1.1** 开发：当前处于 Phase 4（P4 体验增强，按 `docs/remaining-work.md` 优先级），
目标完成后进入 Phase 5（v1.1 回归 + deb/玲珑打包 + Release Notes + 发布）。

- 看板父任务：IDE-181，子任务 IDE-182 ~ IDE-203（stage 1~6 = Phase 0~5）
- Phase 0 收口基线已建立：目录名统一、分支分叉合并、**v1.0.0 tag 已打并推送**

## Active Branch

- 分支：`feature/phase2-utility-tools`
- 与 origin 同步：ahead **0** / behind **0**
- HEAD：`1029650`（Merge origin IDE-178 审查修复）
- Tag：`v1.0.0`（annotated，指向 1029650，已推送 origin）
- Remote：`git@github.com:zdbai6688/vibecodeing.git`（git 仓库根 = `/home/ut005200@uos`）

## Working Tree

项目目录 `06 AI工作提效/40 小U速记260727/`（注意：git 根是 home 目录）：

- 跟踪文件改动：**无**（已干净）
- 未跟踪：`build-deb/`（构建产物，不入库）
- 保底提交 `78509d5`：P4 WIP 改动 21 文件（深色主题 DPalette 适配、导出/存储/待办/周报等），
  尚未按 issue 拆分——后续需拆分到 IDE-196~200 等任务

## Key Artifacts

- `docs/dev-plan.md` — v1.1 计划 + Task→Issue 映射（IDE-189 已入库）
- `docs/remaining-work.md` — 剩余工作清单（IDE-189 已刷新，#5 归入进行中）
- `docs/project-context.md` — 本文档（协作上下文锚点）
- `docs/test-report-v1.0.0.md`、`docs/security-audit.md`、`docs/user-guide.md` — v1.0 基线材料
- `docs/RELEASE_NOTES.md` — 发布说明（v1.1.0 待 IDE-203 更新）
- `build-check/`、`build-tests/` — 最近验证构建（Release / BUILD_TESTS=ON）

## Next Recommended Step

**sdlc-build** — 继续 P4 功能开发：

1. 将保底提交 `78509d5` 的 P4 WIP 按 issue 拆分提交（IDE-196~200），优先收口
   **IDE-197 深色主题**（sidebarwidget DPalette 适配已完成大半，只剩验收）
2. 收尾在途任务：IDE-190（in_review，验收 964e15c 修复）、IDE-186（单测 QWARN）、
   IDE-193/194/196/198/199/200（in_progress）
3. 之后按序推进 IDE-192/195/201/203

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

- **P4 WIP 未拆分**：78509d5 一提交含 21 文件改动，验收与追溯成本高，需尽快按 issue 拆分
- **IDE-190 修复游离**：实现 commit `964e15c` 不在任何本地分支上（dangling），需确认其内容已合入 WIP 或 cherry-pick，避免丢失
- **被打断的任务**：IDE-184/186/190/193 曾被 "task cancelled by server" 打断，测试/审查结果需重新验证
- **构建环境陷阱**：`~/bin/cmake` 损坏（CMAKE_ROOT 找不到），误用会白跑一次；统一用 `/usr/bin/cmake`
- **目录名双轨**：git 历史中旧路径「40 UOS 速记260727」与新路径「40 小U速记260727」并存，diff 时需带 rename 检测
