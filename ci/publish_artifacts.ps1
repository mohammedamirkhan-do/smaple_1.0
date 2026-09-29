<#
  ci/publish_artifacts.ps1 - publish a staged build (ci/build.ps1 output) to the
  artifact branch (default: applicationbackup) as one folder per version.

  Layout committed on the artifact branch:
    <version>\SampleApp.exe, SampleApp-<version>.exe, *.dll, plugins/, zip
    <version>\BUILD_INFO.json, <version>\version.json   (updater-ready manifest)
    latest\version.json                                 (newest manifest)
    index.json                                          (all versions + sha256)

  The generated version.json is directly usable as the app's update manifest
  (UPDATE_FEED_URL / UPDATE_VERSION_URL_DEFAULT), so CI becomes the update
  server - no GitHub Release upload needed.

  Usage:
    powershell -ExecutionPolicy Bypass -File ci\publish_artifacts.ps1 -InDir ci-out\1.2
    powershell -ExecutionPolicy Bypass -File ci\publish_artifacts.ps1 -InDir ci-out\1.2 -NoPush
    powershell -ExecutionPolicy Bypass -File ci\publish_artifacts.ps1 -InDir ci-out\1.2 -PushUrl "https://x-access-token:$env:TOKEN@github.com/o/r.git"
#>
param(
    [string]$InDir          = "",
    [string]$Version        = "",
    [string]$ArtifactBranch = "applicationbackup",
    [string]$Remote         = "origin",
    [string]$CloneDir       = (Join-Path $env:TEMP "sampleapp-artifact-branch"),
    [string]$PushUrl        = "",
    [string]$Token          = "",
    [string]$Notes          = "",
    [int]$Keep              = 0,
    [switch]$NoPush
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

if (-not $InDir) {
    $candidates = @(Get-ChildItem (Join-Path $repo "ci-out") -Directory -ErrorAction SilentlyContinue)
    if ($candidates.Count -ne 1) { throw "-InDir not given and ci-out does not contain exactly one version folder" }
    $InDir = $candidates[0].FullName
}
if (-not (Test-Path $InDir)) { throw "staged input not found: $InDir" }

$infoFile = Join-Path $InDir "BUILD_INFO.json"
if (-not (Test-Path $infoFile)) { throw "missing BUILD_INFO.json in $InDir (run ci\build.ps1 first)" }
$info = Get-Content $infoFile -Raw | ConvertFrom-Json

if (-not $Version) { $Version = $info.version }
if (-not $Version) { throw "cannot determine version (BUILD_INFO.json has none)" }
$payloadName = if ($info.payload) { $info.payload } else { "SampleApp-$Version.exe" }
if (-not (Test-Path (Join-Path $InDir $payloadName))) { throw "payload not found: $payloadName in $InDir" }

# --- remote URL -> raw.githubusercontent base --------------------------------
$remoteUrl = (git remote get-url $Remote).Trim()
if ($remoteUrl -notmatch 'github\.com[:/](?<owner>[^/]+)/(?<repo>[^/]+?)(\.git)?$') {
    throw "unsupported remote URL: $remoteUrl (expected github.com owner/repo)"
}
$owner = $Matches.owner
$repoName = $Matches.repo
$rawBase = "https://raw.githubusercontent.com/$owner/$repoName/$ArtifactBranch/$Version"
if (-not $PushUrl -and $Token) {
    # Built from a CI secret (Jenkins "string" credential); never stored in git config.
    $PushUrl = "https://x-access-token:$Token@github.com/$owner/$repoName.git"
    Write-Host "[ci] push URL derived from -Token"
}
Write-Host "[ci] remote=$remoteUrl"
Write-Host "[ci] publishing $Version -> branch $ArtifactBranch (raw base $rawBase)"


# --- working clone of the artifact branch ------------------------------------
function Invoke-Git([string[]]$GitArgs, [string]$InDir) {
    & git -C $InDir @GitArgs
    if ($LASTEXITCODE -ne 0) { throw "git $($GitArgs -join ' ') failed (exit $LASTEXITCODE)" }
}

$remoteBranchExists = $false
git ls-remote --exit-code --heads $Remote "refs/heads/$ArtifactBranch" *> $null
if ($LASTEXITCODE -eq 0) { $remoteBranchExists = $true }

if (-not (Test-Path (Join-Path $CloneDir ".git"))) {
    Remove-Item $CloneDir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $CloneDir -Force | Out-Null
    if ($remoteBranchExists) {
        Write-Host "[ci] cloning $ArtifactBranch (depth 1)..."
        git clone --quiet --depth 1 --single-branch --branch $ArtifactBranch $remoteUrl $CloneDir
        if ($LASTEXITCODE -ne 0) { throw "clone of $ArtifactBranch failed" }
    } else {
        Write-Host "[ci] $ArtifactBranch does not exist yet - creating it"
        Invoke-Git @("init", "--quiet") $CloneDir
        Invoke-Git @("remote", "add", "origin", $remoteUrl) $CloneDir
        Invoke-Git @("checkout", "--quiet", "-b", $ArtifactBranch) $CloneDir
    }
} elseif ($remoteBranchExists) {
    Write-Host "[ci] refreshing existing clone in $CloneDir"
    git -C $CloneDir fetch --quiet --depth 1 origin $ArtifactBranch
    if ($LASTEXITCODE -eq 0) { Invoke-Git @("checkout", "--quiet", "-f", "-B", $ArtifactBranch, "FETCH_HEAD") $CloneDir }
} else {
    Invoke-Git @("checkout", "--quiet", "-f", "-B", $ArtifactBranch) $CloneDir
}

# --- copy the staged build ---------------------------------------------------
$dest = Join-Path $CloneDir $Version
Remove-Item $dest -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $dest -Force | Out-Null
Copy-Item (Join-Path $InDir "*") $dest -Recurse -Force

$manifest = [ordered]@{
    version      = $Version
    notes        = if ($Notes) { $Notes } else { "CI build of $($info.branch) @ $($info.commit)" }
    url          = "$rawBase/$payloadName"
    fallback_url = "$rawBase/SampleApp.exe"
}
$manifestJson = $manifest | ConvertTo-Json
$manifestJson | Set-Content (Join-Path $dest "version.json") -Encoding UTF8

$latest = Join-Path $CloneDir "latest"
New-Item -ItemType Directory -Path $latest -Force | Out-Null
$manifestJson | Set-Content (Join-Path $latest "version.json") -Encoding UTF8

# --- index.json + optional pruning -------------------------------------------
function Get-VersionFolders {
    Get-ChildItem $CloneDir -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notin @("latest", ".git") } |
        ForEach-Object {
            $bi = Join-Path $_.FullName "BUILD_INFO.json"
            $meta = if (Test-Path $bi) { Get-Content $bi -Raw | ConvertFrom-Json } else { $null }
            [pscustomobject]@{
                version    = $_.Name
                builtAtUtc = if ($meta) { $meta.builtAtUtc } else { "" }
                branch     = if ($meta) { $meta.branch } else { "" }
                commit     = if ($meta) { $meta.commit } else { "" }
                payload    = if ($meta) { $meta.payload } else { "" }
                size       = if ($meta) { $meta.payloadSize } else { 0 }
                sha256     = if ($meta) { $meta.payloadSha256 } else { "" }
            }
        }
}

