<#
  ci/build.ps1 - build SampleApp with Qt 5.15.2 + MSVC 2022 and stage a
  deployable artifact set (exe + Qt runtime + zip + metadata).

  Used by the Jenkins pipeline (Jenkinsfile -> stage "Build + Stage") and it
  can be run by hand on any machine that has the same toolchain.

  Usage (from the repository root):
    powershell -ExecutionPolicy Bypass -File ci\build.ps1
    powershell -ExecutionPolicy Bypass -File ci\build.ps1 -OutDir ci-out -Incremental

  Output (default -OutDir ci-out):
    ci-out\<version>\SampleApp.exe                 updater payload (single file)
    ci-out\<version>\SampleApp-<version>.exe       same payload, versioned name
    ci-out\<version>\runtime\...                   exe + Qt5*.dll + plugins
    ci-out\<version>\SampleApp-<version>-win64.zip full runtime package
    ci-out\<version>\BUILD_INFO.json               version, branch, commit, sha256
#>
param(
    [string]$OutDir     = "ci-out",
    [string]$QtDir      = "C:\Qt\5.15.2\msvc2019_64",
    [string]$VcVars     = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
    [switch]$Incremental,
    [switch]$IncludeCompilerRuntime
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo
Write-Host "[ci] repo = $repo"

if (-not (Test-Path $VcVars))          { throw "vcvars64.bat not found: $VcVars" }
if (-not (Test-Path "$QtDir\bin\qmake.exe")) { throw "qmake not found under: $QtDir" }

# --- version from src/version.h (single source of truth) --------------------
$versionHeader = Get-Content (Join-Path $repo "src\version.h") -Raw
if ($versionHeader -notmatch 'APP_VERSION_STR\s+"([^"]+)"') { throw "APP_VERSION_STR not found in src/version.h" }
$version = $Matches[1]

$branch = (git rev-parse --abbrev-ref HEAD).Trim()
$commit = (git rev-parse --short HEAD).Trim()
Write-Host "[ci] branch=$branch version=$version commit=$commit"

# --- build ------------------------------------------------------------------
$buildBat = Join-Path $env:TEMP "sampleapp-ci-build-$PID.bat"
$cleanBlock = if ($Incremental) { "REM incremental build: keeping objects" } else {
@"
if exist Makefile nmake clean >nul 2>&1
if exist release\*.obj del /q release\*.obj >nul 2>&1
if exist release\SampleApp.exe del /q release\SampleApp.exe >nul 2>&1
"@
}
@"
@echo off
call "$VcVars" >nul
if errorlevel 1 exit /b 1
cd /d "$repo"
$cleanBlock
"$QtDir\bin\qmake.exe" SampleApp.pro
if errorlevel 1 exit /b 1
nmake
if errorlevel 1 exit /b 1
exit /b 0
"@ | Set-Content -Path $buildBat -Encoding ASCII

try {
    Write-Host "[ci] qmake + nmake ($(if ($Incremental) { 'incremental' } else { 'clean' }))..."
    cmd /c $buildBat
    if ($LASTEXITCODE -ne 0) { throw "build failed (exit $LASTEXITCODE)" }
} finally {
    Remove-Item $buildBat -Force -ErrorAction SilentlyContinue
}

$builtExe = Join-Path $repo "release\SampleApp.exe"
if (-not (Test-Path $builtExe)) { throw "build produced no exe: $builtExe" }

# --- stage ------------------------------------------------------------------
$stage   = Join-Path $repo (Join-Path $OutDir $version)
$runtime = Join-Path $stage "runtime"
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $runtime -Force | Out-Null

Copy-Item $builtExe (Join-Path $stage "SampleApp.exe") -Force
Copy-Item $builtExe (Join-Path $stage "SampleApp-$version.exe") -Force
Copy-Item $builtExe (Join-Path $runtime "SampleApp.exe") -Force

Write-Host "[ci] windeployqt (Qt DLLs + plugins)..."
$deployArgs = @("--release", "--no-translations")
if (-not $IncludeCompilerRuntime) { $deployArgs += "--no-compiler-runtime" }
& (Join-Path $QtDir "bin\windeployqt.exe") @deployArgs (Join-Path $runtime "SampleApp.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed (exit $LASTEXITCODE)" }

$zip = Join-Path $stage "SampleApp-$version-win64.zip"
Compress-Archive -Path (Join-Path $runtime "*") -DestinationPath $zip -Force

$payload = Get-Item (Join-Path $stage "SampleApp-$version.exe")
$info = [ordered]@{
    app         = "SampleApp"
    version     = $version
    branch      = $branch
    commit      = $commit
    builtAtUtc  = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    payload     = "SampleApp-$version.exe"
    payloadSize = $payload.Length
    payloadSha256 = (Get-FileHash $payload.FullName -Algorithm SHA256).Hash.ToLower()
    zip         = (Split-Path -Leaf $zip)
    zipSize     = (Get-Item $zip).Length
    runtimeDlls = (Get-ChildItem $runtime -Filter "*.dll").Count
}
$info | ConvertTo-Json | Set-Content -Path (Join-Path $stage "BUILD_INFO.json") -Encoding UTF8

Write-Host "[ci] staged -> $stage"
Get-ChildItem $stage | Select-Object Name, Length | Format-Table -AutoSize
Write-Host "[ci] payload sha256 = $($info.payloadSha256)"
