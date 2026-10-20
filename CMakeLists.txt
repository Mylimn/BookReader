# 电子书阅读器 —— 顶层构建脚本
# 约定见 AGENTS.md：C++17 / Qt6 Widgets / MSVC / CMake >= 3.21

cmake_minimum_required(VERSION 3.21)

project(Reader VERSION 0.1.0 LANGUAGES CXX)

# ---- 全局编译设置 ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 的 Q_OBJECT 类需要 moc 预处理，开启后 CMake 自动处理
set(CMAKE_AUTOMOC ON)

# .qrc 资源（样式表 theme.qss）需经 rcc 编译进可执行文件
set(CMAKE_AUTORCC ON)

# 源码统一从 src/ 下开始包含（如 #include "ui/MainWindow.h"）
list(APPEND CMAKE_INCLUDE_CURRENT_DIR ON)

# ---- 依赖：Qt6 ----
# Core5Compat 用于 TXT 的 GBK 解码（见 TECH_DESIGN 第 1 节）
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Core5Compat)

# ---- 依赖：libzip ----
# EPUB 解压用（见 TECH_DESIGN 第 1 节）；经 vcpkg 引入，configure 时需指定 toolchain 文件
find_package(libzip CONFIG REQUIRED)

# ---- 可执行目标 ----
add_executable(Reader
    src/app/main.cpp

    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/ReaderView.h
    src/ui/ReaderView.cpp

    src/ui/TitleBar.h
    src/ui/TitleBar.cpp

    src/ui/Theme.h
    src/ui/Theme.cpp

    src/document/Book.h
    src/document/Chapter.h
    src/document/IDocumentParser.h

    src/parsers/TxtParser.h
    src/parsers/TxtParser.cpp

    src/parsers/EpubParser.h
    src/parsers/EpubParser.cpp

    src/render/DocumentRenderer.h
    src/render/DocumentRenderer.cpp

    src/storage/Storage.h
    src/storage/Storage.cpp

    src/utils/EncodingDetector.h
    src/utils/EncodingDetector.cpp

    resources/resources.qrc
)

# 让业务代码可以用 "ui/xxx.h" 这种以 src 为根的路径互相包含
target_include_directories(Reader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(Reader PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Core5Compat
    libzip::zip
)
