#include "updatemanager.h"
#include "version.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
    , m_manifestReply(nullptr)
    , m_downloadReply(nullptr)
    , m_triedFallback(false)
    , m_checkInProgress(false)
    , m_applied(false)
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_checkTrigger = QStringLiteral("timer");
        onCheckTimer();
    });
    cleanupStaleArtifacts();
}

// A previous self-update may have left the payload and the swap script behind
// (cmd.exe locks its own .bat while running, so self-deletion is unreliable).
void UpdateManager::cleanupStaleArtifacts() const
{
    const QString dir = QCoreApplication::applicationDirPath();
    const QStringList leftovers = QDir(dir).entryList(QStringList() << QStringLiteral("SampleApp-*.new.exe"),
                                                      QDir::Files);
    for (const QString &name : leftovers)
        QFile::remove(QDir(dir).filePath(name));
    if (!leftovers.isEmpty() || QFile::exists(updaterScriptPath()))
        logLine(QStringLiteral("cleaned stale updater artifacts (payload=%1, script=%2)")
                    .arg(leftovers.size())
                    .arg(QFile::exists(updaterScriptPath()) ? 1 : 0));
    QFile::remove(updaterScriptPath());
}

void UpdateManager::logLine(const QString &msg) const
{
    // Append-only timeline next to the exe: GUI app, so no console output.
    QFile f(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral(UPDATE_LOG_FILE)));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append))
        return;
    f.write(QStringLiteral("[%1] v%2 %3\r\n")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                     QString::fromLatin1(APP_VERSION_STR), msg)
                .toUtf8());
    f.close();
}

void UpdateManager::startAutoCheck(int intervalMs)
{
    m_timer->start(intervalMs);
    logLine(QStringLiteral("startAutoCheck: first check in %1s, then every %2s")
                .arg(intervalMs / 1000)
                .arg(UPDATE_RECHECK_INTERVAL_MS / 1000));
    emit statusMessage(QStringLiteral("Auto-update check in %1s (then every 60s)...").arg(intervalMs / 1000));
}

void UpdateManager::checkNow()
{
    m_timer->stop();
    m_checkTrigger = QStringLiteral("manual check-now button");
    onCheckTimer();
}

QString UpdateManager::currentVersion() const
{
    return QString::fromLatin1(APP_VERSION_STR);
}

QUrl UpdateManager::manifestUrl() const
{
    // Env override (new name first, old name back-compat).
    QString env = QString::fromLocal8Bit(qgetenv("UPDATE_FEED_URL")).trimmed();
    if (env.isEmpty())
        env = QString::fromLocal8Bit(qgetenv("UPDATE_VERSION_URL")).trimmed();
    if (!env.isEmpty())
        return QUrl(env);
    return QUrl(QString::fromLatin1(UPDATE_VERSION_URL_DEFAULT));
}

QString UpdateManager::localManifestPath() const
{
    const QString rel = QString::fromLatin1(UPDATE_LOCAL_MANIFEST_REL);
    const QString inExeDir = QDir(QCoreApplication::applicationDirPath()).filePath(rel);
    if (QFile::exists(inExeDir))
        return inExeDir;
    const QString inCwd = QDir(QDir::currentPath()).filePath(rel);
    if (QFile::exists(inCwd))
        return inCwd;
    return QString();
}

void UpdateManager::onCheckTimer()
{
    const QString trigger = m_checkTrigger.isEmpty() ? QStringLiteral("direct call") : m_checkTrigger;
    m_checkTrigger.clear();
    if (m_checkInProgress || m_applied) {
        // Useful when a check is requested while one is already running.
        logLine(QStringLiteral("check skipped (busy), trigger: %1").arg(trigger));
        return;
    }
    m_checkInProgress = true;
    emit statusMessage(QStringLiteral("Checking for updates..."));
    logLine(QStringLiteral("---- update check started (trigger: %1, timerRemaining: %2ms) ----")
                .arg(trigger)
                .arg(m_timer->remainingTime()));
    if (handleLocalManifest())
        return; // local path (offline demo) handles its own state
    fetchRemoteManifest(manifestUrl());
}


