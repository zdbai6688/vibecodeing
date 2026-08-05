// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GLOBALDEF_H
#define GLOBALDEF_H

// 应用信息
#define APP_NAME "UOS速记"
#define APP_ID "org.deepin.uos-shorthand"
// APP_VERSION 由 CMakeLists.txt 传递

// 数据库
#define DB_NAME "uos-shorthand.db"
#define DB_CONNECTION_NAME "main_connection"

// 快捷键 — 规格书定义 Alt+Space（全局热键）
#define SHORTCUT_QUICK_ENTRY "Alt+Space"
#define SHORTCUT_NEW_NOTE "Ctrl+N"
#define SHORTCUT_SEARCH "Ctrl+F"
#define SHORTCUT_SAVE "Ctrl+S"

// 默认值
#define DEFAULT_TAG_COLOR "#1890FF"
#define MAX_TITLE_LENGTH 256
#define MAX_CONTENT_LENGTH 1048576

#endif // GLOBALDEF_H
