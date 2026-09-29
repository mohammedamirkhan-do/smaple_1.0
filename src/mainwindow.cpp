#include "mainwindow.h"
#include "updatemanager.h"
#include "version.h"

#include <QDebug>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QTimer>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#ifndef DEFAULT_REPO_NAME
#define DEFAULT_REPO_NAME "smaple_1.0"
#endif
#ifndef DEFAULT_REPO_URL
#define DEFAULT_REPO_URL "https://github.com/mohammedamirkhan-do/smaple_1.0.git"
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SampleApp v%1 - %2").arg(QString::fromLatin1(APP_VERSION_STR), gitRepoName()));
    resize(480, 380);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    // ---- TOP banner: git repository name ----
    m_topBannerLabel = new QLabel(central);
    m_topBannerLabel->setAlignment(Qt::AlignCenter);
    m_topBannerLabel->setStyleSheet(
        QStringLiteral("background-color: #1d7d32; color: white; "
                       "font-size: 18px; font-weight: bold; "
                       "padding: 12px; border-radius: 6px;"));
    layout->addWidget(m_topBannerLabel);

    // Full remote URL shown just below the banner.
    m_repoUrlLabel = new QLabel(central);
    m_repoUrlLabel->setAlignment(Qt::AlignCenter);
    m_repoUrlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_repoUrlLabel->setStyleSheet(QStringLiteral("color: #555; font-size: 12px;"));
    layout->addWidget(m_repoUrlLabel);

    layout->addStretch(1);

    // ---- Body ----
    m_welcomeLabel = new QLabel(tr("Hello from Qt Widgets v%1!\nThis is a simple desktop application.")
                                    .arg(QString::fromLatin1(APP_VERSION_STR)), central);
    m_welcomeLabel->setAlignment(Qt::AlignCenter);
    QFont welcomeFont = m_welcomeLabel->font();
    welcomeFont.setPointSize(11);
    m_welcomeLabel->setFont(welcomeFont);
    layout->addWidget(m_welcomeLabel);

    layout->addStretch(1);

    m_refreshButton = new QPushButton(tr("Refresh Repository Name"), central);
    layout->addWidget(m_refreshButton, 0, Qt::AlignCenter);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshRepoInfo);

    // ---- Auto-update UI (WHERE #3): progress + Check Now + status ----
    m_updateProgress = new QProgressBar(central);
    m_updateProgress->setRange(0, 100);
    m_updateProgress->setValue(0);
    m_updateProgress->setTextVisible(true);
    m_updateProgress->setFormat(tr("Idle"));
    layout->addWidget(m_updateProgress);

    auto *updateRow = new QHBoxLayout();
    m_checkUpdateButton = new QPushButton(tr("Check for Updates Now"), central);
    updateRow->addStretch(1);
    updateRow->addWidget(m_checkUpdateButton);
    updateRow->addStretch(1);
    layout->addLayout(updateRow);
    connect(m_checkUpdateButton, &QPushButton::clicked, this, &MainWindow::onCheckUpdateNow);

    m_updateStatusLabel = new QLabel(tr("Auto-update: waiting for 30s check..."), central);
    m_updateStatusLabel->setAlignment(Qt::AlignCenter);
    m_updateStatusLabel->setStyleSheet(QStringLiteral("color: #1a73e8; font-size: 11px;"));
    m_updateStatusLabel->setWordWrap(true);
    layout->addWidget(m_updateStatusLabel);

    m_statusLabel = new QLabel(central);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    layout->addWidget(m_statusLabel);

    setCentralWidget(central);

    updateRepoLabels();
    m_statusLabel->setText(tr("Press Refresh to re-read the git remote."));

    // ---- Auto-update wiring (WHERE #2 + #6) ----
    m_updater = new UpdateManager(this);
    connect(m_updater, &UpdateManager::statusMessage, this, &MainWindow::onUpdateStatus);
    connect(m_updater, &UpdateManager::updateAvailable, this, &MainWindow::onUpdateAvailable);
    connect(m_updater, &UpdateManager::downloadProgress, this, &MainWindow::onUpdateProgress);
    connect(m_updater, &UpdateManager::updateApplied, this, &MainWindow::onUpdateApplied);
    connect(m_updater, &UpdateManager::updateFailed, this, &MainWindow::onUpdateFailed);
    connect(m_updater, &UpdateManager::upToDate, this, &MainWindow::onUpToDate);
    m_updater->startAutoCheck(UPDATE_CHECK_INTERVAL_MS);
}

void MainWindow::refreshRepoInfo()
{
    updateRepoLabels();
    m_statusLabel->setText(tr("Repository info refreshed."));
}

