@echo off
REM ============================================================
REM Demo: simulate the update server for branch 1.0 -> 1.2.
REM WHERE #4: copies the freshly built 1.2 exe next to the
REM running 1.0 exe as updates/SampleApp-1.2.exe so the local
REM manifest (updates/version.json) resolves.
REM Usage: DEMO_PREPARE_UPDATE.bat [path-to-1.2-exe]
REM        (default: release\SampleApp-1.2.exe if you renamed it)
REM ============================================================
setlocal
set SRC=%~1
if "%SRC%"=="" set SRC=release\SampleApp-1.2.exe
if not exist "%SRC%" (
  echo [demo] NOT FOUND: %SRC%
  echo [demo] Build branch 1.2 first, then copy it here, e.g.:
  echo   git checkout 1.2 ^&^& build_msvc.bat
  echo   copy release\SampleApp.exe release\SampleApp-1.2.exe
  echo   git checkout 1.0
  echo   DEMO_PREPARE_UPDATE.bat release\SampleApp-1.2.exe
  exit /b 1
)
if not exist updates mkdir updates
copy /Y "%SRC%" updates\SampleApp-1.2.exe >NUL
echo [demo] staged updates\SampleApp-1.2.exe
type updates\version.json
echo [demo] Now run release\SampleApp.exe (v1.0) and wait 30s.
