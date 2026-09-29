@echo off
REM Build the Qt Widgets app with MSVC 2022 + Qt 5.15.2 (msvc2019_64).
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
echo [build] compiler ready
where cl
where nmake
C:\Qt\5.15.2\msvc2019_64\bin\qmake.exe SampleApp.pro
if errorlevel 1 exit /b 1
nmake

