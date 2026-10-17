#pragma once
#include <QMainWindow>

class QProcess;
class QLabel;
class QPlainTextEdit;
class QToolBar;
class EditorQtHost;
class FileBrowserDock;
class ViewportWidget;

class EditorMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit EditorMainWindow(EditorQtHost* host, QWidget* parent = nullptr);

public slots:
    void AppendLine(const QString& line); // вызывается из потока движка (QueuedConnection)

private slots:
    void RefreshStats();
    
protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void BuildUi();
    void BuildToolBar();
    void UpdatePlayButtons();
    void StartExternalGame();
    void StopExternalGame();
    QToolBar* toolBar_ = nullptr;
    QAction* actPlay_ = nullptr;
    QAction* actPause_ = nullptr;
    QAction* actStop_ = nullptr;
    QAction* actExternal_ = nullptr;
    QProcess* gameProc_ = nullptr;
    QLabel* playStateLabel_ = nullptr;
    EditorQtHost* host_ = nullptr;
    QPlainTextEdit* console_ = nullptr;
    QLabel* statsLabel_ = nullptr;
    QDockWidget* consoleDock_ = nullptr;
    FileBrowserDock* fileDock_ = nullptr;
    ViewportWidget* viewport_ = nullptr;
};
