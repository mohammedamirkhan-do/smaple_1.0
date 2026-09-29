#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QProgressBar;
class QPushButton;
class UpdateManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshRepoInfo();
    void onCheckUpdateNow();
    void onUpdateStatus(const QString &msg);
    void onUpdateAvailable(const QString &newVersion);
    void onUpdateProgress(qint64 received, qint64 total);
    void onUpdateApplied(const QString &newVersion);
    void onUpdateFailed(const QString &error);
    void onUpToDate(const QString &version);

private:
    // Returns e.g. "Sample_0.1" parsed from the git remote URL.
    static QString gitRepoName();
    // Returns e.g. "https://github.com/Amirk9/Sample_0.1.git"
    static QString gitRepoUrl();
    // Parses "https://github.com/Amirk9/Sample_0.1.git" -> "Sample_0.1"
    // Parses "git@github.com:Amirk9/Sample_0.1.git"      -> "Sample_0.1"
    static QString repoNameFromUrl(const QString &url);

    void updateRepoLabels();

    QLabel *m_topBannerLabel;   // <-- TOP of the UI: shows repo name
    QLabel *m_repoUrlLabel;     // shows full remote URL below the banner
    QLabel *m_welcomeLabel;
    QPushButton *m_refreshButton;
    QLabel *m_statusLabel;
    // Auto-update widgets (WHERE #3)
    QProgressBar *m_updateProgress;
    QPushButton *m_checkUpdateButton;
    QLabel *m_updateStatusLabel;
    UpdateManager *m_updater;   // WHERE #2
};

#endif // MAINWINDOW_H