bool UpdateManager::handleLocalManifest()
{
    const QString path = localManifestPath();
    if (path.isEmpty())
        return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_checkInProgress = false;
        emit updateFailed(QStringLiteral("Cannot read %1").arg(path));
        scheduleRecheck();
        return true;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QJsonObject obj = doc.object();
    const QString latest = obj.value(QStringLiteral("version")).toString().trimmed();
    const QString url = obj.value(QStringLiteral("url")).toString().trimmed();
    const QString mirror = obj.value(QStringLiteral("fallback_url")).toString().trimmed();
    qDebug() << "[update] local manifest" << path << "latest=" << latest << "url=" << url;
    logLine(QStringLiteral("local manifest %1 -> version=%2 url=%3").arg(path, latest, url));
    if (latest.isEmpty()) {
        m_checkInProgress = false;
        emit updateFailed(QStringLiteral("Bad manifest (no version): %1").arg(path));
        scheduleRecheck();
        return true;
    }
    if (compareVersions(latest, currentVersion()) <= 0) {
        m_checkInProgress = false;
        logLine(QStringLiteral("already on latest (local manifest): v%1").arg(currentVersion()));
        emit upToDate(currentVersion());
        scheduleRecheck();
        return true;
    }
    logLine(QStringLiteral("new version %1 available (current v%2)").arg(latest, currentVersion()));
    emit updateAvailable(latest);
    downloadUpdate(latest, url, mirror);
    return true;
}

void UpdateManager::fetchRemoteManifest(const QUrl &url)
{
    if (!url.isValid()) {
        m_checkInProgress = false;
        emit updateFailed(QStringLiteral("Bad update URL"));
        scheduleRecheck();
        return;
    }
    logLine(QStringLiteral("fetching manifest %1").arg(url.toString()));
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("User-Agent", "SampleApp-Updater/1.0");
    req.setRawHeader("Cache-Control", "no-cache");
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    if (m_manifestReply)
        m_manifestReply->deleteLater();
    m_manifestReply = m_net->get(req);
    connect(m_manifestReply, &QNetworkReply::finished, this, &UpdateManager::onManifestFinished);
}

void UpdateManager::onManifestFinished()
{
    QNetworkReply *r = m_manifestReply;
    m_manifestReply = nullptr;
    m_checkInProgress = false;
    if (!r) return;
    r->deleteLater();
    if (r->error() != QNetworkReply::NoError) {
        logLine(QStringLiteral("manifest fetch FAILED: %1").arg(r->errorString()));
        emit updateFailed(QStringLiteral("Version check failed: %1 — retrying...").arg(r->errorString()));
        scheduleRecheck();
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(r->readAll());
    const QJsonObject obj = doc.object();
    const QString latest = obj.value(QStringLiteral("version")).toString().trimmed();
    const QString url = obj.value(QStringLiteral("url")).toString().trimmed();
    const QString mirror = obj.value(QStringLiteral("fallback_url")).toString().trimmed();
    qDebug() << "[update] remote manifest latest=" << latest << "url=" << url;
    logLine(QStringLiteral("remote manifest -> version=%1 url=%2 mirror=%3").arg(latest, url, mirror));
    if (latest.isEmpty()) {
        emit updateFailed(QStringLiteral("Bad manifest (no version)"));
        scheduleRecheck();
        return;
    }
    if (compareVersions(latest, currentVersion()) <= 0) {
        logLine(QStringLiteral("already on latest (remote manifest): v%1").arg(currentVersion()));
        emit upToDate(currentVersion());
        scheduleRecheck();
        return;
    }
    logLine(QStringLiteral("new version %1 available (current v%2)").arg(latest, currentVersion()));
    emit updateAvailable(latest);
    m_checkInProgress = true;
    downloadUpdate(latest, url, mirror);
}

int UpdateManager::compareVersions(const QString &a, const QString &b)
{
    const QStringList pa = a.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    const QStringList pb = b.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    const int n = qMax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        const int va = i < pa.size() ? pa.at(i).toInt() : 0;
        const int vb = i < pb.size() ? pb.at(i).toInt() : 0;
        if (va != vb) return va < vb ? -1 : 1;
    }
    return 0;
}


