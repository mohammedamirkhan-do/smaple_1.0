#include "updatemanager.h"
#include "version.h"

#include <QCoreApplication>
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
    , m_checkInProgress(false)
    , m_applied(false)
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &UpdateManager::onCheckTimer);
}

void UpdateManager::startAutoCheck(int intervalMs)
{
    m_timer->start(intervalMs);
    emit statusMessage(QStringLiteral("Auto-update check in %1s...").arg(intervalMs / 1000));
}

void UpdateManager::checkNow()
{
    m_timer->stop();
    onCheckTimer();
}

QString UpdateManager::currentVersion() const
{
    return QString::fromLatin1(APP_VERSION_STR);
}

QUrl UpdateManager::manifestUrl() const
{
    const QString env = QString::fromLocal8Bit(qgetenv("UPDATE_VERSION_URL")).trimmed();
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
    if (m_checkInProgress || m_applied)
        return;
    m_checkInProgress = true;
    emit statusMessage(QStringLiteral("Checking for updates..."));
    if (handleLocalManifest()) {
        m_checkInProgress = false;
        return;
    }
    fetchRemoteManifest(manifestUrl());
}


bool UpdateManager::handleLocalManifest()
{
    const QString path = localManifestPath();
    if (path.isEmpty())
        return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit updateFailed(QStringLiteral("Cannot read %1").arg(path));
        return true;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QJsonObject obj = doc.object();
    const QString latest = obj.value(QStringLiteral("version")).toString().trimmed();
    const QString url = obj.value(QStringLiteral("url")).toString().trimmed();
    qDebug() << "[update] local manifest" << path << "latest=" << latest << "url=" << url;
    if (latest.isEmpty()) {
        emit updateFailed(QStringLiteral("Bad manifest (no version): %1").arg(path));
        return true;
    }
    if (compareVersions(latest, currentVersion()) <= 0) {
        emit upToDate(currentVersion());
        return true;
    }
    emit updateAvailable(latest);
    downloadUpdate(latest, url);
    return true;
}

void UpdateManager::fetchRemoteManifest(const QUrl &url)
{
    if (!url.isValid()) {
        m_checkInProgress = false;
        emit updateFailed(QStringLiteral("Bad update URL"));
        return;
    }
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
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
        emit updateFailed(QStringLiteral("Version check failed: %1").arg(r->errorString()));
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(r->readAll());
    const QJsonObject obj = doc.object();
    const QString latest = obj.value(QStringLiteral("version")).toString().trimmed();
    const QString url = obj.value(QStringLiteral("url")).toString().trimmed();
    qDebug() << "[update] remote manifest latest=" << latest << "url=" << url;
    if (latest.isEmpty()) {
        emit updateFailed(QStringLiteral("Bad manifest (no version)"));
        return;
    }
    if (compareVersions(latest, currentVersion()) <= 0) {
        emit upToDate(currentVersion());
        return;
    }
    emit updateAvailable(latest);
    m_checkInProgress = true;
    downloadUpdate(latest, url);
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

void UpdateManager::downloadUpdate(const QString &version, const QString &urlStr)
{
    m_pendingVersion = version;
    if (urlStr.isEmpty()) {
        m_checkInProgress = false;
        emit updateFailed(QStringLiteral("Manifest has no download url"));
        return;
    }
    QUrl url(urlStr);
    if (url.isRelative()) {
        const QString local = localManifestPath();
        if (!local.isEmpty())
            url = QUrl::fromLocalFile(QDir(QFileInfo(local).absolutePath()).filePath(urlStr));
        else
            url = manifestUrl().resolved(url);
    }
    m_pendingTarget = downloadTargetPath(version);
    qDebug() << "[update] downloading" << url.toString() << "->" << m_pendingTarget;

    if (url.isLocalFile()) {
        // Offline demo: plain file copy, no HTTP needed.
        const QString src = url.toLocalFile();
        QFile::remove(m_pendingTarget);
        if (!QFile::copy(src, m_pendingTarget)) {
            m_checkInProgress = false;
            emit updateFailed(QStringLiteral("Copy failed: %1 not found?").arg(src));
            return;
        }
        emit downloadProgress(1, 1);
        m_checkInProgress = false;
        if (!writeUpdaterScript(m_pendingTarget, QCoreApplication::applicationFilePath())) {
            emit updateFailed(QStringLiteral("Cannot write updater script"));
            return;
        }
        m_applied = true;
        emit updateApplied(version);
        return;
    }

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
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
        emit updateFailed(QStringLiteral("Download failed: %1").arg(r->errorString()));
        return;
    }
    QFile out(m_pendingTarget);
    QFile::remove(m_pendingTarget);
    if (!out.open(QIODevice::WriteOnly)) {
        emit updateFailed(QStringLiteral("Cannot write %1").arg(m_pendingTarget));
        return;
    }
    out.write(r->readAll());
    out.close();
    qDebug() << "[update] saved" << m_pendingTarget;
    if (!writeUpdaterScript(m_pendingTarget, QCoreApplication::applicationFilePath())) {
        emit updateFailed(QStringLiteral("Cannot write updater script"));
        return;
    }
    m_applied = true;
    emit updateApplied(m_pendingVersion);
}

QString UpdateManager::updaterScriptPath() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("apply_update.bat"));
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
    content += QStringLiteral("REM Auto-generated by SampleApp UpdateManager.\r\n");
    content += QStringLiteral("echo [updater] waiting for old app to exit...\r\n");
    content += QStringLiteral(":waitloop\r\n");
    content += QStringLiteral("tasklist /FI \"PID eq %1\" 2>NUL | find /I \"%1\" >NUL\r\n").arg(pid);
    content += QStringLiteral("if not errorlevel 1 ( timeout /t 1 /nobreak >NUL & goto waitloop )\r\n");
    content += QStringLiteral("timeout /t 1 /nobreak >NUL\r\n");
    content += QStringLiteral("echo [updater] installing new version...\r\n");
    content += QStringLiteral("copy /Y \"%1\" \"%2\" >NUL\r\n").arg(nativeNew, nativeTarget);
    content += QStringLiteral("if errorlevel 1 ( echo [updater] copy FAILED & pause & exit /b 1 )\r\n");
    content += QStringLiteral("del \"%1\"\r\n").arg(nativeNew);
    content += QStringLiteral("echo [updater] starting new version...\r\n");
    content += QStringLiteral("start \"\" \"%1\"\r\n").arg(nativeTarget);
    content += QStringLiteral("del \"%%~f0\"\r\n");
    QFile f(bat);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(content.toLocal8Bit());
    f.close();
    return true;
}


