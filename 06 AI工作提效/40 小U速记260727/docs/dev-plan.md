# UOS 速记 — 开发计划（Dev Plan）

> 版本：v0.1 | 日期：2026-07-27 | 基于：product-spec.md v0.1

---

## 1. 总体策略

### 1.1 技术选型

| 层次 | 选型方案 | 理由 |
|------|---------|------|
| UI 框架 | Qt6 + DTK6 | 系统原生、性能好、与 UOS 深度集成、社区活跃 |
| 开发语言 | C++17 | 与 Qt6/DTK6 原生绑定、性能最优 |
| 构建系统 | CMake | 与 deepin-voice-note 一致，支持跨平台 |
| 数据存储 | SQLite | 轻量、零配置、嵌入式可靠 |
| 富文本编辑 | Qt6 RichText + Markdown 扩展 | 轻量级，无需 WebEngine 依赖 |
| 音频录制 | GStreamer | 系统预装，deepin-voice-note已验证 |
| 语音识别 | Whisper.cpp（离线） + 可选云端 ASR | 离线优先，隐私保护，中文精度好 |
| AI 摘要 | DeepSeek API / 本地 LLM | 性价比高，可配置 |
| 全局快捷键 | KF6GlobalAccel / XDG 原生 | Wayland+X11 双协议支持 |
| 系统托盘 | Qt6 SystemTray | 原生托盘支持 |
| 打包发布 | deb + linglong（玲珑） | 覆盖 UOS 商店和玲珑商店 |

### 1.2 架构设计