void MainWindow::onCheckUpdateNow()
{
    m_updateProgress->setValue(0);
    m_updateProgress->setFormat(tr("Checking..."));
    m_updater->checkNow();
}

void MainWindow::onUpdateStatus(const QString &msg)
{
    m_updateStatusLabel->setText(tr("Auto-update: %1").arg(msg));
}

void MainWindow::onUpdateAvailable(const QString &newVersion)
{
    m_updateStatusLabel->setText(tr("Update v%1 found! Downloading...").arg(newVersion));
    m_updateProgress->setFormat(tr("Downloading v%1...").arg(newVersion));
}

void MainWindow::onUpdateProgress(qint64 received, qint64 total)
{
    if (total > 0) {
        const int pct = int(received * 100 / total);
        m_updateProgress->setValue(pct);
        m_updateProgress->setFormat(tr("%1%").arg(pct));
    } else {
        m_updateProgress->setRange(0, 0); // busy
        m_updateProgress->setFormat(tr("Downloading..."));
    }
}

void MainWindow::onUpdateApplied(const QString &newVersion)
{
    // WHERE #6: downloaded + swap script ready -> run script, quit, relaunch.
    m_updateProgress->setRange(0, 100);
    m_updateProgress->setValue(100);
    m_updateProgress->setFormat(tr("Done"));
    m_updateStatusLabel->setText(tr("v%1 downloaded. Restarting into new version...").arg(newVersion));
    qDebug() << "[update] launching apply_update.bat";
    QProcess::startDetached(QDir(QCoreApplication::applicationDirPath())
                                .filePath(QStringLiteral("apply_update.bat")),
                            QStringList(), QCoreApplication::applicationDirPath());
    QTimer::singleShot(1500, qApp, &QCoreApplication::quit);
}

void MainWindow::onUpdateFailed(const QString &error)
{
    m_updateProgress->setRange(0, 100);
    m_updateProgress->setFormat(tr("Failed"));
    m_updateStatusLabel->setText(tr("Update failed: %1").arg(error));
}

void MainWindow::onUpToDate(const QString &version)
{
    m_updateProgress->setRange(0, 100);
    m_updateProgress->setValue(100);
    m_updateProgress->setFormat(tr("Up to date"));
    m_updateStatusLabel->setText(tr("You are on the latest version (v%1).").arg(version));
}

QString MainWindow::repoNameFromUrl(const QString &url)
{
    QString cleaned = url.trimmed();
    // Remove trailing ".git"
    if (cleaned.endsWith(QStringLiteral(".git"), Qt::CaseInsensitive))
        cleaned.chop(4);
    // Remove trailing "/"
    while (cleaned.endsWith(QLatin1Char('/')))
        cleaned.chop(1);
    // Take text after last '/' or ':'
    int slashPos = cleaned.lastIndexOf(QLatin1Char('/'));
    int colonPos = cleaned.lastIndexOf(QLatin1Char(':'));
    int pos = qMax(slashPos, colonPos);
    QString name = (pos >= 0) ? cleaned.mid(pos + 1) : cleaned;
    return name.trimmed();
}

QString MainWindow::gitRepoUrl()
{
    // 1) Try reading the live git remote (works when running from a clone).
    QProcess git;
    git.start(QStringLiteral("git"),
              QStringList() << QStringLiteral("config") << QStringLiteral("--get")
                            << QStringLiteral("remote.origin.url"));
    if (git.waitForFinished(3000) && git.exitCode() == 0) {
        QString url = QString::fromLocal8Bit(git.readAllStandardOutput()).trimmed();
        if (!url.isEmpty())
            return url;
    }
    // 2) Fallback to compile-time default.
    return QString::fromLatin1(DEFAULT_REPO_URL);
}

QString MainWindow::gitRepoName()
{
    QString name = repoNameFromUrl(gitRepoUrl());
    if (!name.isEmpty())
        return name;
    return QString::fromLatin1(DEFAULT_REPO_NAME);
}

void MainWindow::updateRepoLabels()
{
    const QString repoName = gitRepoName();
    const QString repoUrl = gitRepoUrl();

    // TOP of UI: whatever the git repository name is.
    m_topBannerLabel->setText(tr("Git Repository: %1").arg(repoName));
    m_repoUrlLabel->setText(repoUrl);
    setWindowTitle(QStringLiteral("SampleApp v%1 - %2").arg(QString::fromLatin1(APP_VERSION_STR), repoName));

    qDebug() << "Repo name:" << repoName << "| Repo URL:" << repoUrl;
}
