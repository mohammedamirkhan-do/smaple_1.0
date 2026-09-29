#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QMessageBox;
class QProgressBar;
class QPushButton;
class QSpinBox;
class UpdateManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshRepoInfo();
    void onSayHelloClicked();
    void onClearClicked();
    void onAboutClicked();
    void onThemeChanged(const QString &theme);
    void onFontSizeChanged(int size);
    void updateBranchLabel();
    void onCheckUpdateNow();
    void onUpdateStatus(const QString &msg);
    void onUpdateAvailable(const QString &newVersion);
    void onUpdateProgress(qint64 received, qint64 total);
    void onUpdateApplied(const QString &newVersion);
    void onUpdateFailed(const QString &error);
    void onUpToDate(const QString &version);

private:
    // Returns e.g. "smaple_1.0" parsed from the git remote URL.
    static QString gitRepoName();
    // Returns e.g. "https://github.com/mohammedamirkhan-do/smaple_1.0.git"
    static QString gitRepoUrl();
    // Returns current git branch (e.g. "1.2"), empty if unavailable.
    static QString gitBranchName();
    // Parses "https://github.com/user/smaple_1.0.git" -> "smaple_1.0"
    // Parses "git@github.com:user/smaple_1.0.git"      -> "smaple_1.0"
    static QString repoNameFromUrl(const QString &url);

    void updateRepoLabels();
    void applyTheme(const QString &theme);

    QLabel *m_topBannerLabel;   // <-- TOP of the UI: shows repo name
    QLabel *m_repoUrlLabel;     // shows full remote URL below the banner
    QLabel *m_branchLabel;      // NEW (1.2): current git branch
    QLabel *m_welcomeLabel;

    // NEW (1.2) feature controls
    QLineEdit *m_nameEdit;
    QPushButton *m_helloButton;
    QPushButton *m_clearButton;
    QPushButton *m_aboutButton;
    QPushButton *m_refreshButton;
    QComboBox *m_themeCombo;
    QSpinBox *m_fontSizeSpin;
    QCheckBox *m_boldCheck;
    QLabel *m_statusLabel;
    QLabel *m_versionLabel;     // NEW (1.2): version footer
    // Auto-update widgets (same engine as 1.0)
    QProgressBar *m_updateProgress;
    QPushButton *m_checkUpdateButton;
    QLabel *m_updateStatusLabel;
    UpdateManager *m_updater;
};

#endif // MAINWINDOW_H

