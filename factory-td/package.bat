@echo off
rem =====================================================================
rem package.bat   factory-td   one-click portable package builder
rem
rem !! ENCODING GUARD - DO NOT EDIT OR REMOVE THE BLOCK BELOW !!
rem   This file is UTF-8 (no BOM). Chinese text in a .bat file makes cmd.exe
rem   mis-parse it (the notorious "is not recognized" desync). So we first run
rem   a pure-ASCII guard that switches the console to code page 65001, then
rem   re-launch this whole file in a child cmd which reads it correctly as
rem   UTF-8 from byte 0. Keep every byte ABOVE the guard pure ASCII.
rem =====================================================================
if not defined __PKG_U8 (
    chcp 65001 >nul
    set "__PKG_U8=1"
    cmd /c ""%~f0" %*"
    exit /b
)
rem =====================================================================
rem package.bat —— factory-td 免安装绿色包一键打包
rem
rem 产出： dist\factory-td-v<版本>-win64.zip
rem        解压后为 exe + 运行库 + assets + 文档，双击 exe 即玩
rem
rem 用法： cd factory-td  &&  package.bat
rem         package.bat nobuild   （跳过自动重编译，直接用现有 build 目录打包）
rem
rem 说明： 若检测到 build 是非通用构建（含 -march=native，只适配本机 CPU），
rem        脚本会自动用 -DFACTORYTD_PORTABLE=ON 重新编译，保证他人电脑可运行。
rem =====================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

rem ---------- 可配置项 ----------
set "BUILD_DIR=build"
set "OUT_ROOT=dist"
rem MinGW 运行时 DLL 来源（须与编译时使用的编译器一致）
set "MINGW_BIN=C:\Program Files\RedPanda-Cpp\mingw64\bin"
set "JOBS=%NUMBER_OF_PROCESSORS%"
if "%JOBS%"=="" set "JOBS=8"
set "SKIP_BUILD=0"
if /i "%~1"=="nobuild" set "SKIP_BUILD=1"

rem ---------- 0. 定位 cmake（PATH 里没有就找常见安装位置） ----------
set "CMAKE_EXE="
for /f "delims=" %%c in ('where cmake 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%c"
if not defined CMAKE_EXE if exist "D:\miniconda\envs\lerobot\Scripts\cmake.exe" set "CMAKE_EXE=D:\miniconda\envs\lerobot\Scripts\cmake.exe"
if not defined CMAKE_EXE if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"

rem ---------- 1. 解析版本号（src\GameConfig.h 里的 vX.Y.Z） ----------
set "VER="
for /f "usebackq delims=" %%v in (`powershell -NoProfile -Command "if ((Get-Content 'src\GameConfig.h' -Raw) -match 'v(\d+\.\d+\.\d+)') { $Matches[1] }"`) do set "VER=%%v"
if "%VER%"=="" (
    echo [错误] 无法从 src\GameConfig.h 解析出 vX.Y.Z 版本号
    pause & exit /b 1
)
set "PKGNAME=factory-td-v%VER%-win64"
set "STAGE=%OUT_ROOT%\%PKGNAME%"
set "ZIP=%OUT_ROOT%\%PKGNAME%.zip"
echo [信息] 版本 %VER%  ^-^>  %ZIP%

rem ---------- 2. 确保存在"可分发"的通用构建 ----------
set "NEED_REBUILD=0"
if not exist "%BUILD_DIR%\factory-td.exe" set "NEED_REBUILD=1"
if exist "%BUILD_DIR%\CMakeFiles\factory-td.dir\flags.make" (
    findstr /c:"march=native" "%BUILD_DIR%\CMakeFiles\factory-td.dir\flags.make" >nul 2>nul
    if !errorlevel!==0 set "NEED_REBUILD=1"
)
if "!NEED_REBUILD!"=="1" (
    if "!SKIP_BUILD!"=="1" (
        echo [警告] 当前 build 含 -march=native 或缺少 exe；你指定了 nobuild，将直接打包。
        echo        这样产出的 zip 在别人电脑上可能闪退，仅建议自用。
    ) else (
        if not defined CMAKE_EXE (
            echo [警告] 未检测到 cmake，无法自动重建通用版。请任选其一：
            echo          a^) 把 cmake 加入 PATH 后重跑 package.bat
            echo          b^) 手动执行：
            echo             cmake -B build -S . -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DFACTORYTD_PORTABLE=ON
            echo             cmake --build build -j %JOBS%
        ) else (
            echo [信息] 检测到非通用构建，正在用 -DFACTORYTD_PORTABLE=ON 重新编译（首次会稍慢）...
            set "PATH=%MINGW_BIN%;!PATH!"
            "!CMAKE_EXE!" -B "%BUILD_DIR%" -S . -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DFACTORYTD_PORTABLE=ON
            if errorlevel 1 (
                echo [错误] 通用版 cmake 配置失败，请检查上方输出
                pause & exit /b 1
            )
            "!CMAKE_EXE!" --build "%BUILD_DIR%" -j %JOBS%
            if errorlevel 1 (
                echo [错误] 通用版编译失败，请检查上方输出
                pause & exit /b 1
            )
            echo [信息] 通用版编译完成。
        )
    )
)

