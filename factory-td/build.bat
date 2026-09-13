@echo off
rem =====================================================================
rem build.bat —— factory-td 一键构建脚本（小熊猫C++ MinGW 工具链，已实测通过）
rem
rem 依赖获取顺序（CMakeLists 自动探测）：
rem   1. deps\local2 本地预下载依赖（SFML预编译包 + EnTT头文件）→ 离线可用
rem   2. 系统已安装的 SFML/EnTT
rem   3. 都没有 → 联网自动下载源码（github 走 hosts 屏蔽时
rem      自动使用本机 Clash 代理 127.0.0.1:7897）
rem =====================================================================
chcp 65001 >nul
cd /d "%~dp0"

rem ---- 1. 编译器加入 PATH（小熊猫C++ 自带 GCC 11.5） ----
set "PATH=C:\Program Files\RedPanda-Cpp\mingw64\bin;%PATH%"
where g++ >nul 2>nul || (echo [错误] 未找到 g++.exe，请检查小熊猫C++安装路径 & pause & exit /b 1)

rem ---- 2. 定位 cmake ----
set "CMAKE_BIN=D:\miniconda\envs\lerobot\Scripts\cmake.exe"
if not exist "%CMAKE_BIN%" (echo [错误] 未找到 cmake.exe: %CMAKE_BIN% & pause & exit /b 1)

rem ---- 3. 检测本机代理(Clash Verge 默认混合端口7897，仅联网下载时用到) ----
set "PROXY_PORT=7897"
netstat -ano | findstr ":%PROXY_PORT% .*LISTENING" >nul 2>nul
if %errorlevel%==0 (
    set "HTTP_PROXY=http://127.0.0.1:%PROXY_PORT%"
    set "HTTPS_PROXY=http://127.0.0.1:%PROXY_PORT%"
    echo [信息] 检测到本机代理 %HTTP_PROXY%，联网下载依赖将走代理
) else (
    echo [信息] 未检测到本地代理（本地依赖存在时无需网络）
)

rem ---- 4. CMake 配置 ----
echo ===== 第1步：CMake 配置 =====
"%CMAKE_BIN%" -B build -S . -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (echo [失败] CMake 配置出错，请把上面输出发给AI排查 & pause & exit /b 1)

rem ---- 5. 编译 ----
echo ===== 第2步：编译（-j8 并行）=====
"%CMAKE_BIN%" --build build -j 8
if errorlevel 1 (echo [失败] 编译出错，请把错误信息发给AI排查 & pause & exit /b 1)

echo.
echo ===== 构建成功！运行 build\factory-td.exe =====
pause
