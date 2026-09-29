/*
 * Jenkinsfile - SampleApp (Qt 5.15.2 + MSVC 2022) build + artifact publishing.
 *
 * Stages
 *   1. Checkout ............ the source branch (BRANCH parameter)
 *   2. Build + stage ....... ci/build.ps1 -> qmake, nmake, windeployqt, zip
 *   3. Verify artifacts .... payload exists, sha256 matches BUILD_INFO.json
 *   4. Archive ............. ci-out/** as Jenkins artifacts (build log page)
 *   5. Publish ............. ci/publish_artifacts.ps1 -> applicationbackup
 *
 * Requirements on the Windows agent (label below):
 *   - Git, Java (agent remoting), Qt 5.15.2 msvc2019_64, VS 2022 build tools
 *   - Jenkins credential "github-pat" (Secret text) with repo push rights
 *   - "Timestamper" + "Credentials Binding" plugins
 */
pipeline {
    agent { label 'windows && qt-5.15.2' }

    options {
        timestamps()
        disableConcurrentBuilds()
        buildDiscarder(logRotator(numToKeepStr: '20', artifactNumToKeepStr: '5'))
        skipDefaultCheckout(true)
    }

    parameters {
        string(name: 'BRANCH', defaultValue: '1.0',
               description: 'Source branch to build (1.0, 1.2, ...)')
        booleanParam(name: 'PUBLISH', defaultValue: false,
               description: 'Push the build to the applicationbackup artifact branch')
        string(name: 'ARTIFACT_BRANCH', defaultValue: 'applicationbackup',
               description: 'Target branch for published artifacts')
        string(name: 'KEEP_VERSIONS', defaultValue: '0',
               description: 'Keep only the newest N version folders on the artifact branch (0 = keep all)')
        string(name: 'QT_DIR', defaultValue: 'C:\\Qt\\5.15.2\\msvc2019_64',
               description: 'Qt installation used for qmake/windeployqt')
        string(name: 'VCVARS', defaultValue: 'C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat',
               description: 'MSVC environment script (vcvars64.bat)')
    }

    environment {
        OUT_DIR = 'ci-out'
    }

    stages {
        stage('Checkout') {
            steps {
                checkout scm
                withCredentials([string(credentialsId: 'github-pat', variable: 'GH_TOKEN')]) {
                    powershell '''
                        $ErrorActionPreference = "Stop"
                        $url = (git remote get-url origin).Trim()
                        if ($url -match '^https://') { $url = $url -replace '^https://', "https://x-access-token:$env:GH_TOKEN@" }
                        git fetch --quiet --depth 1 $url $env:BRANCH
                        if ($LASTEXITCODE -ne 0) { throw "fetch of branch $env:BRANCH failed" }
                        git checkout --quiet -B $env:BRANCH FETCH_HEAD
                        git log -1 --stat
                    '''
                }
            }
        }

        stage('Build + stage') {
            steps {
                powershell '''
                    $ErrorActionPreference = "Stop"
                    & "$env:WORKSPACE\\ci\\build.ps1" -OutDir $env:OUT_DIR -QtDir $env:QT_DIR -VcVars $env:VCVARS
                    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
                '''
            }
        }

        stage('Verify artifacts') {
            steps {
                powershell '''
                    $ErrorActionPreference = "Stop"
                    $stage = Get-ChildItem "$env:WORKSPACE\\$env:OUT_DIR" -Directory | Select-Object -First 1
                    if (-not $stage) { throw "no staged output found" }
                    $info = Get-Content (Join-Path $stage.FullName "BUILD_INFO.json") -Raw | ConvertFrom-Json
                    $payload = Join-Path $stage.FullName $info.payload
                    if (-not (Test-Path $payload)) { throw "payload missing: $payload" }
                    $sha = (Get-FileHash $payload -Algorithm SHA256).Hash.ToLower()
                    if ($sha -ne $info.payloadSha256) { throw "payload sha256 mismatch" }
                    $zip = Join-Path $stage.FullName $info.zip
                    if (-not (Test-Path $zip)) { throw "zip missing: $zip" }
                    $dlls = (Get-ChildItem (Join-Path $stage.FullName "runtime") -Filter *.dll).Count
                    if ($dlls -lt 5) { throw "windeployqt produced too few DLLs ($dlls)" }
                    Write-Host "[ci] OK version=$($info.version) branch=$($info.branch)@$($info.commit) payload=$($info.payloadSize) bytes dlls=$dlls sha256=$sha"
                '''
            }
        }

        stage('Archive') {
            steps {
                archiveArtifacts artifacts: "${OUT_DIR}/**", fingerprint: true, allowEmptyArchive: false
            }
        }

        stage('Publish to artifact branch') {
            when { expression { return params.PUBLISH } }
            steps {
                withCredentials([string(credentialsId: 'github-pat', variable: 'GH_TOKEN')]) {
                    powershell '''
                        $ErrorActionPreference = "Stop"
                        $stage = Get-ChildItem "$env:WORKSPACE\\$env:OUT_DIR" -Directory | Select-Object -First 1
                        $keep = 0
                        [void][int]::TryParse($env:KEEP_VERSIONS, [ref]$keep)
                        & "$env:WORKSPACE\\ci\\publish_artifacts.ps1" -InDir $stage.FullName `
                            -ArtifactBranch $env:ARTIFACT_BRANCH -Token $env:GH_TOKEN -Keep $keep
                        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
                    '''
                }
            }
        }
    }

    post {
        success {
            echo 'SampleApp build finished successfully. Artifacts archived from ci-out/.'
        }
        failure {
            echo 'SampleApp build failed - see the stage log above.'
        }
    }
}
