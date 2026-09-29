# Simple Qt Widgets Desktop Application (v1.2)

Built with **Qt 5.15.2** + **qmake** (`.pro` project).

## Requirement
On the **top of the UI**, whatever the git repository name is, it is displayed.

- Repo URL: `https://github.com/mohammedamirkhan-do/smaple_1.0.git`
- Repo name shown on top banner: **`smaple_1.0`**

## New in 1.2 (branch `1.2`, based on `1.0`)

- **Branch badge** under the repo URL showing current git branch (`git rev-parse --abbrev-ref HEAD`)
- **Say Hello + Clear buttons** with a `QLineEdit` name input (Enter also triggers Hello)
- **About button** showing `QMessageBox` with repo + branch info
- **Theme picker** (`QComboBox`: Green / Blue / Dark) restyling the top banner
- **Font size** (`QSpinBox` 8–24 pt) + **Bold** (`QCheckBox`) for welcome text
- **Status bar label** with timestamped refresh + **version footer** `SampleApp v1.2`

## How it works
At runtime the app runs:

```sh
git config --get remote.origin.url
```

parses the repository name from the URL (handles both HTTPS and SSH forms,
strips trailing `.git`), and shows it in the top green banner:

```text
Git Repository: smaple_1.0
```

If `git` is not available (e.g. running from an installed copy), it falls back
to the hardcoded defaults in `src/mainwindow.cpp`.

## Project layout

```text
SampleApp.pro
src/
  main.cpp
  mainwindow.h
  mainwindow.cpp
```

## Build (Windows + MSVC)

From a Visual Studio command prompt (`vcvars64.bat` loaded):

```bat
qmake SampleApp.pro
nmake            :: or jom
release\SampleApp.exe
```

Or open `SampleApp.pro` directly in **Qt Creator** and press Run.

## Build (Linux / macOS)

```sh
qmake SampleApp.pro
make
./SampleApp
```

## Continuous integration (Jenkins)

Builds and binary publishing are automated: the pipeline stages the app
(exe + Qt DLLs via `windeployqt` + zip) and pushes it to the
`applicationbackup` branch, which also serves the update manifest consumed by
the in-app updater.

- Pipeline definition: `Jenkinsfile`
- Scripts: `ci/build.ps1`, `ci/publish_artifacts.ps1`
- Setup guide (Jenkins, credentials, artifact layout): `ci/JENKINS.md`

Local equivalents, no Jenkins required:

```powershell
powershell -ExecutionPolicy Bypass -File ci\build.ps1
powershell -ExecutionPolicy Bypass -File ci\publish_artifacts.ps1 -InDir ci-out\<version> -NoPush
```
