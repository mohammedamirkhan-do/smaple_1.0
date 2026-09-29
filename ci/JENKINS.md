# Jenkins CI for SampleApp (Qt 5.15.2 + MSVC 2022)

Build the app on every commit and publish the runnable artifacts (exe, Qt DLLs,
plugins, zip) to the long-lived **`applicationbackup`** branch, which doubles as
the update server for the in-app updater.

## Architecture

```text
   commit/push                     Jenkins (Windows agent)
  ┌──────────────┐   trigger   ┌────────────────────────────────────────┐
  │ branch 1.0   │────────────▶│ 1 Checkout   (BRANCH param)            │
  │ branch 1.2   │             │ 2 Build      qmake + nmake             │
  │ (source)     │             │ 3 Deploy     windeployqt               │
  └──────────────┘             │ 4 Stage      ci-out/<version>/...      │
                               │ 5 Verify     sha256 from BUILD_INFO    │
                               │ 6 Archive    jenkins artifacts         │
                               │ 7 Publish    ci/publish_artifacts.ps1  │
                               └───────────────┬────────────────────────┘
                                               │ git push
                                               ▼
                        ┌───────────────────────────────────────────────┐
                        │ branch applicationbackup  (artifacts only)    │
                        │  1.0/…  1.2/…  latest/version.json  index.json│
                        └───────────────┬───────────────────────────────┘
                                        │ raw.githubusercontent.com/...
                                        ▼
                       in-app updater (UPDATE_FEED_URL / version.h default)
                       downloads SampleApp-<v>.exe, swaps, relaunches
```

Source branches keep **code + updated `updates/version.json`**; binaries live only
on `applicationbackup`, so the source branches stay small and reviewable.

## Files

| File | Purpose |
| --- | --- |
| `Jenkinsfile` | Declarative pipeline: checkout → build → stage → verify → archive → publish |
| `ci/build.ps1` | qmake + nmake (clean by default) + `windeployqt` + zip + `BUILD_INFO.json` |
| `ci/publish_artifacts.ps1` | Commits `ci-out/<version>` to the artifact branch, writes manifests, pushes |
| `ci/JENKINS.md` | This document |

Keep these files on **every** branch you build (1.0, 1.2, main) — Jenkins checks
out the source branch and looks for `Jenkinsfile` in it.

## 1. Jenkins controller + agent

Jenkins itself is not part of the repo; install it on a Windows machine that has
the toolchain:

1. Install **Jenkins LTS** (`.msi`, runs as a service, port 8080) — see
   <https://www.jenkins.io/doc/book/installing/windows/>.
   Java 17 is required and already present here (`Temurin-17.0.19`).
2. Install the agent prerequisites on the same machine (simplest: use the
   built-in node, no extra agent needed for a demo):
   - Git for Windows (`git --version`)
   - Visual Studio 2022 with "Desktop development with C++"
     (`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat`)
   - Qt 5.15.2 msvc2019_64 at `C:\Qt\5.15.2\msvc2019_64` (needs `qmake.exe` + `windeployqt.exe`)
3. Install plugins: **Git**, **Pipeline** (workflow-aggregator),
   **Credentials Binding**, **Timestamper**, **Workspace Cleanup** (optional).
4. If you use a real agent instead of the built-in node, give it the labels used
   by the pipeline (`windows` + `qt-5.15.2`):

   *Manage Jenkins → Nodes → New Node* → name `win-qt`, label `windows qt-5.15.2`,
   launch method *Launch agent by connecting it to the controller*, remote root
   `C:\jenkins-agent`, then follow the shown `agent.jar` command.

5. Create the push credential (*Manage Jenkins → Credentials → System → Global →
   Add Credentials*):
   - Kind: **Secret text**
   - ID: **`github-pat`**
   - Secret: a GitHub PAT (classic, scope `repo`) or fine-grained token with
     *Contents: Read and write* on `mohammedamirkhan-do/smaple_1.0`


## 2. Create the job

*New Item → Pipeline → OK*, then:

- **Pipeline → Definition**: *Pipeline script from SCM*
- **SCM**: Git, Repository URL `https://github.com/mohammedamirkhan-do/smaple_1.0.git`,
  Credentials = the same PAT (as *Username with password*: `<user>` + PAT, or a
  *Secret text* via `credentialsId`), **Branches to build** = `*/main`
  (`main` only needs to exist with the `Jenkinsfile`; the pipeline itself
  fetches the `BRANCH` parameter afterwards)
- **Script Path**: `Jenkinsfile`
- **Build triggers**: *Poll SCM* `H/5 * * * *` or a GitHub webhook
  (`http://<jenkins>:8080/github-webhook/`, needs the *GitHub* plugin)

Run it with *Build with Parameters*:

| Parameter | Meaning |
| --- | --- |
| `BRANCH` | source branch to build (`1.0`, `1.2`) |
| `PUBLISH` | `true` → push the result to `applicationbackup` |
| `ARTIFACT_BRANCH` | target branch, default `applicationbackup` |
| `KEEP_VERSIONS` | keep only the newest N version folders (`0` = all) |
| `QT_DIR`, `VCVARS` | toolchain paths (override per agent) |

Equivalent job creation from the CLI:

```bash
java -jar jenkins-cli.jar -s http://localhost:8080/ \
  -auth <user>:<api-token> create-job sampleapp-build < sampleapp-build.xml
```

with a minimal `sampleapp-build.xml`:

```xml
<?xml version='1.0' encoding='UTF-8'?>
<flow-definition plugin="workflow-job">
  <description>SampleApp build + publish to applicationbackup</description>
  <definition class="org.jenkinsci.plugins.workflow.cps.CpsScmFlowDefinition">
    <scm class="hudson.plugins.git.GitSCM">
      <userRemoteConfigs>
        <hudson.plugins.git.UserRemoteConfig>
          <url>https://github.com/mohammedamirkhan-do/smaple_1.0.git</url>
          <credentialsId>github-pat</credentialsId>
        </hudson.plugins.git.UserRemoteConfig>
      </userRemoteConfigs>
      <branches><hudson.plugins.git.BranchSpec><name>*/main</name></hudson.plugins.git.BranchSpec></branches>
    </scm>
    <scriptPath>Jenkinsfile</scriptPath>
  </definition>
</flow-definition>
```

## 3. Run the same steps without Jenkins

Everything the pipeline does is plain PowerShell, so it can be run by hand (or by
any other CI) on a machine with the toolchain:

```powershell
# build the currently checked-out branch and stage it under ci-out\<version>\
powershell -ExecutionPolicy Bypass -File ci\build.ps1

# publish that stage to applicationbackup (add -NoPush to commit locally only)
powershell -ExecutionPolicy Bypass -File ci\publish_artifacts.ps1 -InDir ci-out\1.2 -NoPush

# in CI: token comes from the Jenkins credential
powershell -ExecutionPolicy Bypass -File ci\publish_artifacts.ps1 -InDir ci-out\1.2 -Token $env:GH_TOKEN
```

## 4. What gets published, and how the updater uses it

```text
applicationbackup/
  index.json                     all versions + builtAt/branch/commit/sha256
  latest/version.json            manifest of the newest build
  1.0/  version.json  BUILD_INFO.json  SampleApp.exe  SampleApp-1.0.exe
        SampleApp-1.0-win64.zip   runtime/ (exe + Qt5*.dll + platforms/ ...)
  1.2/  ...
```

`ci/publish_artifacts.ps1` generates a manifest the app understands as-is:

```json
{
  "version": "1.2",
  "notes": "CI build of 1.2 @ a1b2c3d",
  "url": "https://raw.githubusercontent.com/mohammedamirkhan-do/smaple_1.0/applicationbackup/1.2/SampleApp-1.2.exe",
  "fallback_url": "https://raw.githubusercontent.com/mohammedamirkhan-do/smaple_1.0/applicationbackup/1.2/SampleApp.exe"
}
```

Both URLs point at a **raw exe** (the updater swaps the downloaded file in place),
so the mirror is a genuine second download path, not a zip.

Wire it up in one of two ways:

1. **Per-branch default** — set `UPDATE_VERSION_URL_DEFAULT` in `src/version.h` to
   `.../applicationbackup/latest/version.json` (or `<version>/version.json`).
2. **Runtime override (no rebuild)** — set the environment variable before
   launching: `set UPDATE_FEED_URL=https://raw.githubusercontent.com/.../applicationbackup/latest/version.json`

This replaces the manual "upload the exe to a GitHub Release" step: CI becomes
the update server, and a published build is immediately downloadable.

## 5. Caveats

- **Repo size**: every version folder adds ~50 MB of Qt runtime + a ~21 MB zip.
  Use `-Keep 2`, or `-IncludeCompilerRuntime:$false` (already the default, which
  skips the 25 MB `vc_redist`), or move the runtime to Git LFS / Jenkins
  `archiveArtifacts` only and publish just the payload exe to the branch.
- `ci/publish_artifacts.ps1` uses `git add -f` because `*.exe` is git-ignored in
  the source tree — the artifact branch intentionally keeps its own content.
- The publisher keeps a shallow clone in `%TEMP%\sampleapp-artifact-branch` and
  refreshes it with `git fetch --depth 1`, so repeated pipeline runs stay fast.
- The agent must be able to run `cmd` with `vcvars64.bat`; `nmake` refuses to
  relink if `release\SampleApp.exe` is newer than the objects, which is why
  `ci/build.ps1` cleans by default (use `-Incremental` only locally).
- Never echo `$env:GH_TOKEN`; the pipeline passes it via *Credentials Binding*
  (masked in the log) and the push URL is used once, never stored in git config
  (`-c credential.helper=`).