```
┌─────────────────────────────────────────────────────────────────────┐
│                          UOS 速记 (uos-voice-note)                    │
├─────────────────────────────────────────────────────────────────────┤
│  UI 层                                                               │
│  ├── 主窗口 (笔记列表 + 编辑器 + 待办看板)                             │
│  ├── 快速录入窗口 (Alt+Space 全局唤醒)                                 │
│  ├── 会议助手窗口 (录音 + 转写 + 摘要)                                 │
│  └── 系统托盘 + 快捷菜单                                              │
├─────────────────────────────────────────────────────────────────────┤
│  业务逻辑层                                                            │
│  ├── NoteManager    — 笔记/待办 CRUD                                  │
│  ├── TodoManager    — 待办状态/优先级管理                              │
│  ├── MeetingManager — 录音/转写/摘要编排                               │
│  ├── TagManager     — 标签分类管理                                     │
│  ├── AIService      — LLM 摘要/润色/待办提取                           │
│  └── QuickEntry     — 全局快捷键快速录入                                │
├─────────────────────────────────────────────────────────────────────┤
│  基础设施层                                                            │
│  ├── Storage (SQLite ORM)  — 数据持久化                                │
│  ├── AudioRecorder (GStreamer) — 音频采集                              │
│  ├── ASREngine (Whisper.cpp) — 语音转文字                              │
│  ├── HotkeyManager — 全局快捷键注册                                    │
│  └── NetworkService — HTTP API 调用                                    │
├─────────────────────────────────────────────────────────────────────┤
│  系统集成层                                                            │
│  ├── DTK6 主题跟随 (深色/浅色)                                         │
│  ├── D-Bus 通信                                                        │
│  ├── XDG 桌面规范 (desktop/autostart/MIME)                             │
│  └── Wayland/X11 兼容                                                  │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.3 基于 deepin-voice-note 的复用策略

直接复用 deepin-voice-note 的成熟模块：

| 模块 | 复用方式 | 说明 |
|------|---------|------|
| 音频录制 (gstreamrecorder) | 直接复用 | GStreamer 管道已验证 |
| 音频裁剪 (audiotrimmer) | 直接复用 | 波形裁剪 UI + 逻辑 |
| 格式转换 (audioformatconverter) | 直接复用 | MP3/WAV/OGG/FLAC/AAC |
| 播放器 (vlcpalyer) | 直接复用 | VLC 播放引擎 |
| 数据库操作层 (vnotedbmanager) | 改造复用 | 扩展笔记/待办数据模型 |
| 文件夹管理 (vnotefolderoper) | 改造复用 | 扩展为标签+文件夹 |

需要新增开发：

| 模块 | 新增方式 | 说明 |
|------|---------|------|
| 笔记编辑器 (RichText/Markdown) | 新增 | 从 WebEngine 改为轻量 Markdown |
| 快速录入窗口 | 新增 | Alt+Space 全局唤醒 |
| 会议助手 | 新增 | 录音+转写+摘要完整流程 |
| ASR 语音识别 | 新增 | Whisper.cpp 集成 |
| AI 摘要服务 | 新增 | LLM API 调用 + prompt 管理 |
| 待办管理 | 新增 | 待办 CRUD + 看板 |
| 周报生成 | 新增 | 数据聚合 + AI 模板 |
| 系统托盘常驻 | 改造复用 | 扩展托盘菜单 |
| 全局快捷键 | 新增 | Wayland/X11 兼容 |

---

## 2. 开发路线图

### Phase 0：项目初始化（1周）

**目标**：搭建工程骨架，确认技术栈可用性

| 任务 | 估算 | 产出 |
|------|------|------|
| 创建 CMake 工程，配置 Qt6 + DTK6 | 1天 | 可编译空窗口 |
| 导入 deepin-voice-note 音频模块 | 1天 | 录音/播放可工作 |
| 搭建 SQLite 数据层 | 1天 | 数据库 CRUD 演示 |
| 配置 deb 打包 + linglong.yaml | 1天 | 可安装 deb |
| CI/CD 基础流水线 | 1天 | GitHub Actions 构建 |

### Phase 1：笔记+待办核心（3周）— MVP

**目标**：可用的笔记和待办管理功能

| 任务 | 估算 | 产出 |
|------|------|------|
| 笔记 CRUD + 列表视图 | 1周 | 新建/编辑/删除/搜索笔记 |
| Markdown 编辑器 | 3天 | 编辑/预览/语法高亮 |
| 待办管理 (CRUD + 看板) | 4天 | 新建/截止/优先级/完成 |
| 标签分类 + 筛选 | 2天 | 标签 CRUD / 按标签筛选 |
| 回收站 | 1天 | 软删除 / 恢复 / 清空 |
| 数据导入 (deepin-voice-note 兼容) | 2天 | 迁移旧数据 |

### Phase 2：快速录入+系统集成（2周）

**目标**：高效捕捉能力，系统级集成

| 任务 | 估算 | 产出 |
|------|------|------|
| Alt+Space 全局快捷键 | 3天 | X11+Wayland 双协议 |
| 快速录入窗口 UI | 3天 | 文字/语音/截图三种输入 |
| 系统托盘常驻 | 2天 | 托盘图标 + 快捷菜单 |
| 开机自启 (XDG autostart) | 1天 | 设置选项 |
| 全局搜索 (笔记+待办) | 2天 | 全文搜索 |
| 深色/浅色主题跟随 | 1天 | DTK 自动跟随 |

### Phase 3：AI 文本处理（2周）

**目标**：AI 辅助编辑、摘要、待办提取

| 任务 | 估算 | 产出 |
|------|------|------|
| AI 服务抽象层 (多引擎支持) | 3天 | DeepSeek/通义千问/OpenAI 兼容 |
| 笔记 AI 润色/扩写/翻译 | 3天 | 选中文本 AI 操作 |
| 待办 AI 提取 | 2天 | 从文本自动提取待办 |
| AI 智能摘要 | 2天 | 长文本自动摘要 |
| Prompt 模板管理 | 1天 | 可配置模板 |
| API Key 配置界面 | 1天 | 设置页面 |

### Phase 4：会议助手（4周）— 核心差异化

**目标**：录音+转写+AI 摘要一体化

| 任务 | 估算 | 产出 |
|------|------|------|
| 录音控制 UI (开始/暂停/继续/结束) | 3天 | 录音控件 + 波形 |
| 集成 Whisper.cpp 离线转写 | 2周 | 实时转写（中文优化） |
| 转写结果实时展示 | 3天 | 流式文本展示 |
| 会议记录管理 | 2天 | 历史列表 + 搜索 |
| AI 会议摘要生成 | 3天 | 结构化会议纪要 |
| 待办从会议提取 | 2天 | 自动识别待办事项 |
| 会议录音导出 | 1天 | MP3/文本/PDF 导出 |

### Phase 5：智能周报+发布准备（2周）

**目标**：周报生成 + 商店发布

| 任务 | 估算 | 产出 |
|------|------|------|
| 自然周视图 | 2天 | 周视图 UI |
| 待办完成统计 | 2天 | 完成率/趋势图 |
| AI 周报生成 | 3天 | 摘要 + 数据 + AI 润色 |
| 周报导出 (PDF/DOCX) | 2天 | 格式化导出 |
| UOS 商店上架 | 2天 | 应用截图/描述/审核 |
| 玲珑商店上架 | 2天 | 玲珑包构建/测试/发布 |
| 用户手册 + 帮助文档 | 2天 | 使用指南 + 在线帮助 |

---

## 3. 项目结构

```
uos-voice-note/
├── CMakeLists.txt                 # 顶层 CMake 配置
├── linglong.yaml                  # 玲珑打包配置
├── debian/                        # deb 打包配置
│   ├── changelog
│   ├── control
│   ├── rules
│   └── install
├── assets/                        # 资源文件
│   ├── icons/                     # 应用图标
│   ├── desktop/                   # .desktop 文件
│   ├── translations/              # 翻译文件 (.ts/.qm)
│   └── web/                       # 富文本编辑器 Web 资源
├── src/
│   ├── main.cpp                   # 入口
│   ├── application/               # 应用层
│   │   ├── app.h/cpp
│   │   └── settings.h/cpp
│   ├── ui/                        # UI 层
│   │   ├── mainwindow/            # 主窗口
│   │   ├── editor/                # 笔记编辑器
│   │   ├── quickentry/            # 快速录入窗口
│   │   ├── meeting/               # 会议助手
│   │   ├── todo/                  # 待办看板
│   │   ├── weekly/                # 周报视图
│   │   └── tray/                  # 系统托盘
│   ├── core/                      # 核心业务逻辑
│   │   ├── notemanager.h/cpp
│   │   ├── todomanager.h/cpp
│   │   ├── meetingmanager.h/cpp
│   │   ├── tagmanager.h/cpp
│   │   └── searchmanager.h/cpp
│   ├── services/                  # 服务层
│   │   ├── aiservice.h/cpp        # AI 服务抽象
│   │   ├── asrservice.h/cpp       # 语音识别服务
│   │   ├── ttservice.h/cpp        # 语音合成服务
│   │   └── hotkeyservice.h/cpp    # 全局快捷键
│   ├── storage/                   # 数据存储
│   │   ├── database.h/cpp
│   │   └── models/                # 数据模型
│   └── audio/                     # 音频处理
│       ├── recorder.h/cpp         # 录音器
│       ├── player.h/cpp           # 播放器
│       └── converter.h/cpp        # 格式转换
├── tests/                         # 单元测试
│   ├── CMakeLists.txt
│   └── ...
├── docs/                          # 文档
│   ├── product-spec.md
│   ├── dev-plan.md
│   └── user-guide.md
└── packaging/                     # 打包脚本
    ├── build-deb.sh
    └── build-linglong.sh