QString UpdateManager::downloadTargetPath(const QString &version) const
{
    const QString dir = QCoreApplication::applicationDirPath();
    return QDir(dir).filePath(QStringLiteral("SampleApp-%1.new.exe").arg(version));
}

void UpdateManager::downloadUpdate(const QString &version, const QString &urlStr,
                                   const QString &fallbackUrlStr)
{
    m_pendingVersion = version;
    m_pendingFallback = fallbackUrlStr;
    m_triedFallback = false;
    if (urlStr.isEmpty() && fallbackUrlStr.isEmpty()) {
        m_checkInProgress = false;
        emit updateFailed(QStringLiteral("Manifest has no download url"));
        scheduleRecheck();
        return;
    }
    m_pendingTarget = downloadTargetPath(version);
    if (urlStr.isEmpty()) {           // manifest only carried a mirror
        m_triedFallback = true;
        startDownload(QUrl(fallbackUrlStr));
        return;
    }
    startDownload(QUrl(urlStr));
}

void UpdateManager::startDownload(const QUrl &rawUrl)
{
    QUrl url = rawUrl;
    if (url.isRelative()) {
        // Relative "url" support: resolve against the local manifest folder,
        // otherwise against the remote manifest url.
        const QString local = localManifestPath();
        if (!local.isEmpty())
            url = QUrl::fromLocalFile(QDir(QFileInfo(local).absolutePath()).filePath(rawUrl.toString()));
        else
            url = manifestUrl().resolved(url);
    }
    qDebug() << "[update] downloading" << url.toString() << "->" << m_pendingTarget;
    logLine(QStringLiteral("downloading %1").arg(url.toString()));

    if (url.isLocalFile()) {
        // Offline demo: plain file copy, no HTTP needed.
        const QString src = url.toLocalFile();
        QFile::remove(m_pendingTarget);
        if (!QFile::copy(src, m_pendingTarget)) {
            retryWithFallbackOrRecheck(QStringLiteral("Copy failed: %1 not found?").arg(src));
            return;
        }
        if (QFileInfo(m_pendingTarget).size() < 20000) {
            QFile::remove(m_pendingTarget);
            retryWithFallbackOrRecheck(QStringLiteral("Local payload too small — not a valid exe."));
            return;
        }
        emit downloadProgress(1, 1);
        finishDownload();
        return;
    }

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("User-Agent", "SampleApp-Updater/1.0");
    req.setRawHeader("Cache-Control", "no-cache");
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    if (m_downloadReply)
        m_downloadReply->deleteLater();
    m_downloadReply = m_net->get(req);
    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, &UpdateManager::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished, this, &UpdateManager::onDownloadFinished);
}

void UpdateManager::onDownloadProgress(qint64 received, qint64 total)
{
    emit downloadProgress(received, total);
}

void UpdateManager::onDownloadFinished()
{
    QNetworkReply *r = m_downloadReply;
    m_downloadReply = nullptr;
    m_checkInProgress = false;
    if (!r) return;
    r->deleteLater();
    if (r->error() != QNetworkReply::NoError) {
        retryWithFallbackOrRecheck(QStringLiteral("Download failed: %1").arg(r->errorString()));
        return;
    }
    QFile out(m_pendingTarget);
    QFile::remove(m_pendingTarget);
    if (!out.open(QIODevice::WriteOnly)) {
        retryWithFallbackOrRecheck(QStringLiteral("Cannot write %1").arg(m_pendingTarget));
        return;
    }
    const QByteArray data = r->readAll();
    out.write(data);
    out.close();
    logLine(QStringLiteral("download finished: %1 bytes -> %2")
                .arg(data.size())
                .arg(m_pendingTarget));
    if (QFileInfo(m_pendingTarget).size() < 20000) {
        QFile::remove(m_pendingTarget);
        retryWithFallbackOrRecheck(QStringLiteral("Downloaded file too small (%1 bytes) — not a valid exe.")
                                       .arg(data.size()));
        return;
    }
    finishDownload();
}