rem ---------- 3. 前置检查 ----------
if not exist "%BUILD_DIR%\factory-td.exe" (
    echo [错误] 未找到 %BUILD_DIR%\factory-td.exe，请先运行 build.bat 完成编译
    pause & exit /b 1
)
if not exist "%BUILD_DIR%\assets\config.json" (
    echo [错误] 未找到 %BUILD_DIR%\assets\config.json，请重新编译（CMake 会把资源拷到 build\）
    pause & exit /b 1
)

rem ---------- 4. 准备暂存目录 ----------
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
if not exist "%STAGE%" (echo [错误] 无法创建 %STAGE% & pause & exit /b 1)
mkdir "%STAGE%\docs"

rem ---------- 5. 主程序 ----------
copy /y "%BUILD_DIR%\factory-td.exe" "%STAGE%\" >nul

rem ---------- 6. SFML 运行库（只取 Release 版，跳过 *-d-*.dll 调试库） ----------
for %%d in (sfml-graphics-2.dll sfml-window-2.dll sfml-system-2.dll sfml-audio-2.dll sfml-network-2.dll openal32.dll) do (
    if exist "%BUILD_DIR%\%%d" (
        copy /y "%BUILD_DIR%\%%d" "%STAGE%\" >nul
    ) else (
        echo [警告] 未找到 %%d
    )
)

rem ---------- 7. MinGW 运行时库（缺了别人机器会报找不到 libstdc++-6.dll） ----------
for %%d in (libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll) do (
    if exist "%MINGW_BIN%\%%d" (
        copy /y "%MINGW_BIN%\%%d" "%STAGE%\" >nul
    ) else (
        echo [警告] 未找到 %%d —— 请检查 MINGW_BIN: %MINGW_BIN%
    )
)

rem ---------- 8. 游戏资源（config.json / 贴图 / 字体） ----------
xcopy /e /i /y "%BUILD_DIR%\assets" "%STAGE%\assets" >nul

rem ---------- 9. 文档：根目录全部 .md + LICENSE ----------
copy /y "..\*.md" "%STAGE%\docs\" >nul
if exist "..\README.md" copy /y "..\README.md" "%STAGE%\README.md" >nul
if exist "..\LICENSE"    copy /y "..\LICENSE"    "%STAGE%\LICENSE" >nul
if exist "..\LICENSE"    copy /y "..\LICENSE"    "%STAGE%\docs\" >nul

rem ---------- 10. 生成运行说明 ----------
> "%STAGE%\运行说明.txt" echo 异星工厂塔防 v%VER%  ——  Windows 64 位免安装版
>>"%STAGE%\运行说明.txt" echo.
>>"%STAGE%\运行说明.txt" echo 1. 把整个文件夹解压到任意目录（路径避免特殊符号）。
>>"%STAGE%\运行说明.txt" echo 2. 双击 factory-td.exe 即开始游戏，无需安装 SFML / 编译器 / 任何运行库。
>>"%STAGE%\运行说明.txt" echo 3. 请保持 exe 与 assets 文件夹的相对位置，不要单独移动 exe。
>>"%STAGE%\运行说明.txt" echo 4. 存档与设置会自动生成在本目录的 saves 文件夹（F5 保存 / F9 读取）。
>>"%STAGE%\运行说明.txt" echo 5. 操作与玩法见 README.md，更新日志见 docs\update.md。

rem ---------- 11. 压缩（优先用系统自带 tar，快且无 2GB 限制） ----------
if exist "%ZIP%" del /q "%ZIP%"
where tar >nul 2>nul
if %errorlevel%==0 (
    tar -a -c -f "%ZIP%" -C "%OUT_ROOT%" "%PKGNAME%"
) else (
    powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%' -DestinationPath '%ZIP%' -Force"
)
if not exist "%ZIP%" (echo [失败] 压缩包未生成 & pause & exit /b 1)

rem ---------- 12. 结果与发布指引 ----------
for %%f in ("%ZIP%") do echo [完成] %%~nxf   %%~zf 字节
echo.
echo   发布到 GitHub Releases：
echo     1^) git tag -a v%VER% -m "Alpha v%VER%"
echo        git push origin v%VER%
echo     2^) 打开 https://github.com/liuyandi386/jyfactorio/releases/new?tag=v%VER%
echo     3^) 把 %ZIP% 拖进 "Attach binaries" 区域，标题填 Alpha v%VER%，点 Publish release
echo     4^) 发布后永久下载地址：
echo        https://github.com/liuyandi386/jyfactorio/releases/download/v%VER%/%PKGNAME%.zip
echo.
pause
