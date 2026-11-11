#pragma once

#include "CodeGraphSchema.h"
#include "QtHost.h"

#include <QHash>
#include <QMainWindow>
#include <QString>
#include <QVector>
#include <memory>

class QLabel;
class QListWidget;
class QTableWidget;
class QTimer;
class ScriptGraphWidget;

// Окно одного визуального скрипта (.vscript). Без Q_OBJECT: найти окно можно через EditorMainWindow::ScriptEditor.
//
// Каталог нод и проверка графа приходят из игрового модуля (расширение Engine.Scene.Script), поэтому
// редактор и Play собирают граф одним компилятором. Каждая правка сразу записывается в файл (как граф
// материала) и проверяется; ошибки показываются на нодах и в списке под холстом. Во время Play окно
// показывает ошибки выполнения этого скрипта на объектах сцены.
class ScriptEditorWindow : public QMainWindow
{
public:
    // script: /Game путь .vscript
    ScriptEditorWindow(EditorQtHost* host, const QString& script, QWidget* parent = nullptr);
    ~ScriptEditorWindow() override;

    const QString& Script() const { return script_; }
    ScriptGraphWidget* Graph() const { return graph_; }
    bool IsReady() const { return ready_; }           // каталог и файл загружены
    bool IsReadOnly() const { return readOnly_; }
    const QString& Status() const { return status_; } // почему окно только для чтения
    // Ошибки сборки после последней проверки и ошибки Play: нода -> текст.
    QHash<int, QString> Problems() const;
    int CheckCount() const { return checks_; }        // сколько проверок файла завершилось

    // Записать файл и проверить его движком сразу, не дожидаясь паузы в правках.
    void SaveNow();

private:
    struct Problem
    {
        int node = 0;
        QString object; // объект сцены для ошибок Play
        QString message;
        bool runtime = false;
    };

    void BuildUi();
    void LoadCatalog();
    void LoadFile(const QString& catalog);
    void SetReadOnly(const QString& reason);
    bool WriteFile();
    void Check();
    void PollPlayProblems();
    void ShowProblems();
    void RefreshVariables();
    void EditVariables(int row, int column);
    void AddVariable();
    void RemoveVariable();
    QString PhysicalPath() const;

    EditorQtHost* host_ = nullptr;
    QString script_;
    std::unique_ptr<EditorCore::CodeGraphSchema> schema_;
    ScriptGraphWidget* graph_ = nullptr;
    QTableWidget* variables_ = nullptr;
    QListWidget* problemList_ = nullptr;
    QLabel* banner_ = nullptr;
    QTimer* saveTimer_ = nullptr;
    QTimer* playTimer_ = nullptr;
    QVector<Problem> checkProblems_, playProblems_;
    QString status_;
    bool ready_ = false, readOnly_ = false, refreshingVariables_ = false;
    int checks_ = 0;
};
