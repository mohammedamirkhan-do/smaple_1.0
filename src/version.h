#pragma once

// Central version for this branch. Bump per branch:
//  - branch 1.0 -> "1.0"
//  - branch 1.2 -> "1.2"
#define APP_VERSION_STR "1.0"
#define APP_BRANCH_STR "1.0"

// Auto-check after startup (30 sec as requested) + repeat while running.
#define UPDATE_CHECK_INTERVAL_MS 30000
#define UPDATE_RECHECK_INTERVAL_MS 60000

// Remote manifest polled when no local manifest is found.
// Host this file on branch 1.2 at updates/version.json, e.g.:
// https://raw.githubusercontent.com/mohammedamirkhan-do/smaple_1.0/1.2/updates/version.json
#define UPDATE_VERSION_URL_DEFAULT "https://raw.githubusercontent.com/mohammedamirkhan-do/smaple_1.0/1.2/updates/version.json"

// Local manifest searched first (offline demo, no server needed):
//   <exeDir>/updates/version.json
//   <workDir>/updates/version.json
#define UPDATE_LOCAL_MANIFEST_REL "updates/version.json"

// The manifest may carry an optional "fallback_url" mirror. If the primary
// "url" fails (HTTP error, or 404 because the GitHub Release is not published
// yet, or a truncated file), the updater retries the mirror automatically —
// still zero user clicks. Same swap + relaunch either way.
// NOTE: no compile-time mirror, it always comes from the manifest.

// Timeline log written next to the exe. The app is built as a Windows GUI
// (CONFIG -= console), so qDebug() output goes nowhere: this file is the only
// way to see what the auto-updater did.
#define UPDATE_LOG_FILE "update.log"

