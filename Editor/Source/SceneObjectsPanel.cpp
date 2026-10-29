#include "SceneObjectsPanel.h"
#include "QtHost.h"
#include "EditorLocale.h"

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
    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
        RetranslateUi();
    });
}

SceneObjectsPanel::~SceneObjectsPanel()
{
    if (langSubId_) {
        EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        langSubId_ = 0;
    }
}

void SceneObjectsPanel::RetranslateUi()
{
    if (placementGroup_) placementGroup_->setTitle(Tr("Добавление объекта", "Add Object"));
    if (categoryLabel_) categoryLabel_->setText(Tr("Категория", "Category"));
    if (objectLabel_) objectLabel_->setText(Tr("Объект из категории", "Category Object"));
    if (spawnPointLabel_) spawnPointLabel_->setText(Tr("Точка добавления — X / Y / Z", "Spawn Position — X / Y / Z"));
    if (addPrimitiveButton_) addPrimitiveButton_->setText(Tr("+ Добавить объект", "+ Add Object"));
    if (hierarchy_) hierarchy_->setHeaderLabels({Tr("Объект", "Object"), Tr("Тип", "Type")});
    if (actDuplicate_) actDuplicate_->setText(Tr("Дублировать объект", "Duplicate Object"));
    if (actRemove_) actRemove_->setText(Tr("Удалить объект", "Remove Object"));

    if (typeCategory_) {
        const int cur = typeCategory_->currentIndex();
        typeCategory_->setItemText(0, Tr("Примитивы", "Primitives"));
        typeCategory_->setItemText(1, Tr("Свет", "Lights"));
        typeCategory_->setItemText(2, Tr("Частицы", "Particles"));
        typeCategory_->setItemText(3, Tr("Окружение", "Environment"));
        typeCategory_->setItemText(4, Tr("Персонажи", "Characters"));
        typeCategory_->setItemText(5, Tr("Камеры", "Cameras"));
        typeCategory_->setItemText(6, Tr("Классы проекта", "Project Classes"));
        typeCategory_->setItemText(7, Tr("Все объекты", "All Objects"));
        typeCategory_->setCurrentIndex(cur);
    }
    FillTypes();
}

