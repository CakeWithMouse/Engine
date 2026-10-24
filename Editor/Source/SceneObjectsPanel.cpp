#include "SceneObjectsPanel.h"
#include "QtHost.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QAction>
#include <QKeySequence>
#include <QSignalBlocker>
#include <QTreeWidgetItemIterator>
#include <QVariant>

#include <map>

SceneObjectsPanel::SceneObjectsPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildUi();
}

void SceneObjectsPanel::BuildUi()
{
    auto* layout = new QVBoxLayout(this);

    primitive_ = new QComboBox(this);
    primitive_->setObjectName(QStringLiteral("PrimitiveType"));
    layout->addWidget(primitive_);

    layout->addWidget(new QLabel(QStringLiteral("Точка добавления — X / Y / Z"), this));

    auto* spawnRow = new QHBoxLayout;
    for (int i = 0; i < 3; ++i)
    {
        spawn_[i] = new QDoubleSpinBox(this);
        spawn_[i]->setRange(-10000, 10000);
        spawn_[i]->setDecimals(2);
        spawn_[i]->setSingleStep(0.5);
        spawnRow->addWidget(spawn_[i]);
    }

    spawn_[0]->setValue(2);
    layout->addLayout(spawnRow);

    addPrimitiveButton_ = new QPushButton(QStringLiteral("+ Добавить объект"), this);
    addPrimitiveButton_->setObjectName(QStringLiteral("AddPrimitive"));
    layout->addWidget(addPrimitiveButton_);

    addModelButton_ = new QPushButton(QStringLiteral("+ Добавить модель…"), this);
    addModelButton_->setObjectName(QStringLiteral("AddModel"));
    addModelButton_->setToolTip(QStringLiteral(
        "Выбрать модель из Content/Models и разместить в точке добавления X/Y/Z."
    ));
    layout->addWidget(addModelButton_);

    importModelButton_ = new QPushButton(QStringLiteral("Импорт модели в проект…"), this);
    importModelButton_->setObjectName(QStringLiteral("SceneImportModel"));
    importModelButton_->setToolTip(QStringLiteral(
        "Скопировать внешний файл в проект. Затем нажмите «Добавить модель…»."
    ));
    layout->addWidget(importModelButton_);

    hierarchy_ = new QTreeWidget(this);
    hierarchy_->setHeaderLabels({QStringLiteral("Объект"), QStringLiteral("Тип")});
    hierarchy_->setSelectionMode(QAbstractItemView::SingleSelection);

    // Delete работает только в иерархии, чтобы не мешать вводу текста в полях.
    removeShortcut_ = new QAction(this);
    removeShortcut_->setShortcut(QKeySequence::Delete);
    removeShortcut_->setShortcutContext(Qt::WidgetShortcut);
    hierarchy_->addAction(removeShortcut_);

    layout->addWidget(hierarchy_, 1);

    auto* buttons = new QHBoxLayout;
    duplicateButton_ = new QPushButton(QStringLiteral("Дубликат"), this);
    removeButton_ = new QPushButton(QStringLiteral("Удалить"), this);
    buttons->addWidget(duplicateButton_);
    buttons->addWidget(removeButton_);
    layout->addLayout(buttons);

    auto* help = new QLabel(QStringLiteral(
        "ПКМ + мышь: обзор; WASD, Q/E: камера.\n"
        "ЛКМ/Tab: выбор; F: фокус камеры.\n"
        "Стрелки, PgUp/PgDn: объект.\n"
        "Ctrl+S: сохранить; Ctrl+Z: отменить.\n"
        "Play проверяет копию сцены; Stop возвращает правки."
    ), this);
    help->setWordWrap(true);
    layout->addWidget(help);

    connect(addPrimitiveButton_, &QPushButton::clicked, this, [this]
    {
        if (primitive_->currentIndex() < 0)
            return;

        std::array<double, 3> position{};
        for (int i = 0; i < 3; ++i)
            position[i] = spawn_[i]->value();

        emit AddPrimitiveRequested(
            primitive_->currentData().toString(),
            primitive_->currentText(),
            position
        );
    });

    connect(addModelButton_, &QPushButton::clicked, this, &SceneObjectsPanel::AddModelRequested);
    connect(importModelButton_, &QPushButton::clicked, this, &SceneObjectsPanel::ImportModelRequested);
    connect(duplicateButton_, &QPushButton::clicked, this, &SceneObjectsPanel::DuplicateRequested);
    connect(removeButton_, &QPushButton::clicked, this, &SceneObjectsPanel::RemoveRequested);

    connect(removeShortcut_, &QAction::triggered, this, &SceneObjectsPanel::RemoveRequested);

    connect(hierarchy_, &QTreeWidget::itemSelectionChanged, this, [this]
    {
        if (suppressSelectionChanged_)
            return;

        const auto items = hierarchy_->selectedItems();
        const quint64 objectId = items.isEmpty()
                                     ? 0
                                     : items.front()->data(0, Qt::UserRole).toULongLong();

        emit SelectionChanged(objectId);
    });

    connect(hierarchy_, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem*, int)
            {
                emit FocusRequested();
            });
}

void SceneObjectsPanel::SetEnabled(bool enabled)
{
    setEnabled(enabled);
}

void SceneObjectsPanel::SetPrimitiveTypes(const QVector<std::pair<QString, QString>>& types)
{
    QString key;
    key.reserve(types.size() * 32);

    for (const auto& type : types)
    {
        key += type.first;
        key += QLatin1Char('|');
        key += type.second;
        key += QLatin1Char(';');
    }

    if (key == primitiveKey_)
        return;

    QSignalBlocker blocker(primitive_);

    primitive_->clear();

    for (const auto& type : types)
        primitive_->addItem(type.first, type.second);

    primitiveKey_ = key;
}

std::array<double, 3> SceneObjectsPanel::SpawnPosition() const
{
    std::array<double, 3> position{};

    for (int i = 0; i < 3; ++i)
    {
        if (spawn_[i])
            position[i] = spawn_[i]->value();
    }

    return position;
}

void SceneObjectsPanel::RefreshHierarchy(const EditorQtHost::Snapshot& view)
{
    suppressSelectionChanged_ = true;

    QString key;
    key.reserve(static_cast<int>(view.objects.size()) * 64);

    for (const auto& object : view.objects)
    {
        key += QString::number(object.id);
        key += QLatin1Char('/');
        key += QString::number(object.parent);
        key += QLatin1Char('/');
        key += QString::number(object.name.size());
        key += QLatin1Char(':');
        key += QString::fromStdString(object.name);
        key += QLatin1Char(';');
    }

    if (key != hierarchyKey_)
    {
        hierarchy_->clear();

        std::map<quint64, QTreeWidgetItem*> items;

        for (const auto& object : view.objects)
        {
            auto* item = new QTreeWidgetItem({
                QString::fromStdString(object.name),
                QString::fromStdString(object.type)
            });

            item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(object.id));
            items[object.id] = item;
        }

        for (const auto& object : view.objects)
        {
            if (object.parent && items.count(object.parent))
                items[object.parent]->addChild(items[object.id]);
            else
                hierarchy_->addTopLevelItem(items[object.id]);
        }

        hierarchy_->expandAll();
        hierarchyKey_ = key;
    }

    for (QTreeWidgetItemIterator it(hierarchy_); *it; ++it)
    {
        (*it)->setSelected(
            (*it)->data(0, Qt::UserRole).toULongLong() == view.selected
        );
    }

    suppressSelectionChanged_ = false;
}
