#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

// UpdateManager (WHERE #2): polls version.json after startup,
// downloads the new exe, swaps it via updater script, restarts.
class UpdateManager : public QObject
{
    Q_OBJECT
public:
    explicit UpdateManager(QObject *parent = nullptr);
    // WHERE #3: interval comes from version.h (30s). checkNow() = "Check Now" button.
    void startAutoCheck(int intervalMs);
    void checkNow();

signals:
    void statusMessage(const QString &msg);
    void updateAvailable(const QString &newVersion);
    void downloadProgress(qint64 received, qint64 total);
    void updateApplied(const QString &newVersion);
    void updateFailed(const QString &error);
    void upToDate(const QString &version);

private slots:
    void onCheckTimer();
    void onManifestFinished();
    void onDownloadFinished();
    void onDownloadProgress(qint64 received, qint64 total);

private:
    QString currentVersion() const;
    QUrl manifestUrl() const;
    QString localManifestPath() const;
    bool handleLocalManifest();
    void fetchRemoteManifest(const QUrl &url);
    static int compareVersions(const QString &a, const QString &b);
    // WHERE #4: pick download url (primary, then optional mirror) and fetch it.
    void downloadUpdate(const QString &version, const QString &urlStr,
                        const QString &fallbackUrlStr = QString());
    void startDownload(const QUrl &url);
    void finishDownload();
    // WHERE #6: after a failed download, try the manifest mirror, else wait 60s.
    bool retryWithFallbackOrRecheck(const QString &reason);
    void logLine(const QString &msg) const;
    // Deletes payload/script leftovers from an earlier self-update.
    void cleanupStaleArtifacts() const;
    QString downloadTargetPath(const QString &version) const;
    // WHERE #5: generates apply_update.bat next to the exe.
    bool writeUpdaterScript(const QString &newExePath, const QString &targetExePath) const;
    QString updaterScriptPath() const;
    void scheduleRecheck();

    QNetworkAccessManager *m_net;
    QTimer *m_timer;
    QNetworkReply *m_manifestReply;
    QNetworkReply *m_downloadReply;
    QString m_pendingVersion;
    QString m_pendingTarget;
    QString m_pendingFallback;
    bool m_triedFallback;
    bool m_checkInProgress;
    bool m_applied;
};

#endif // UPDATEMANAGER_H