```

---

## 4. 关键依赖

### 4.1 系统依赖

```
# 基础
qt6-base-dev                     # Qt6 核心
qt6-declarative-dev              # QML 支持
libdtk6widget-dev                # DTK6 控件
libdtk6core-dev                  # DTK6 核心
libdtk6gui-dev                   # DTK6 图形
libdtk6log-dev                   # DTK6 日志
libsqlite3-dev                   # SQLite

# 音频
libgstreamer1.0-dev              # GStreamer 音频
libgstreamer-plugins-base1.0-dev
libvlc-dev                       # VLC 播放引擎

# 打包
debhelper
cmake
pkg-config
```

### 4.2 可选依赖

```
# Whisper.cpp 离线 ASR（Phase 4）
whisper.cpp                      # 本地语音识别

# 系统托盘
libqt6svg6-dev                   # SVG 图标支持

# 全局快捷键
extra-cmake-modules              # KF6GlobalAccel (Wayland)
```

### 4.3 外部服务

| 服务 | 用途 | 免费额度 |
|------|------|---------|
| DeepSeek API | AI 摘要/润色/待办提取 | 注册赠送 500 万 tokens |
| 通义千问 API | AI 备选引擎 | 百万 token 免费 |
| 百度 ASR API | 云端高精度语音识别 | 新用户免费额度 |
| 讯飞 ASR API | 云端高精度语音识别 | 新用户免费额度 |

---

## 5. 里程碑与交付标准

| 里程碑 | 时间 | 交付物 | 验收标准 |
|--------|------|--------|---------|
| M0: 项目初始化 | 第1周 | 工程骨架 + 音频模块 | 可编译运行，录音/播放功能正常 |
| M1: MVP 发布 | 第4周 | 笔记+待办核心功能 | 笔记 CRUD、待办管理、标签分类、搜索 |
| M2: 快速体验 | 第6周 | 全局快捷键+系统托盘 | Alt+Space 快速录入，托盘常驻 |
| M3: AI 赋能 | 第8周 | AI 摘要+润色+待办提取 | AI 功能可用，可配置 API Key |
| M4: 会议助手 | 第12周 | 录音+转写+AI 摘要 | 完整会议记录流程 |
| M5: 正式发布 | 第14周 | 周报+商店上架 | UOS 商店 + 玲珑商店可下载 |

---

## 6. 风险与应对

| 风险 | 影响 | 概率 | 应对方案 |
|------|------|------|---------|
| Whisper.cpp 中文精度不足 | 高 | 中 | 备选百度/讯飞云端 ASR |
| Wayland 全局快捷键兼容 | 中 | 高 | 提前调研，使用 portal D-Bus API |
| Qt6 与 DTK6 兼容问题 | 中 | 低 | 参考 deepin 官方应用实现 |
| 玲珑包依赖过大 | 中 | 中 | 按模块拆分，精简运行时依赖 |
| AI 服务 API 变更 | 低 | 高 | 抽象层隔离，多引擎支持 |
| 离线功能受限 | 中 | 低 | 优先保证离线核心功能可用 |

---

## 7. 发布渠道

| 渠道 | 包格式 | 审核周期 | 备注 |
|------|--------|---------|------|
| UOS 应用商店 | deb | 1-2周 | 主渠道，需统信开发者认证 |
| 玲珑商店 | .uab | 1周 | 如意玲珑格式，沙箱运行 |
| GitHub Releases | deb + AppImage | 即时 | 开源社区分发 |
| 统信官网 | deb | 随商店 | 官网同步更新 |
---

# v1.1 迭代开发计划

> Updated: 2026-08-05 | Status: Active(基线 Draft)
> 依据: 2026-08-04 梳理检查报告、IDE-163 todo-refactor-plan.md、docs/remaining-work.md

## Current Codebase(现状基线)

- v1.0.0 已发布(2026-07-27);桌面模式三栏布局 + 紧凑便签已落地(IDE-162/172/174);待办多标签 Phase A/B 已落地(IDE-163, commit 62dedb9);IDE-177/178/180 UI 修复已完成
- 验证基线: 全新构建通过(build-check),单测 13/13 通过
- 待收口: 目录重命名后未入库;本地分支 ahead 3 / behind 2;在线 ASR(百度)未开通;旧 build 目录全部失效

## Target Architecture

- 保持 Qt6 + DTK6 + SQLite + GStreamer + Whisper 现有技术栈,不做重构性替换
- 周报日程联动: 基于现有 WeeklyReportWidget + todo_tags 多标签数据模型
- 导出统一收敛到 ExportService;主题统一 DPalette;i18n 用 Qt 翻译机制(ts/qm)

## Risks and Assumptions

1. 目录未入库 → 任何改动前必须先完成 Phase 0 收口并备份当前目录
2. 分支分叉合并有冲突风险(sidebarwidget.cpp 已被两端修改)→ 合并时逐文件核对
3. 在线 ASR(百度短语音)依赖外部账号开通,不阻塞离线功能
4. IDE-163 Phase C/D 依赖 todo_tags 数据(已就绪)
5. deb/玲珑发布需商店审核周期,发布任务以材料准备为准

## Delivery Phases

- Phase 0 — 仓库收口与基线(P0, 最先做)
- Phase 1 — 看板收尾与质量验证(P1)
- Phase 2 — 文档与工程规范(P2)
- Phase 3 — IDE-163 Phase C/D: 待办日程化(功能主线)
- Phase 4 — v1.1 体验增强(按 remaining-work.md 优先级)
- Phase 5 — v1.1 发布

## Task Table

| ID | 任务 | 需求关联 | 复杂度 | 依赖 | 验证 | 状态 |
|----|------|---------|--------|------|------|------|
| P0-T1 | 目录名确认并入库(统一规范名, add 新路径, 清理旧路径跟踪) | 梳理报告 P0-1 | S | - | git status 干净, git log 可见新目录提交 |Done|
| P0-T2 | 合并分叉: rebase 到 origin(拉取 ffbbece/3c95c5a), 解决 sidebarwidget 冲突并推送; 打 v1.0.0 tag | 梳理报告 P0-2 | M | P0-T1 | ahead=0 behind=0; git tag 含 v1.0.0 |Done|
| P0-T3 | 清理残留(.bak/bak2/todowidget_clean.cpp/new_ui.py)与失效 build 目录 | 梳理报告 P2-9 | S | P0-T1 | 无 .bak; 干净目录可构建 |Done|
| P1-T1 | IDE-178 置 done(开发/审查/验收均完成) | IDE-178 | S | - | issue status=done |Done|
| P1-T2 | IDE-169 功能验证测试: 36 项 GUI 用例逐项执行并出报告; 修复单测 QWARN | IDE-169; 梳理报告 P2-7 | M | P0-T2 | test-report 全勾选; ctest 无 QWARN |In Progress|
| P1-T3 | 例行任务收口: IDE-175 每日代码审查 done; IDE-167/168 按流程推进 | - | S | - | 看板状态更新 |Done|
| P2-T1 | 生成 docs/project-context.md(sdlc-context 8 节) | 梳理报告 P2-6 | S | P0-T2 | 文件存在且与 git 状态一致 |Done|
| P2-T2 | dev-plan.md 更新入库 + remaining-work.md 状态刷新 | 本计划 | S | P2-T1 | 文档与代码一致 |Done|
| P2-T3 | 修复 AsrService unused-parameter 编译警告 | 梳理报告验证项 | S | P0-T2 | 构建 0 警告 |Done|
| P3-T1 | 周报日程联动(Phase C): 周视图每日待办展示/点击详情/按标签统计/AI 周报标签维度 | IDE-163 C; spec §2.2 周报 | L | todo_tags 已就绪 | 周报页显示每日待办+标签完成率, AI 周报含标签维度; 新增单测 |Done|
| P3-T2 | 待办 UI 对齐(Phase D): 日历网格/拖拽改期/标签徽章/简单卡片 | IDE-163 D; remaining #10 | L | P3-T1 | 待办按日期分布, 拖拽改期生效 |Done|
| P4-T1 | 紧凑模式贴边自动隐藏 + 悬停重现 | remaining #5 | M | - | 拖到屏幕边缘自动隐藏 |Done|
| P4-T2 | 多紧凑窗口(独立保存) | remaining #6 | L | P4-T1 | 多便签并存互不干扰 |Done|
| P4-T3 | 全局快捷键自定义(设置页) | remaining #7 | M | - | 改键生效 |Done|
| P4-T4 | 开机自启紧凑模式 | remaining #8 | S | P4-T1 | 启动直接出紧凑窗 |Done|
| P4-T5 | 深色主题完整适配(硬编码→DPalette) | remaining #10 | M | - | 深色下无白块/硬编码色 |Done|
| P4-T6 | 导出统一走 ExportService(笔记/会议/周报) | remaining #11 | M | - | 导出路径单一 |Done|
| P4-T7 | 回收站增强(批量恢复/永久删除/搜索) | remaining #12 | M | - | 批量操作+搜索可用 |Done|
| P4-T8 | 编辑器撤销/重做 + 自动保存 + 字数统计 | remaining #19/20/21 | M | - | Ctrl+Z/Y, 自动保存, 字数显示 |Done|
| P4-T9 | 国际化(ts/qm, 英文界面) | remaining #25 | L | - | 英文界面可切换 | Done |
| P5-T1 | v1.1 回归(单测+GUI) + deb/玲珑打包 | IDE-109/112 延续 | M | P3, P4 | ctest 全绿, deb/玲珑可安装 |Done|
| P5-T2 | Release Notes v1.1.0 + tag + 商店/Release 发布 | IDE-93 延续 | S | P5-T1 | 发布材料齐, tag 存在 |Done|

## Validation Strategy

- Phase 0: 操作前 `cp -a` 备份整个目录; 合并后核对 sidebarwidget.cpp / meetingwidget.cpp 与 origin 一致
- Phase 3(风险最高): 数据迁移先行(旧 tag 字符串 → todo_tags), 新增 storage 层单测覆盖多标签统计; UI 用手工 GUI 用例
- 每个功能任务完成后: 构建 0 警告 + ctest 全绿 + 对应 GUI 用例勾选

## Change Log

- 2026-08-05: 创建 v1.1 计划(基于 2026-08-04 梳理检查报告)

## Task → Multica Issue 映射

> 父任务: IDE-181 UOS速记 v1.1 开发计划执行(Phase 0-5)
> 建立日期: 2026-08-05

| 任务 | Issue | 阶段 |
|------|-------|------|
| P0-T1 | IDE-182 |Done|
| P0-T2 | IDE-183 |Done|
| P0-T3 | IDE-184 |Done|
| P1-T1 | IDE-185 |Done|
| P1-T2 | IDE-186 |In Progress|
| P1-T3 | IDE-187 |Done|
| P2-T1 | IDE-188 |Done|
| P2-T2 | IDE-189 |Done|
| P2-T3 | IDE-190 |Done|
| P3-T1 | IDE-191 |Done|
| P3-T2 | IDE-192 |Done|
| P4-T1 | IDE-193 |Done|
| P4-T2 | IDE-194 |Done|
| P4-T3 | IDE-195 |Done|
| P4-T4 | IDE-196 |Done|
| P4-T5 | IDE-197 |Done|
| P4-T6 | IDE-198 |Done|
| P4-T7 | IDE-199 |Done|
| P4-T8 | IDE-200 |Done|
| P4-T9 | IDE-201 |Done|
| P5-T1 | IDE-202 |Done|
| P5-T2 | IDE-203 |Done|

---

## 8. v1.2 规划（v1.1 收口后候选，Phase 6）

> 更新日期：2026-08-06
> 来源：docs/remaining-work.md 待开发 5 项（#15/#18/#22/#23/#24）
> 看板：父任务 IDE-209，子任务 IDE-210~214（stage 7）

| 任务 | Issue | 功能 | 需求关联 | 优先级 | 复杂度 | 状态 |
|------|-------|------|----------|--------|--------|------|
| V2-T1 | IDE-210 | 说话人识别（转写区分发言人，会议纪要按发言人组织） | remaining #15 | medium | L | ✅ Done（commit 644c7ee：Whisper 说话人聚类 + 会议页标签 + AI 纪要按发言人组织） |
| V2-T2 | IDE-211 | 启动速度优化（懒加载非必要模块） | remaining #18 | low | M | ✅ Done（commit 644c7ee：会议/周报页懒加载，首窗 2210ms→1522ms，-31%） |
| V2-T3 | IDE-212 | 标签颜色自定义（设置页 UI，数据模型已支持颜色字段） | remaining #22 | low | M | ✅ Done（commit 644c7ee：设置页标签选色 + 侧栏/列表/卡片按色显示） |
| V2-T4 | IDE-213 | 周报模板自定义（Markdown 模板 + 占位符） | remaining #23 | low | M | ✅ Done（commit 644c7ee：设置页模板编辑 + 8 类占位符） |
| V2-T5 | IDE-214 | 数据备份/恢复（一键 SQLite + 校验） | remaining #24 | high | M | ✅ Done（commit 644c7ee：BackupService + 设置页 + 单测） |

> 2026-08-06 状态：5 项全部实现并推送（单测 18/18 通过）。待 v1.2 Release Notes 与版本 tag 收口。

**验收总入口**：5 个子任务全部 done + v1.2 Release Notes/tag 就绪（沿用 v1.1 收口流程）。
