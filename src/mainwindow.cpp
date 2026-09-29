#include "mainwindow.h"

#include <QDebug>
#include <QFont>
#include <QLabel>
#include <QProcess>
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
    setWindowTitle(QStringLiteral("SampleApp - %1").arg(gitRepoName()));
    resize(480, 320);

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
    m_welcomeLabel = new QLabel(tr("Hello from Qt Widgets!\nThis is a simple desktop application."), central);
    m_welcomeLabel->setAlignment(Qt::AlignCenter);
    QFont welcomeFont = m_welcomeLabel->font();
    welcomeFont.setPointSize(11);
    m_welcomeLabel->setFont(welcomeFont);
    layout->addWidget(m_welcomeLabel);

    layout->addStretch(1);

    m_refreshButton = new QPushButton(tr("Refresh Repository Name"), central);
    layout->addWidget(m_refreshButton, 0, Qt::AlignCenter);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshRepoInfo);

    m_statusLabel = new QLabel(central);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    layout->addWidget(m_statusLabel);

    setCentralWidget(central);

    updateRepoLabels();
    m_statusLabel->setText(tr("Press Refresh to re-read the git remote."));
}

void MainWindow::refreshRepoInfo()
{
    updateRepoLabels();
    m_statusLabel->setText(tr("Repository info refreshed."));
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
    setWindowTitle(QStringLiteral("SampleApp - %1").arg(repoName));

    qDebug() << "Repo name:" << repoName << "| Repo URL:" << repoUrl;
}
