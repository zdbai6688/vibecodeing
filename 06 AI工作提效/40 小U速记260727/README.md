# UOS速记

统信 UOS 原生的办公信息中枢 — 笔记、待办、会议速记、AI 摘要四合一工具。

## 功能特性

- 📝 **笔记管理**：Markdown/富文本双模编辑，标签分类，快速搜索，文件夹管理，回收站
- ✅ **待办管理**：截止日期、优先级、完成状态，今日/本周/逾期自动分组看板
- ⚡ **快速捕捉**：Alt+Shift+S 全局快捷键，敬业签风格紧凑窗口，随时随地记录灵感
- 🎤 **会议助手**：录音 + 实时转写（离线 Whisper / 在线百度/讯飞/阿里云），说话人识别，时间戳跳转，AI 会议纪要
- 🤖 **AI 助手**：文本润色、扩写、翻译、摘要、待办提取（DeepSeek / 通义千问）
- 📊 **智能周报**：横向日历视图，待办完成统计，AI 辅助生成周报
- 📤 **导出生态**：笔记导出 Markdown/TXT/PDF，会议记录导出，批量 ZIP 打包
- 🔒 **隐私优先**：数据 100% 本地存储，离线语音识别，无需注册账号

## 技术栈

| 组件 | 版本 |
|------|------|
| 开发语言 | C++17 |
| UI 框架 | Qt6 + DTK6 |
| 构建系统 | CMake |
| 数据存储 | SQLite 3 (WAL 模式) |
| 录音引擎 | GStreamer 1.0 |
| 离线转写 | Whisper.cpp (CPU 推理) |
| 在线转写 | 百度/讯飞/阿里云 API |
| AI 引擎 | DeepSeek / 通义千问 API |
| 打包格式 | deb + 玲珑包 |

## 构建与运行

```bash
# 依赖
sudo apt install -y qt6-base-dev libdtk6widget-dev libdtk6core-dev libdtk6gui-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libsqlite3-dev

# 构建
/usr/bin/cmake -B build -DCMAKE_BUILD_TYPE=Release
/usr/bin/cmake --build build -j$(nproc)

# 运行
./build/uos-shorthand
```

## 项目结构

```
src/
├── main.cpp                  # 入口
├── application/              # 应用层（全局服务管理）
├── core/                     # 业务逻辑（笔记/待办/标签/会议管理器）
├── storage/                  # 数据存储（SQLite CRUD）
├── ui/
│   ├── mainwindow/           # 三栏主窗口 + 侧边导航 + 笔记列表
│   ├── editor/               # 右侧编辑器 + Markdown 高亮
│   ├── todo/                 # 待办看板
│   ├── meeting/              # 会议助手（录音/转写/时间戳）
│   ├── weekly/               # 周报视图
│   ├── quickentry/           # 快速录入（紧凑/完整双形态）
│   ├── settings/             # 设置弹窗（通用/AI/语音/快捷键）
│   └── tray/                 # 系统托盘
├── services/                 # AI / ASR / 导出 / 迁移 / 截图服务
├── audio/                    # 录音 / 播放器
└── whisper/                  # Whisper 离线 ASR 引擎
```

## 发布

| 渠道 | 格式 | 脚本 |
|------|------|------|
| UOS 应用商店 | deb | `bash packaging/build-deb.sh` |
| 玲珑商店 | 玲珑包 | `bash packaging/build-linglong.sh` |
| GitHub Releases | deb + AppImage | 手动发布 |

## 文档

- [产品需求规格](docs/product-spec.md)
- [开发计划](docs/dev-plan.md)
- [用户手册](docs/user-guide.md)
- [发布说明](docs/RELEASE_NOTES.md)
- [安全审计](docs/security-audit.md)
- [应用商店上架材料](docs/store-listing.md)

## 许可证

GPL-3.0-or-later · Copyright 2026 UnionTech Software Technology Co., Ltd.