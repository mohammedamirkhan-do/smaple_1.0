#pragma once

// Central version for this branch. Bump per branch:
//  - branch 1.0 -> "1.0"
//  - branch 1.2 -> "1.2"
#define APP_VERSION_STR "1.0"
#define APP_BRANCH_STR "1.0"

// Auto-check after startup (30 sec as requested).
#define UPDATE_CHECK_INTERVAL_MS 30000

// Remote manifest polled when no local manifest is found.
// Host this file on branch 1.2 at updates/version.json, e.g.:
// https://raw.githubusercontent.com/mohammedamirkhan-do/smaple_1.0/1.2/updates/version.json
#define UPDATE_VERSION_URL_DEFAULT "https://raw.githubusercontent.com/mohammedamirkhan-do/smaple_1.0/1.2/updates/version.json"

// Local manifest searched first (offline demo, no server needed):
//   <exeDir>/updates/version.json
//   <workDir>/updates/version.json
#define UPDATE_LOCAL_MANIFEST_REL "updates/version.json"
