#include "mainwindow.h"

#include <QDebug>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
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

#ifndef APP_VERSION
#define APP_VERSION "1.2"
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SampleApp v%1 - %2").arg(QString::fromLatin1(APP_VERSION), gitRepoName()));
    resize(520, 460);

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

    // NEW (1.2): branch badge.
    m_branchLabel = new QLabel(central);
    m_branchLabel->setAlignment(Qt::AlignCenter);
    m_branchLabel->setStyleSheet(
        QStringLiteral("background-color: #e8f0fe; color: #1a73e8; "
                       "font-size: 12px; font-weight: bold; "
                       "padding: 4px; border-radius: 4px;"));
    layout->addWidget(m_branchLabel);

    layout->addStretch(1);

    // ---- Body ----
    m_welcomeLabel = new QLabel(tr("Hello from Qt Widgets!\nThis is a simple desktop application."), central);
    m_welcomeLabel->setAlignment(Qt::AlignCenter);
    QFont welcomeFont = m_welcomeLabel->font();
    welcomeFont.setPointSize(11);
    m_welcomeLabel->setFont(welcomeFont);
    layout->addWidget(m_welcomeLabel);

    // NEW (1.2): name input + greeting buttons.
    auto *nameRow = new QHBoxLayout();
    m_nameEdit = new QLineEdit(central);
    m_nameEdit->setPlaceholderText(tr("Enter your name..."));
    m_nameEdit->setClearButtonEnabled(true);
    nameRow->addWidget(m_nameEdit, 1);

    m_helloButton = new QPushButton(tr("Say Hello"), central);
    nameRow->addWidget(m_helloButton);

    m_clearButton = new QPushButton(tr("Clear"), central);
    nameRow->addWidget(m_clearButton);
    layout->addLayout(nameRow);

    // NEW (1.2): theme + font size controls.
    auto *settingsRow = new QHBoxLayout();
    auto *themeLabel = new QLabel(tr("Theme:"), central);
    settingsRow->addWidget(themeLabel);

    m_themeCombo = new QComboBox(central);
    m_themeCombo->addItems(QStringList() << QStringLiteral("Green")
                                         << QStringLiteral("Blue")
                                         << QStringLiteral("Dark"));
    settingsRow->addWidget(m_themeCombo, 1);

    auto *fontLabel = new QLabel(tr("Font:"), central);
    settingsRow->addWidget(fontLabel);

    m_fontSizeSpin = new QSpinBox(central);
    m_fontSizeSpin->setRange(8, 24);
    m_fontSizeSpin->setValue(11);
    m_fontSizeSpin->setSuffix(tr(" pt"));
    settingsRow->addWidget(m_fontSizeSpin);

    m_boldCheck = new QCheckBox(tr("Bold"), central);
    settingsRow->addWidget(m_boldCheck);
    layout->addLayout(settingsRow);

    // NEW (1.2): button row with Refresh + About.
    auto *buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    m_refreshButton = new QPushButton(tr("Refresh Repository Name"), central);
    buttonRow->addWidget(m_refreshButton);
    m_aboutButton = new QPushButton(tr("About"), central);
    buttonRow->addWidget(m_aboutButton);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    layout->addStretch(1);

    m_statusLabel = new QLabel(central);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    layout->addWidget(m_statusLabel);

    // NEW (1.2): version footer.
    m_versionLabel = new QLabel(tr("SampleApp v%1 | branch 1.2 features").arg(QString::fromLatin1(APP_VERSION)), central);
    m_versionLabel->setAlignment(Qt::AlignCenter);
    m_versionLabel->setStyleSheet(QStringLiteral("color: #aaa; font-size: 10px;"));
    layout->addWidget(m_versionLabel);

    setCentralWidget(central);

    // ---- Signals ----
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshRepoInfo);
    connect(m_helloButton, &QPushButton::clicked, this, &MainWindow::onSayHelloClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &MainWindow::onClearClicked);
    connect(m_aboutButton, &QPushButton::clicked, this, &MainWindow::onAboutClicked);
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &MainWindow::onSayHelloClicked);
    connect(m_themeCombo, &QComboBox::currentTextChanged, this, &MainWindow::onThemeChanged);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onFontSizeChanged);
    connect(m_boldCheck, &QCheckBox::toggled, this, [this](bool) { onFontSizeChanged(m_fontSizeSpin->value()); });

    updateRepoLabels();
    updateBranchLabel();
    m_statusLabel->setText(tr("Welcome to v1.2! Try the new buttons below."));
}