void SceneObjectsPanel::BuildUi()
{
    auto* layout = new QVBoxLayout(this);

    placementGroup_ = new QGroupBox(Tr("Добавление объекта", "Add Object"), this);
    auto* placementLayout = new QVBoxLayout(placementGroup_);
    layout->addWidget(placementGroup_);
    typeCategory_ = new QComboBox(placementGroup_);
    typeCategory_->setObjectName(QStringLiteral("ObjectCategory"));
    typeCategory_->addItem(Tr("Примитивы", "Primitives"), "Primitive.");
    typeCategory_->addItem(Tr("Свет", "Lights"), "Light.");
    typeCategory_->addItem(Tr("Частицы", "Particles"), "Particle.");
    typeCategory_->addItem(Tr("Окружение", "Environment"), "Environment.");
    typeCategory_->addItem(Tr("Персонажи", "Characters"), "Gameplay.Character");
    typeCategory_->addItem(Tr("Камеры", "Cameras"), "Gameplay.Camera");
    typeCategory_->addItem(Tr("Классы проекта", "Project Classes"), "project");
    typeCategory_->addItem(Tr("Все объекты", "All Objects"), "all");
    connect(typeCategory_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { FillTypes(); });
    categoryLabel_ = new QLabel(Tr("Категория", "Category"), placementGroup_);
    placementLayout->addWidget(categoryLabel_);
    placementLayout->addWidget(typeCategory_);
    objectLabel_ = new QLabel(Tr("Объект из категории", "Category Object"), placementGroup_);
    placementLayout->addWidget(objectLabel_);
    placementGroup_->setToolTip(Tr("Категория меняет список объектов для добавления. Выбранный объект сцены не изменяется.",
                                  "Category changes the list of spawnable objects. Selected scene object is unchanged."));
    primitive_ = new QComboBox(this);
    primitive_->setObjectName(QStringLiteral("PrimitiveType"));
    placementLayout->addWidget(primitive_);

    spawnPointLabel_ = new QLabel(Tr("Точка добавления — X / Y / Z", "Spawn Position — X / Y / Z"), this);
    placementLayout->addWidget(spawnPointLabel_);

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

    addPrimitiveButton_ = new QPushButton(Tr("+ Добавить объект", "+ Add Object"), this);
    addPrimitiveButton_->setObjectName(QStringLiteral("AddPrimitive"));
    placementLayout->addWidget(addPrimitiveButton_);

    hierarchy_ = new QTreeWidget(this);
    hierarchy_->setHeaderLabels({Tr("Объект", "Object"), Tr("Тип", "Type")});
    hierarchy_->setSelectionMode(QAbstractItemView::SingleSelection);
    hierarchy_->setContextMenuPolicy(Qt::ActionsContextMenu);
    actDuplicate_ = new QAction(Tr("Дублировать объект", "Duplicate Object"), hierarchy_);
    actRemove_ = new QAction(Tr("Удалить объект", "Remove Object"), hierarchy_);
    hierarchy_->addAction(actDuplicate_); hierarchy_->addAction(actRemove_);
    connect(actDuplicate_, &QAction::triggered, this, &SceneObjectsPanel::DuplicateRequested);
    connect(actRemove_, &QAction::triggered, this, &SceneObjectsPanel::RemoveRequested);

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

    connect(removeShortcut_, &QAction::triggered, this, [this]
    {
        emit RemoveRequested();
    });

    connect(
        hierarchy_,
        &QTreeWidget::itemSelectionChanged,
        this,
        [this]
        {
            if (suppressSelectionChanged_)
                return;

            const auto items = hierarchy_->selectedItems();
            if (items.isEmpty())
                emit SelectionChanged(0);
            else
                emit SelectionChanged(items.first()->data(0, Qt::UserRole).toULongLong());
        }
    );

    connect(
        hierarchy_,
        &QTreeWidget::itemDoubleClicked,
        this,
        [this](QTreeWidgetItem*, int)
        {
            emit FocusRequested();
        }
    );
}

void SceneObjectsPanel::SetEnabled(bool enabled)
{
    addPrimitiveButton_->setEnabled(enabled);
    primitive_->setEnabled(enabled);

    for (int i = 0; i < 3; ++i)
        spawn_[i]->setEnabled(enabled);

    hierarchy_->setEnabled(enabled);
}

void SceneObjectsPanel::SetPrimitiveTypes(
    const QVector<std::pair<QString, QString>>& types
)
{
    types_ = types;
    FillTypes();
}

void SceneObjectsPanel::FillTypes()
{
    primitive_->clear();
    const QString category = typeCategory_->currentData().toString();
    for (const auto& item : types_)
    {
        const auto& type = item.second;
        bool matches = false;
        if (category == "all") matches = true;
        else if (category == "project") matches = !type.startsWith("Primitive.") && !type.startsWith("Light.") && !type.startsWith("Particle.") && !type.startsWith("Environment.") && !type.startsWith("Gameplay.");
        else if (category.endsWith('.')) matches = type.startsWith(category);
        else matches = type == category;
        if (matches) primitive_->addItem(item.first, item.second);
    }
}

void SceneObjectsPanel::SetShowServiceObjects(bool visible)
{
    showServiceObjects_ = visible;
    hierarchyKey_.clear();
}

std::array<double, 3> SceneObjectsPanel::SpawnPosition() const
{
    std::array<double, 3> pos{};
    for (int i = 0; i < 3; ++i)
        pos[i] = spawn_[i]->value();
    return pos;
}

void SceneObjectsPanel::RefreshHierarchy(const EditorQtHost::Snapshot& view)
{
    suppressSelectionChanged_ = true;

    QString key;
    for (const auto& object : view.objects)
    {
        if (!showServiceObjects_ && (object.gameplayKind == 3 || object.type == "Gameplay.PlaySettings")) continue;
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
            (*it)->data(0, Qt::UserRole).toULongLong() == view.selected
        );
    }

    suppressSelectionChanged_ = false;
}
