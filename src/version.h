#pragma once

// Branch 1.2 is the NEWEST release: reports "1.2" so a 1.0 client
// comparing 1.0 < 1.2 knows an update exists.
#define APP_VERSION_STR "1.2"
#define APP_BRANCH_STR "1.2"

// 1.2 also self-updates (checks the same feed; reports "up to date").
#define UPDATE_CHECK_INTERVAL_MS 30000
#define UPDATE_RECHECK_INTERVAL_MS 60000

// GLOBAL update feed — 1.2 also reads it (but finds itself current).
#define UPDATE_FEED_URL_DEFAULT "https://raw.githubusercontent.com/mohammedamirkhan-do/smaple_1.0/1.2/updates/version.json"
#define UPDATE_VERSION_URL_DEFAULT UPDATE_FEED_URL_DEFAULT

#define UPDATE_LOCAL_MANIFEST_REL "updates/version.json"

// The manifest may carry an optional "fallback_url" mirror. If the primary
// "url" fails (HTTP error, or 404 because the GitHub Release is not published
// yet, or a truncated file), the updater retries the mirror automatically —
// still zero user clicks. Same swap + relaunch either way.

// Timeline log written next to the exe. The app is built as a Windows GUI
// (CONFIG -= console), so qDebug() output goes nowhere: this file is the only
// way to see what the auto-updater did.
#define UPDATE_LOG_FILE "update.log"
