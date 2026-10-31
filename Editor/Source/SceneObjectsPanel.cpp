#include "SceneObjectsPanel.h"
#include "QtHost.h"

#include <QVBoxLayout>
#include <QGroupBox>
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

    auto* placement = new QGroupBox(QStringLiteral("Добавление объекта"), this);
    auto* placementLayout = new QVBoxLayout(placement);
    layout->addWidget(placement);
    typeCategory_ = new QComboBox(placement);
    typeCategory_->setObjectName(QStringLiteral("ObjectCategory"));
    typeCategory_->addItem(QStringLiteral("Примитивы"), "Primitive.");
    typeCategory_->addItem(QStringLiteral("Свет"), "Light.");
    typeCategory_->addItem(QStringLiteral("Частицы"), "Particle.");
    typeCategory_->addItem(QStringLiteral("Окружение"), "Environment.");
    typeCategory_->addItem(QStringLiteral("Персонажи"), "Gameplay.Character");
    typeCategory_->addItem(QStringLiteral("Камеры"), "Gameplay.Camera");
    typeCategory_->addItem(QStringLiteral("Классы проекта"), "project");
    typeCategory_->addItem(QStringLiteral("Все объекты"), "all");
    connect(typeCategory_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { FillTypes(); });
    placementLayout->addWidget(new QLabel(QStringLiteral("Категория"), placement));
    placementLayout->addWidget(typeCategory_);
    placementLayout->addWidget(new QLabel(QStringLiteral("Объект из категории"), placement));
    placement->setToolTip(QStringLiteral("Категория меняет список объектов для добавления. Выбранный объект сцены не изменяется."));
    primitive_ = new QComboBox(this);
    primitive_->setObjectName(QStringLiteral("PrimitiveType"));
    placementLayout->addWidget(primitive_);

    placementLayout->addWidget(new QLabel(QStringLiteral("Точка добавления — X / Y / Z"), this));

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
    placementLayout->addLayout(spawnRow);

    addPrimitiveButton_ = new QPushButton(QStringLiteral("+ Добавить объект"), this);
    addPrimitiveButton_->setObjectName(QStringLiteral("AddPrimitive"));
    placementLayout->addWidget(addPrimitiveButton_);

    hierarchy_ = new QTreeWidget(this);
    hierarchy_->setHeaderLabels({QStringLiteral("Объект"), QStringLiteral("Тип")});
    hierarchy_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    hierarchy_->setStyleSheet(QStringLiteral("QTreeView::item:selected { background: #a66008; color: white; }"));
    hierarchy_->setContextMenuPolicy(Qt::ActionsContextMenu);
    auto* duplicate = new QAction(QStringLiteral("Дублировать объект"), hierarchy_);
    auto* remove = new QAction(QStringLiteral("Удалить объект"), hierarchy_);
    hierarchy_->addAction(duplicate); hierarchy_->addAction(remove);
    connect(duplicate, &QAction::triggered, this, &SceneObjectsPanel::DuplicateRequested);
    connect(remove, &QAction::triggered, this, &SceneObjectsPanel::RemoveRequested);

    // Delete работает только в иерархии, чтобы не мешать вводу текста в полях.
    removeShortcut_ = new QAction(this);
    removeShortcut_->setShortcut(QKeySequence::Delete);
    removeShortcut_->setShortcutContext(Qt::WidgetShortcut);
    hierarchy_->addAction(removeShortcut_);

    layout->addWidget(hierarchy_, 1);

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

    connect(removeShortcut_, &QAction::triggered, this, &SceneObjectsPanel::RemoveRequested);

    connect(hierarchy_, &QTreeWidget::itemSelectionChanged, this, [this]
    {
        if (suppressSelectionChanged_)
            return;

        const auto items = hierarchy_->selectedItems();
        const quint64 objectId = items.isEmpty()
                                     ? 0
                                     : items.front()->data(0, Qt::UserRole).toULongLong();

        QVector<quint64> ids; for (auto* item : items) ids.push_back(item->data(0, Qt::UserRole).toULongLong());
        const auto active = hierarchy_->currentItem();
        emit SelectionChanged(active && active->isSelected() ? active->data(0, Qt::UserRole).toULongLong() : objectId, ids);
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

    types_ = types;
    primitiveKey_ = key;
    FillTypes();
}

void SceneObjectsPanel::FillTypes()
{
    const auto previous = primitive_->currentData();
    const auto category = typeCategory_->currentData().toString();
    QSignalBlocker blocker(primitive_);
    primitive_->clear();
    for (const auto& type : types_) {
        const auto& name = type.second;
        if (name == "Gameplay.PlaySettings") continue;
        const bool standard = name.startsWith("Primitive.") || name.startsWith("Light.") || name.startsWith("Particle.") || name.startsWith("Environment.") || name.startsWith("Gameplay.");
        if (category == "all" || name.startsWith(category) || (category == "project" && !standard))
            primitive_->addItem(type.first, name);
    }
    const int selected = primitive_->findData(previous);
    if (selected >= 0) primitive_->setCurrentIndex(selected);
    addPrimitiveButton_->setEnabled(primitive_->count() != 0);
}

void SceneObjectsPanel::SetShowServiceObjects(bool visible)
{
    showServiceObjects_ = visible;
    hierarchyKey_.clear();
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
    hierarchy_->setSelectionMode(view.batchEditing ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);

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
            if (!showServiceObjects_ && (object.gameplayKind == 3 || object.type == "Gameplay.PlaySettings")) continue;
            auto* item = new QTreeWidgetItem({
                QString::fromStdString(object.name),
                QString::fromStdString(object.type)
            });

            item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(object.id));
            items[object.id] = item;
        }

        for (const auto& object : view.objects)
        {
            if (!items.count(object.id)) continue;
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
            std::find(view.selectedIds.begin(), view.selectedIds.end(), (*it)->data(0, Qt::UserRole).toULongLong()) != view.selectedIds.end()
        );
    }

    suppressSelectionChanged_ = false;
}
