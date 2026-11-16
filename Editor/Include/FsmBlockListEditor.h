#pragma once

#include "FsmGraphSchema.h"

#include <QVariantList>
#include <QWidget>
#include <functional>

class QFormLayout;
class QListWidget;
class QPushButton;
class QToolButton;

// Список блоков автомата ИИ одного вида (задачи, условия, службы) с полями параметров выбранного блока.
// Добавление — меню блоков из каталога движка (встроенные, затем блоки игры), порядок — кнопками ↑ ↓.
// Ключи памяти выбираются из переменных автомата подходящего типа. Группы условий «Хотя бы одно из» / «Все из»
// показываются и переставляются, но их состав правится в файле.
class FsmBlockListEditor : public QWidget
{
public:
    // kind: task, condition, service; prefix — начало objectName дочерних виджетов (FsmTasks, FsmEnterIf…).
    FsmBlockListEditor(const EditorCore::FsmGraphSchema& schema, const QString& kind, const QString& prefix, QWidget* parent = nullptr);

    void SetBlocks(const QVariantList& blocks);
    const QVariantList& Blocks() const { return blocks_; }

    // Добавить блок этого типа в конец и выбрать его; false — блока нет в каталоге или он другого вида.
    bool AddBlock(const QString& type);
    void SelectBlock(int row);
    int CurrentRow() const;

    std::function<void()> changed;

private:
    void RefreshList();
    void ShowParams();
    void SetParam(int row, const QString& name, const QVariant& value); // Invalid — убрать (значение по умолчанию)
    void Move(int delta);

    const EditorCore::FsmGraphSchema& schema_;
    QString kind_, prefix_;
    QVariantList blocks_;
    QListWidget* list_ = nullptr;
    QToolButton* add_ = nullptr;
    QPushButton* remove_ = nullptr;
    QPushButton* up_ = nullptr;
    QPushButton* down_ = nullptr;
    QWidget* params_ = nullptr;
    QFormLayout* form_ = nullptr;
    bool refreshing_ = false;
};