// Mirrors are only tried once per check; after that we wait for the next tick.
bool UpdateManager::retryWithFallbackOrRecheck(const QString &reason)
{
    logLine(QStringLiteral("FAILED: %1").arg(reason));
    if (!m_triedFallback && !m_pendingFallback.isEmpty()) {
        m_triedFallback = true;
        logLine(QStringLiteral("retrying with mirror %1").arg(m_pendingFallback));
        emit statusMessage(QStringLiteral("Primary download failed — trying mirror..."));
        emit updateFailed(reason + QStringLiteral(" — trying mirror"));
        startDownload(QUrl(m_pendingFallback));
        return true;
    }
    m_checkInProgress = false;
    emit updateFailed(reason + QStringLiteral(" — will retry in %1s")
                                   .arg(UPDATE_RECHECK_INTERVAL_MS / 1000));
    scheduleRecheck();
    return false;
}

// WHERE #5/#6: the payload is on disk -> write the swap script and hand over.
void UpdateManager::finishDownload()
{
    m_checkInProgress = false;
    if (!writeUpdaterScript(m_pendingTarget, QCoreApplication::applicationFilePath())) {
        m_checkInProgress = false;
        emit updateFailed(QStringLiteral("Cannot write updater script"));
        scheduleRecheck();
        return;
    }
    m_applied = true;
    logLine(QStringLiteral("updater script written: %1").arg(updaterScriptPath()));
    emit updateApplied(m_pendingVersion);
}

QString UpdateManager::updaterScriptPath() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("apply_update.bat"));
}

void UpdateManager::scheduleRecheck()
{
    if (!m_applied)
        m_timer->start(UPDATE_RECHECK_INTERVAL_MS);
}

bool UpdateManager::writeUpdaterScript(const QString &newExePath, const QString &targetExePath) const
{
    // Windows cannot overwrite a running exe: .bat waits, swaps, restarts.
    const QString bat = updaterScriptPath();
    const QString nativeNew = QDir::toNativeSeparators(newExePath);
    const QString nativeTarget = QDir::toNativeSeparators(targetExePath);
    const QString pid = QString::number(QCoreApplication::applicationPid());
    QString content;
    content += QStringLiteral("@echo off\r\n");
    content += QStringLiteral("REM Auto-generated by SampleApp UpdateManager. Do not commit.\r\n");
    content += QStringLiteral("set \"LOG=%~dp0%1\"\r\n").arg(QString::fromLatin1(UPDATE_LOG_FILE));
    content += QStringLiteral("echo [%date% %time%] [updater] waiting for PID %1 to exit >> \"%LOG%\"\r\n").arg(pid);
    content += QStringLiteral(":waitloop\r\n");
    content += QStringLiteral("tasklist /FI \"PID eq %1\" 2>NUL | find /I \"%1\" >NUL\r\n").arg(pid);
    content += QStringLiteral("if not errorlevel 1 ( timeout /t 1 /nobreak >NUL & goto waitloop )\r\n");
    content += QStringLiteral("timeout /t 1 /nobreak >NUL\r\n");
    content += QStringLiteral("echo [%date% %time%] [updater] old app exited, installing new version >> \"%LOG%\"\r\n");
    content += QStringLiteral("copy /Y \"%1\" \"%2\" >NUL\r\n").arg(nativeNew, nativeTarget);
    content += QStringLiteral("if errorlevel 1 (\r\n");
    content += QStringLiteral("  echo [%date% %time%] [updater] COPY FAILED >> \"%LOG%\"\r\n");
    content += QStringLiteral("  exit /b 1\r\n");
    content += QStringLiteral(")\r\n");
    content += QStringLiteral("echo [%date% %time%] [updater] swapped in new exe, launching it >> \"%LOG%\"\r\n");
    content += QStringLiteral("del \"%1\"\r\n").arg(nativeNew);
    content += QStringLiteral("start \"\" \"%1\"\r\n").arg(nativeTarget);
    // cmd.exe keeps the running .bat locked, so self-deletion often fails;
    // whatever survives is removed by cleanupStaleArtifacts() on next start.
    content += QStringLiteral("del \"%~f0\" 2>NUL\r\n");
    QFile f(bat);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(content.toLocal8Bit());
    f.close();
    return true;
}


