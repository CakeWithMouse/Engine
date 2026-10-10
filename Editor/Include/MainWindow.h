#pragma once
#include <QMainWindow>

class QLabel;
class QPlainTextEdit;
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

private:
    void BuildUi();
    EditorQtHost* host_ = nullptr;
    QPlainTextEdit* console_ = nullptr;
    QLabel* statsLabel_ = nullptr;
    QDockWidget* consoleDock_ = nullptr;
    FileBrowserDock* fileDock_ = nullptr;
    ViewportWidget* viewport_ = nullptr;
};