cmake_minimum_required(VERSION 3.16)
project(UOSShorthand VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ─── 资源文件 ──────────────────────────────────────────────────────
set(RESOURCES resources.qrc)

# ─── 安全编译参数 ──────────────────────────────────────────────────
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fstack-protector-strong -D_FORTIFY_SOURCE=2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie -z relro -z now -z noexecstack")

# ─── 查找 Qt6 和 DTK6 ──────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Sql
    DBus
    Concurrent
    Xml
    Network
)

find_package(Dtk6 REQUIRED COMPONENTS
    Core
    Gui
    Widget
)

# ─── X11 ────────────────────────────────────────────────────────────
find_package(X11 REQUIRED)

# ─── 源文件 ─────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    # 应用层
    src/application/shorthandapplication.cpp
    # 存储层
    src/storage/database.cpp
    src/storage/notestorage.cpp
    src/storage/todostorage.cpp
    src/storage/tagstorage.cpp
    src/storage/meetingstorage.cpp
    # 核心业务
    src/core/notemanager.cpp
    src/core/todomanager.cpp
    src/core/tagmanager.cpp
    src/core/meetingmanager.cpp
    # UI - 主窗口
    src/ui/mainwindow/mainwindow.cpp
    src/ui/mainwindow/sidebarwidget.cpp
    src/ui/mainwindow/notelistwidget.cpp
    # UI - 编辑器
    src/ui/editor/noteeditorwidget.cpp
    src/ui/editor/markdownhighlighter.cpp
    # UI - 快速录入
    src/ui/quickentry/quickentrydialog.cpp
    # UI - 桌面模式
    src/ui/desktop/stickynotecard.cpp
    src/ui/desktop/desktopmodemanager.cpp
    # UI - 设置
    src/ui/settings/settingsdialog.cpp
    # UI - 待办看板
    src/ui/todo/todowidget.cpp
    # UI - 会议
    src/ui/meeting/meetingwidget.cpp
    # UI - 周报
    src/ui/weekly/weeklyreportwidget.cpp
    # UI - 系统托盘
    src/ui/tray/traymanager.cpp
    # 音频处理
    src/audio/audiorecorder.cpp
    src/audio/audioplayer.cpp
    # 服务层
    src/services/migration.cpp
    src/services/aiservice.cpp
    src/services/asrservice.cpp
    src/services/exportservice.cpp
    src/services/screenshotmanager.cpp
    src/services/globalshortcutmanager.cpp
    # Whisper 离线 ASR
    src/whisper/whisperasrengine.cpp
    src/services/cryptoutil.cpp
    # 孤立源文件（恢复编译）
    src/ui/meeting/meetingview.cpp
    src/ui/settings/settingswidget.cpp
)

set(HEADERS
    src/application/shorthandapplication.h
    src/storage/database.h
    src/storage/notestorage.h
    src/storage/todostorage.h
    src/storage/tagstorage.h
    src/storage/meetingstorage.h
    src/core/notemanager.h
    src/core/todomanager.h
    src/core/tagmanager.h
    src/core/meetingmanager.h
    src/ui/mainwindow/mainwindow.h
    src/ui/mainwindow/sidebarwidget.h
    src/ui/mainwindow/notelistwidget.h
    src/ui/editor/noteeditorwidget.h
    src/ui/editor/markdownhighlighter.h
    src/ui/quickentry/quickentrydialog.h
    src/ui/desktop/stickynotecard.h
    src/ui/desktop/desktopmodemanager.h
    src/ui/meeting/meetingwidget.h
    src/ui/weekly/weeklyreportwidget.h
    src/ui/settings/settingsdialog.h
    src/ui/todo/todowidget.h
    src/ui/tray/traymanager.h
    src/audio/audiorecorder.h
    src/audio/audioplayer.h
    src/services/migration.h
    src/services/aiservice.h
    src/services/asrservice.h
    src/services/exportservice.h
    src/services/screenshotmanager.h
    src/services/globalshortcutmanager.h
    src/whisper/whisperasrengine.h
    src/globaldef.h
    src/services/cryptoutil.h
    src/ui/meeting/meetingview.h
    src/ui/settings/settingswidget.h
)

# ─── 执行文件 ───────────────────────────────────────────────────────
qt_add_executable(uos-shorthand
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# ─── 查找 GStreamer ────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_AUDIO REQUIRED gstreamer-audio-1.0)

# ─── 链接库 ─────────────────────────────────────────────────────────
target_link_libraries(uos-shorthand PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::DBus
    Qt6::Concurrent
    Qt6::Xml
    Qt6::Network
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${CMAKE_SOURCE_DIR}/whisper/libwhisper.so
    ${CMAKE_SOURCE_DIR}/whisper/libggml.so
    ${CMAKE_SOURCE_DIR}/whisper/libggml-cpu.so
    ${CMAKE_SOURCE_DIR}/whisper/libggml-base.so
    ${X11_LIBRARIES}
)

# 运行时搜索 whisper 共享库（避免 libggml.so.0 找不到）
set(CMAKE_BUILD_RPATH "${CMAKE_SOURCE_DIR}/whisper")
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/share/uos-shorthand/whisper")
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
# 使用 RPATH 而非 RUNPATH，确保对传递依赖（libwhisper → libggml）也生效
target_link_options(uos-shorthand PRIVATE "-Wl,--disable-new-dtags")

# ─── 包含目录 ───────────────────────────────────────────────────────
target_include_directories(uos-shorthand PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/application
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/storage
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/ui/todo
    ${CMAKE_SOURCE_DIR}/src/ui/meeting
    ${CMAKE_SOURCE_DIR}/src/ui/weekly
    ${CMAKE_SOURCE_DIR}/src/services
    ${CMAKE_SOURCE_DIR}/src/audio
    ${CMAKE_SOURCE_DIR}/src/whisper
    ${X11_INCLUDE_DIR}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
)

# ─── 编译定义 ───────────────────────────────────────────────────────
target_compile_definitions(uos-shorthand PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_NAME="UOS速记"
)

# ─── 安装 ───────────────────────────────────────────────────────────
install(TARGETS uos-shorthand DESTINATION bin)
install(FILES assets/desktop/uos-shorthand.desktop DESTINATION share/applications)

# 安装图标
install(FILES assets/icons/uos-shorthand.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES assets/icons/uos-shorthand-48.svg DESTINATION share/icons/hicolor/48x48/apps)
install(FILES assets/icons/uos-shorthand-tray.svg DESTINATION share/icons/hicolor/scalable/status)

# 安装 whisper 共享库（运行时依赖）
install(DIRECTORY whisper/ DESTINATION share/uos-shorthand/whisper
        FILES_MATCHING PATTERN "*.so*")
install(FILES whisper/ggml-tiny.bin DESTINATION share/uos-shorthand/whisper)

# 安装讯飞 ASR 脚本（运行时 node 依赖）
install(FILES src/services/xfyun_asr.js DESTINATION share/uos-shorthand/services)

# 安装启动脚本
install(PROGRAMS run.sh DESTINATION bin)

# ─── 测试 ───────────────────────────────────────────────────────────
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()