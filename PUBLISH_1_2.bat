@echo off
REM ============================================================
REM Publish 1.2: build branch 1.2, deploy Qt DLLs, create the
REM GitHub Release asset that 1.0 auto-downloads (no clicks).
REM Run from workspace root on branch 1.2:
REM   PUBLISH_1_2.bat
REM What it does:
REM   1) qmake + nmake (release\SampleApp.exe = v1.2)
REM   2) windeployqt -> release\ with Qt5*.dll + platforms\
REM   3) copy to updates\SampleApp-1.2.exe (local test payload)
REM   4) prints gh CLI command to create release v1.2
REM The 1.0 updater pulls:
REM   https://github.com/mohammedamirkhan-do/smaple_1.0/releases/download/v1.2/SampleApp-1.2.exe
REM (see updates/version.json on this branch)
REM ============================================================
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
C:\Qt\5.15.2\msvc2019_64\bin\qmake.exe SampleApp.pro
if errorlevel 1 exit /b 1
nmake
if errorlevel 1 exit /b 1
C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe --release --no-translations release\SampleApp.exe
if not exist updates mkdir updates
copy /Y release\SampleApp.exe updates\SampleApp-1.2.exe >NUL
echo.
echo [publish] built updates\SampleApp-1.2.exe — test locally, then release:
echo   gh release create v1.2 updates/SampleApp-1.2.exe --title "v1.2" --notes "Auto-update target for 1.0"
echo   — or — upload updates\SampleApp-1.2.exe manually at:
echo   https://github.com/mohammedamirkhan-do/smaple_1.0/releases/new  (tag v1.2)