$versions = @(Get-VersionFolders | Sort-Object { $_.version })
if ($Keep -gt 0 -and $versions.Count -gt $Keep) {
    $drop = @($versions | Sort-Object builtAtUtc | Select-Object -First ($versions.Count - $Keep))
    foreach ($d in $drop) {
        Write-Host "[ci] pruning old version folder: $($d.version)"
        Remove-Item (Join-Path $CloneDir $d.version) -Recurse -Force -ErrorAction SilentlyContinue
    }
    $versions = @(Get-VersionFolders | Sort-Object { $_.version })
}

[ordered]@{
    artifactBranch = $ArtifactBranch
    updatedAtUtc   = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    latest         = $Version
    versions       = $versions
} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $CloneDir "index.json") -Encoding UTF8

# --- commit + push -----------------------------------------------------------
Invoke-Git @("add", "-f", "-A") $CloneDir
$staged = @(& git -C $CloneDir diff --cached --name-only).Count
if ($staged -eq 0) {
    Write-Host "[ci] nothing changed on $ArtifactBranch - already up to date"
    exit 0
}
$msg = "ci: publish SampleApp $Version ($($info.branch)@$($info.commit))"
Invoke-Git @("-c", "user.name=jenkins-ci", "-c", "user.email=jenkins@localhost", "commit", "--quiet", "-m", $msg) $CloneDir
Write-Host "[ci] committed: $msg ($staged files)"

if ($NoPush) {
    Write-Host "[ci] -NoPush given: local commit only in $CloneDir"
    Invoke-Git @("log", "--oneline", "-1") $CloneDir
    exit 0
}

$pushTarget = if ($PushUrl) { $PushUrl } else { $Remote }
git -C $CloneDir -c credential.helper= push --quiet $pushTarget "HEAD:refs/heads/$ArtifactBranch"
if ($LASTEXITCODE -ne 0) { throw "push to $ArtifactBranch failed" }
Write-Host "[ci] pushed ${ArtifactBranch}: $Version"
Write-Host "[ci] manifest url    = $($manifest.url)"
Write-Host "[ci] manifest mirror = $($manifest.fallback_url)"
