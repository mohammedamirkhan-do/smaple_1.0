#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshRepoInfo();

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
};

#endif // MAINWINDOW_H
