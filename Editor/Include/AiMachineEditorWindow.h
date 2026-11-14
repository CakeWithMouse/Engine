#pragma once

#include "FsmGraphSchema.h"
#include "QtHost.h"

#include <QHash>
#include <QMainWindow>
#include <QString>
#include <QVector>
#include <memory>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QTableWidget;
class QTimer;
class FsmGraphWidget;

// Окно одного автомата ИИ (.fsm). Без Q_OBJECT: найти окно можно через EditorMainWindow::AiMachineEditor.
//
// Каталог блоков и проверка файла приходят из игрового модуля (расширение Engine.Scene.AiMachine), поэтому
// редактор и Play собирают автомат одним загрузчиком. Каждая правка сразу записывается в файл и проверяется;
// ошибки показываются на нодах и в списке под холстом. Слева — память автомата (ключи и начальные значения)
// и настройки персонажа (куда восприятие пишет цель, интервал решений).
class AiMachineEditorWindow : public QMainWindow
{
public:
    // machine: /Game путь .fsm
    AiMachineEditorWindow(EditorQtHost* host, const QString& machine, QWidget* parent = nullptr);
    ~AiMachineEditorWindow() override;

    const QString& Machine() const { return machine_; }
    FsmGraphWidget* Graph() const { return graph_; }
    const EditorCore::FsmGraphSchema& Schema() const { return *schema_; }
    bool IsReady() const { return ready_; }
    bool IsReadOnly() const { return readOnly_; }
    const QString& Status() const { return status_; }
    // Ошибки последней проверки движком: нода -> текст (0 — файл целиком).
    const QHash<int, QString>& Problems() const { return problems_; }
    const QHash<int, QString>& Warnings() const { return warnings_; }
    int CheckCount() const { return checks_; }

    // Записать файл и проверить его движком сразу, не дожидаясь паузы в правках.
    void SaveNow();
    // Память: добавить ключ (имя свободное) и записать её целиком из таблицы.
    void AddVariable();

private:
    void BuildUi();
    void LoadCatalog();
    void LoadFile(const QString& catalog);
    void SetReadOnly(const QString& reason);
    bool WriteFile();
    void Check();
    void ShowProblems();
    void RefreshPanels();
    void EditVariables();
    void RemoveVariable();
    void EditSettings();
    QString PhysicalPath() const;

    EditorQtHost* host_ = nullptr;
    QString machine_;
    std::unique_ptr<EditorCore::FsmGraphSchema> schema_;
    FsmGraphWidget* graph_ = nullptr;
    QTableWidget* variables_ = nullptr;
    QComboBox* targetKey_ = nullptr;
    QComboBox* lastSeenKey_ = nullptr;
    QDoubleSpinBox* thinkInterval_ = nullptr;
    QListWidget* problemList_ = nullptr;
    QLabel* banner_ = nullptr;
    QTimer* saveTimer_ = nullptr;
    QHash<int, QString> problems_, warnings_;
    QString status_, hint_;
    bool ready_ = false, readOnly_ = false, refreshing_ = false;
    int checks_ = 0;
    int writeRetries_ = 0;
};