void MainWindow::refreshRepoInfo()
{
    updateRepoLabels();
    updateBranchLabel();
    m_statusLabel->setText(tr("Repository info refreshed at %1.")
                               .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"))));
}

void MainWindow::onSayHelloClicked()
{
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        m_welcomeLabel->setText(tr("Hello from Qt Widgets!\nPlease enter your name above."));
        m_statusLabel->setText(tr("Tip: type a name and press Say Hello."));
        return;
    }
    m_welcomeLabel->setText(tr("Hello, %1! Welcome to SampleApp v%2.").arg(name, QString::fromLatin1(APP_VERSION)));
    m_statusLabel->setText(tr("Greeted %1.").arg(name));
}

void MainWindow::onClearClicked()
{
    m_nameEdit->clear();
    m_welcomeLabel->setText(tr("Hello from Qt Widgets!\nThis is a simple desktop application."));
    m_statusLabel->setText(tr("Cleared."));
    m_nameEdit->setFocus();
}

void MainWindow::onAboutClicked()
{
    QMessageBox::about(this, tr("About SampleApp"),
                       tr("<b>SampleApp v%1</b><br>"
                          "Simple Qt Widgets desktop app.<br><br>"
                          "Top banner shows git repo: <b>%2</b><br>"
                          "Branch: <b>%3</b><br>"
                          "New in 1.2: Say Hello, Clear, Theme picker, Font size.")
                           .arg(QString::fromLatin1(APP_VERSION), gitRepoName(), gitBranchName()));
    m_statusLabel->setText(tr("About dialog shown."));
}

void MainWindow::onThemeChanged(const QString &theme)
{
    applyTheme(theme);
    m_statusLabel->setText(tr("Theme changed to %1.").arg(theme));
}

void MainWindow::onFontSizeChanged(int size)
{
    QFont f = m_welcomeLabel->font();
    f.setPointSize(size);
    f.setBold(m_boldCheck->isChecked());
    m_welcomeLabel->setFont(f);
    m_statusLabel->setText(tr("Welcome text set to %1 pt%2.")
                               .arg(size)
                               .arg(m_boldCheck->isChecked() ? tr(" (bold)") : QString()));
}

void MainWindow::updateBranchLabel()
{
    const QString branch = gitBranchName();
    if (branch.isEmpty())
        m_branchLabel->setText(tr("Branch: (unknown)"));
    else
        m_branchLabel->setText(tr("Branch: %1").arg(branch));
    setWindowTitle(QStringLiteral("SampleApp v%1 - %2 [%3]")
                       .arg(QString::fromLatin1(APP_VERSION), gitRepoName(), branch));
}

void MainWindow::applyTheme(const QString &theme)
{
    if (theme == QLatin1String("Blue")) {
        m_topBannerLabel->setStyleSheet(
            QStringLiteral("background-color: #1a73e8; color: white; "
                           "font-size: 18px; font-weight: bold; "
                           "padding: 12px; border-radius: 6px;"));
    } else if (theme == QLatin1String("Dark")) {
        m_topBannerLabel->setStyleSheet(
            QStringLiteral("background-color: #202124; color: #e8eaed; "
                           "font-size: 18px; font-weight: bold; "
                           "padding: 12px; border-radius: 6px; "
                           "border: 1px solid #5f6368;"));
    } else {
        m_topBannerLabel->setStyleSheet(
            QStringLiteral("background-color: #1d7d32; color: white; "
                           "font-size: 18px; font-weight: bold; "
                           "padding: 12px; border-radius: 6px;"));
    }
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

QString MainWindow::gitBranchName()
{
    QProcess git;
    git.start(QStringLiteral("git"),
              QStringList() << QStringLiteral("rev-parse") << QStringLiteral("--abbrev-ref")
                            << QStringLiteral("HEAD"));
    if (git.waitForFinished(3000) && git.exitCode() == 0) {
        QString branch = QString::fromLocal8Bit(git.readAllStandardOutput()).trimmed();
        if (!branch.isEmpty() && branch != QLatin1String("HEAD"))
            return branch;
    }
    return QString();
}

void MainWindow::updateRepoLabels()
{
    const QString repoName = gitRepoName();
    const QString repoUrl = gitRepoUrl();

    // TOP of UI: whatever the git repository name is.
    m_topBannerLabel->setText(tr("Git Repository: %1").arg(repoName));
    m_repoUrlLabel->setText(repoUrl);
    setWindowTitle(QStringLiteral("SampleApp v%1 - %2").arg(QString::fromLatin1(APP_VERSION), repoName));

    qDebug() << "Repo name:" << repoName << "| Repo URL:" << repoUrl;
}
