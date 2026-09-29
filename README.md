# Simple Qt Widgets Desktop Application

Built with **Qt 5.15.2** + **qmake** (`.pro` project).

## Requirement
On the **top of the UI**, whatever the git repository name is, it is displayed.

- Repo URL: `https://github.com/Amirk9/Sample_0.1.git`
- Repo name shown on top banner: **`Sample_0.1`**

## How it works
At runtime the app runs:

```sh
git config --get remote.origin.url
```

parses the repository name from the URL (handles both HTTPS and SSH forms,
strips trailing `.git`), and shows it in the top green banner:

```text
Git Repository: Sample_0.1
```

If `git` is not available (e.g. running from an installed copy), it falls back
to the compile-time defaults `DEFAULT_REPO_NAME` / `DEFAULT_REPO_URL`
defined in `SampleApp.pro`.

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
