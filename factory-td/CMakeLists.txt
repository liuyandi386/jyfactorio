# =====================================================================
# factory-td —— 《异星工厂》风格自动化塔防游戏 (C++20 + SFML 2.6 + EnTT)
# 由 Python/Pygame 版本重构移植而来
#
# 依赖获取支持三种方式（自动探测）：
#   1. vcpkg manifest：cmake 指定 -DCMAKE_TOOLCHAIN_FILE=vcpkg.cmake
#   2. 系统已安装（如 conda install -c conda-forge cmake sfml entt）
#   3. 都没有 → 打开 FACTORYTD_FETCH_DEPS(默认ON)，配置时自动下载
#      SFML 2.6.1 源码包与 EnTT 源码并一起编译（需网络，首次较慢）
# =====================================================================
cmake_minimum_required(VERSION 3.20)
project(factory-td LANGUAGES CXX)

# ---------- C++20 标准 ----------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------- 依赖自动下载开关 ----------
option(FACTORYTD_FETCH_DEPS "找不到 sfml/entt 时自动下载源码构建" ON)
# 本地预下载依赖目录（离线可用：SFML-2.6.1预编译包 与 entt头文件 解压到这里）
set(FACTORYTD_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps/local2" CACHE PATH "本地第三方依赖目录")

# ---------- SFML 2.6 ----------
find_package(SFML 2.6 COMPONENTS graphics window system QUIET)
if(NOT SFML_FOUND AND EXISTS "${FACTORYTD_DEPS_DIR}/SFML-2.6.1/lib/cmake/SFML/SFMLConfig.cmake")
    # 本地预编译包（含SFMLConfig.cmake，直接复用其导入目标）
    message(STATUS "使用本地预编译SFML: ${FACTORYTD_DEPS_DIR}/SFML-2.6.1")
    set(SFML_DIR "${FACTORYTD_DEPS_DIR}/SFML-2.6.1/lib/cmake/SFML")
    find_package(SFML 2.6 COMPONENTS graphics window system QUIET)
endif()
if(NOT SFML_FOUND)
    # 关掉用不到的模块，减少编译时间与依赖
    set(SFML_BUILD_AUDIO    OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK  OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_DOC      OFF CACHE BOOL "" FORCE)

    if(EXISTS "${FACTORYTD_DEPS_DIR}/SFML-2.6.1/CMakeLists.txt")
        # 本地源码树 → 直接加入构建
        message(STATUS "使用本地SFML源码: ${FACTORYTD_DEPS_DIR}/SFML-2.6.1")
        add_subdirectory("${FACTORYTD_DEPS_DIR}/SFML-2.6.1" "${CMAKE_BINARY_DIR}/sfml-src")
    elseif(FACTORYTD_FETCH_DEPS)
        # 回退：联网自动下载源码
        message(STATUS "未找到已安装的SFML → 自动下载 SFML 2.6.1 源码并编译(首次需数分钟)...")
        include(FetchContent)
        FetchContent_Declare(SFML
            URL https://github.com/SFML/SFML/archive/refs/tags/2.6.1.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(SFML)
    else()
        message(FATAL_ERROR "未找到SFML 2.6。请安装SFML、放入本地依赖目录，或打开 FACTORYTD_FETCH_DEPS 自动下载。")
    endif()
    set(SFML_FOUND TRUE)   # 源码构建后 sfml-graphics/window/system 目标已可用
endif()

# ---------- EnTT ----------
find_package(EnTT QUIET)
if(NOT EnTT_FOUND)
    if(EXISTS "${FACTORYTD_DEPS_DIR}/entt/src/entt/entt.hpp")
        # 本地纯头文件 → 直接创建接口目标
        message(STATUS "使用本地EnTT头文件: ${FACTORYTD_DEPS_DIR}/entt/src")
        add_library(EnTT INTERFACE)
        add_library(EnTT::EnTT ALIAS EnTT)
        target_include_directories(EnTT INTERFACE "${FACTORYTD_DEPS_DIR}/entt/src")
        target_compile_features(EnTT INTERFACE cxx_std_17)
    elseif(FACTORYTD_FETCH_DEPS)
        # 回退：联网自动下载
        message(STATUS "未找到已安装的EnTT → 自动下载 EnTT v3.13.2(纯头文件)...")
        include(FetchContent)
        FetchContent_Declare(EnTT
            URL https://github.com/skypjack/entt/archive/refs/tags/v3.13.2.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(EnTT)   # 提供 EnTT::EnTT 接口目标
    else()
        message(FATAL_ERROR "未找到EnTT。请安装EnTT、放入本地依赖目录，或打开 FACTORYTD_FETCH_DEPS 自动下载。")
    endif()
endif()

find_package(Threads REQUIRED)   # 线程支持

# ---------- 可执行目标 ----------
add_executable(factory-td
    src/main.cpp
    src/Game.cpp
    src/Camera.cpp
    src/AssetManager.cpp
    src/SaveSystem.cpp
    src/ConfigLoader.cpp
    src/Settings.cpp
    src/systems/PipeSystem.cpp
    src/systems/MeSystem.cpp
    src/systems/MachineSystem.cpp
    src/systems/PowerSystem.cpp
    src/systems/TurretSystem.cpp
    src/systems/EnemySystem.cpp
    src/systems/ItemSystem.cpp
    src/systems/PlayerSystem.cpp
    src/systems/RenderSystem.cpp
    src/ui/EntrySystem.cpp
    src/ui/GameUI.cpp
)

target_include_directories(factory-td PRIVATE src)
# nlohmann/json：本地单头优先（deps/local2/nlohmann/json.hpp），其次系统安装
if(EXISTS "${FACTORYTD_DEPS_DIR}/nlohmann/json.hpp")
    target_include_directories(factory-td PRIVATE "${FACTORYTD_DEPS_DIR}")
    message(STATUS "使用本地 nlohmann/json 单头文件")
else()
    find_package(nlohmann_json 3.11 REQUIRED)
    target_link_libraries(factory-td PRIVATE nlohmann_json::nlohmann_json)
endif()
target_link_libraries(factory-td PRIVATE
    sfml-graphics sfml-window sfml-system
    EnTT::EnTT
    Threads::Threads
)

# ---------- std::execution::par 后端 ----------
# MSVC 自带并行算法支持；GCC 需要 TBB。
# 找不到 TBB 时定义 FT_NO_PAR，代码自动退化为串行（见 utils/Profiler.h）。
if(NOT MSVC)
    find_package(TBB QUIET)
    if(TBB_FOUND)
        target_link_libraries(factory-td PRIVATE TBB::tbb)
        message(STATUS "链接 TBB 以支持 std::execution::par")
    else()
        target_compile_definitions(factory-td PRIVATE FT_NO_PAR)
        message(STATUS "未找到 TBB：并行更新退化为串行（不影响正确性）")
    endif()
endif()

# ---------- 优化选项 (O3 / -march=native / LTO) ----------
# FACTORYTD_PORTABLE=ON：不使用本机专属指令集，生成可分发到其它电脑的通用二进制
# （对外发布走 package.bat 时请打开，否则 -march=native 会让 exe 换机器就崩）。
option(FACTORYTD_PORTABLE "生成可分发到其它机器的通用二进制(不用 -march=native)" OFF)

if(MSVC)
    # MSVC: 全程序优化 + UTF-8源码编码
    target_compile_options(factory-td PRIVATE /O2 /GL /utf-8)
    target_link_options(factory-td PRIVATE /LTCG)
    if(NOT FACTORYTD_PORTABLE)
        target_compile_options(factory-td PRIVATE /arch:AVX2)
    else()
        message(STATUS "可分发构建：不启用 /arch:AVX2")
    endif()
else()
    # GCC(含小熊猫C++自带MinGW)/Clang: O3 + 链接时优化
    if(FACTORYTD_PORTABLE)
        target_compile_options(factory-td PRIVATE -O3 -flto)
        message(STATUS "可分发构建：不使用 -march=native（通用 x86-64 指令集）")
    else()
        target_compile_options(factory-td PRIVATE -O3 -march=native -flto)
        message(STATUS "本机构建：启用 -march=native（勿直接对外分发）")
    endif()
    target_link_options(factory-td PRIVATE -flto)
endif()

# ---------- 可选: Tracy性能分析 ----------
option(TRACY_ENABLE "启用Tracy性能打点" OFF)
if(TRACY_ENABLE)
    find_package(Tracy REQUIRED)
    target_link_libraries(factory-td PRIVATE Tracy::TracyClient)
    target_compile_definitions(factory-td PRIVATE TRACY_ENABLE)
    message(STATUS "Tracy打点已启用")
endif()

# ---------- 拷贝资源目录到输出目录 ----------
add_custom_command(TARGET factory-td POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:factory-td>/assets
    COMMENT "拷贝资源文件到输出目录"
)

# ---------- 拷贝SFML动态库(本地预编译包) ----------
if(EXISTS "${FACTORYTD_DEPS_DIR}/SFML-2.6.1/bin")
    add_custom_command(TARGET factory-td POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${FACTORYTD_DEPS_DIR}/SFML-2.6.1/bin"
                $<TARGET_FILE_DIR:factory-td>
        COMMENT "拷贝SFML动态库到输出目录"
    )
endif()
