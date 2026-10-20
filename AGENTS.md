# AGENTS.md

本文件是本仓库的**协作约定**，供人与 AI 代理共同遵守。开始任何改动前先读这里。

## 1. 这个项目是什么

一个 **Windows 桌面电子书阅读器**（C++ + Qt 6），定位是轻量、专注"阅读"这一件事：打开本地 EPUB / TXT 书籍，舒适地读完，并在下次打开时自动回到上次的位置。

- **不做什么**：书库管理平台、多格式（PDF/MOBI/AZW3）、划线批注、云同步、查词翻译。
- **当前阶段**：MVP 的 **P0 六项与 PRD 第 5 节共享规则（TXT 编码识别）已全部完成**：里程碑 ①（TXT 最小闭环）、②（字号 / 主题 / 进度保存恢复）、③（EPUB 解压解析 + 目录导航）、④（TXT 编码探测）以及界面深度定制均落地并通过验证。P1（最近书列表、单书签）明确留作**迭代二**，见 PRD 第 2 节。
- **核心闭环**：打开书籍 → 解析渲染 → 目录/翻页导航 → 字号与主题调节 → 进度自动保存与恢复。

### 文档索引

| 文档 | 内容 |
|---|---|
| [PRD.md](./PRD.md) | 产品需求：范围、功能需求、共享规则与待定项 |
| [TECH_DESIGN.md](./TECH_DESIGN.md) | 技术设计：模块划分、关键决策、存储设计、里程碑 |
| AGENTS.md | 本文件：协作与工程约定 |

需求以 PRD 为准，实现方式以技术设计为准；两者冲突时先在文档里对齐，再动代码。

## 2. 怎么跑起来

**前置依赖**

| 依赖 | 说明 |
|---|---|
| Qt 6.5+ | 需包含 `Widgets`、`Gui`、`Core`、`Core5Compat` 组件 |
| CMake | ≥ 3.21 |
| MSVC | VS 2022（Windows 构建链） |
| libzip | ≥ 1.10（EPUB 解压用，建议经 vcpkg 引入；其依赖 zlib） |

**构建**

```bash
# 配置（Qt 路径与 vcpkg 路径按本机实际调整）
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
      -DCMAKE_PREFIX_PATH=<Qt6 安装路径> ^
      -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake

# 编译（多配置生成器，需指定 --config）
cmake --build build --config Debug
```

libzip 经 vcpkg 引入（三元组用动态的 `x64-windows`，与 Qt 的 `/MD` 一致）。依赖 DLL 由 vcpkg 的 toolchain 在编译后自动部署到可执行文件目录（Debug 配置下为 `zip.dll`、`zd.dll`、`bz2d.dll`），无需手工复制。

**运行**

```bash
./build/Debug/Reader.exe   # 若 Qt 未加入 PATH，需先把 <Qt6>/bin 加入 PATH
```

**约定**：任何提交前，工程必须能在上述流程下**编译通过并启动**。

## 3. 硬性约定

### 3.1 技术栈（不可擅自替换）

- 语言：**C++17**；GUI：**Qt 6 Widgets**（不用 QML）。
- 构建：**CMake**；编译器：**MSVC**。
- EPUB 解压：**libzip**；XML 解析：**QXmlStreamReader**（不引入额外 XML 库）。
- 正文渲染：**QTextDocument / QTextBrowser**（不引入 Qt WebEngine）。
- 持久化：**QJsonDocument** 写本地 JSON，存于 `QStandardPaths::AppDataLocation`（不写死在工程目录）。
- 外部依赖只允许 **Qt + libzip(+zlib)**。新增第三方库前必须先更新本文件并说明理由。

### 3.2 目录结构

```text
Reader/
├── CMakeLists.txt
├── PRD.md / TECH_DESIGN.md / AGENTS.md
├── src/
│   ├── app/            # main.cpp、MainWindow
│   ├── ui/             # ReaderView、ToolBar、设置面板
│   ├── document/       # Book.h、Chapter.h、IDocumentParser.h
│   ├── parsers/        # EpubParser、TxtParser
│   ├── render/         # DocumentRenderer
│   ├── storage/        # Storage（进度/偏好/最近书）
│   └── utils/          # EncodingDetector
├── third_party/        # libzip（或经包管理器引入）
└── resources/          # 图标、样式表
```

- 新文件必须落在对应模块目录内，不得在根目录堆放源码。
- **核心层（`document/`、`parsers/`、`storage/`、`utils/`）不得依赖 `ui/` 或任何 `QWidget`**：UI 与业务逻辑必须分离。

### 3.3 代码风格

- **缩进**：4 个空格，禁用 Tab；单行不超过 **100** 字符。
- **花括号**：函数与类的左花括号另起一行（Qt 风格）。
- **命名**：
  - 类 / 结构体：`PascalCase`（`EpubParser`）
  - 函数 / 局部变量 / 参数：`camelCase`
  - 成员变量：`m_camelCase` 前缀
  - 常量：`kPascalCase`；宏：`UPPER_SNAKE_CASE`
- **头文件**：`.h` / `.cpp` 配对；一律用 `#pragma once`。
- **命名空间**：统一顶层命名空间 `reader`，按模块分层（如 `reader::parsers`）。
- **Qt 用法**：
  - 对象通信一律用**信号槽**，不用回调指针。
  - 面向用户的字符串用 `tr()` 包裹，为将来翻译留口子。
  - 内存管理优先用父对象（`QObject` 树）或智能指针，避免裸 `new`/`delete`。
- **头文件包含顺序**：自身头文件 → Qt 头文件 → 标准库 → 项目内其他模块。
- **语言**：注释、文档、提交信息用**中文**；标识符（类/函数/变量）用**英文**。

### 3.4 其他约定

- **范围纪律**：只实现 PRD 中 P0/P1 的功能，不提前实现"明确不做"清单里的东西。
- **已验证的事实**：新增技术选型前先在 `TECH_DESIGN.md` 记录，再落到代码。
- **每步可运行**：按技术设计第 8 节的里程碑推进，每一步都应是可独立编译、可独立验证的增量。
- **已定稿的产品决策**：正文形态 = **分页**；字号默认 **18px**（范围 12–32、步进 2）；本版本**不显示**页码 / 百分比进度。PRD 第 6 节的 `[TBD]` 已全部定稿，无遗留待定项。
