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
#include <QLineEdit>
#include <QToolButton>
#include <QMenu>
#include <QDropEvent>

#include <map>

class SceneOutlinerTree : public QTreeWidget
{
public:
    explicit SceneOutlinerTree(QWidget* parent = nullptr) : QTreeWidget(parent)
    {
        setDragEnabled(true);
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setDragDropMode(QAbstractItemView::InternalMove);
    }

    std::function<void(uint64_t childId, uint64_t parentId)> onParentChanged;

protected:
    void dropEvent(QDropEvent* event) override
    {
        QTreeWidgetItem* draggedItem = currentItem();
        if (!draggedItem) {
            QTreeWidget::dropEvent(event);
            return;
        }

        uint64_t childId = draggedItem->data(0, Qt::UserRole).toULongLong();

        QTreeWidget::dropEvent(event);

        uint64_t parentId = 0;
        if (draggedItem->parent()) {
            parentId = draggedItem->parent()->data(0, Qt::UserRole).toULongLong();
        }

        if (childId && onParentChanged) {
            onParentChanged(childId, parentId);
        }
    }
};

SceneObjectsPanel::SceneObjectsPanel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(80, 60);
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
    if (searchEdit_) searchEdit_->setPlaceholderText(Tr("Поиск объектов...", "Search objects..."));
    if (addBtn_) addBtn_->setText(Tr("+ Добавить", "+ Add"));
    if (newFolderBtn_) {
        newFolderBtn_->setText(Tr("Папка", "Folder"));
        newFolderBtn_->setToolTip(Tr("Создать группу / папку", "Create Empty Group / Folder"));
    }
    RebuildAddMenu();

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
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Modern clean header in one row
    auto* headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(4);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(Tr("Поиск объектов...", "Search objects..."));
    headerRow->addWidget(searchEdit_, 1);

    addBtn_ = new QPushButton(this);
    addBtn_->setText(Tr("+ Добавить", "+ Add"));
    addMenu_ = new QMenu(addBtn_);
    addBtn_->setMenu(addMenu_);
    headerRow->addWidget(addBtn_);

    newFolderBtn_ = new QPushButton(this);
    newFolderBtn_->setText(Tr("Папка", "Folder"));
    newFolderBtn_->setToolTip(Tr("Создать группу / папку", "Create Empty Group / Folder"));
    headerRow->addWidget(newFolderBtn_);

    layout->addLayout(headerRow);

    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        FilterHierarchy(text);
    });

    connect(newFolderBtn_, &QPushButton::clicked, this, [this] {
        QString groupType = QStringLiteral("Primitive.Cube");
        for (const auto& t : types_) {
            if (t.second.startsWith(QStringLiteral("Primitive."))) {
                groupType = t.second;
                break;
            }
        }
        emit AddPrimitiveRequested(groupType, Tr("Группа", "Group"), SpawnPosition());
    });

    placementGroup_ = new QGroupBox(Tr("Добавление объекта", "Add Object"), this);
    auto* placementLayout = new QVBoxLayout(placementGroup_);
    layout->addWidget(placementGroup_);
    placementGroup_->hide(); // Hidden from modern UI, preserved for EditorUiTests

    typeCategory_ = new QComboBox(placementGroup_);
    typeCategory_->setMinimumWidth(40);
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
    primitive_->setMinimumWidth(40);
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
        spawn_[i]->setMinimumWidth(40);
        spawnRow->addWidget(spawn_[i]);
    }

    spawn_[0]->setValue(2);
    placementLayout->addLayout(spawnRow);

    addPrimitiveButton_ = new QPushButton(Tr("+ Добавить объект", "+ Add Object"), this);
    addPrimitiveButton_->setObjectName(QStringLiteral("AddPrimitive"));
    placementLayout->addWidget(addPrimitiveButton_);

    auto* outlinerTree = new SceneOutlinerTree(this);
    outlinerTree->onParentChanged = [this](uint64_t childId, uint64_t parentId) {
        emit SetParentRequested(childId, parentId);
    };
    hierarchy_ = outlinerTree;
    hierarchy_->setMinimumSize(60, 50);
    hierarchy_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    hierarchy_->setHeaderLabels({Tr("Объект", "Object"), Tr("Тип", "Type")});
    hierarchy_->setSelectionMode(QAbstractItemView::SingleSelection);
    hierarchy_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(hierarchy_, &QTreeWidget::customContextMenuRequested, this, &SceneObjectsPanel::ShowTreeContextMenu);

    actDuplicate_ = new QAction(Tr("Дублировать объект", "Duplicate Object"), hierarchy_);
    actDuplicate_->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    actDuplicate_->setShortcutContext(Qt::WidgetShortcut);
    hierarchy_->addAction(actDuplicate_);
    connect(actDuplicate_, &QAction::triggered, this, &SceneObjectsPanel::DuplicateRequested);

    actRemove_ = new QAction(Tr("Удалить объект", "Remove Object"), hierarchy_);
    actRemove_->setShortcut(QKeySequence::Delete);
    actRemove_->setShortcutContext(Qt::WidgetShortcut);
    hierarchy_->addAction(actRemove_);
    connect(actRemove_, &QAction::triggered, this, &SceneObjectsPanel::RemoveRequested);

    auto* actRename = new QAction(Tr("Переименовать (F2)", "Rename (F2)"), hierarchy_);
    actRename->setShortcut(QKeySequence(Qt::Key_F2));
    actRename->setShortcutContext(Qt::WidgetShortcut);
    hierarchy_->addAction(actRename);
    connect(actRename, &QAction::triggered, this, [this] {
        if (hierarchy_->currentItem()) hierarchy_->editItem(hierarchy_->currentItem(), 0);
    });

    auto* actFocus = new QAction(Tr("Фокус (F)", "Focus (F)"), hierarchy_);
    actFocus->setShortcut(QKeySequence(Qt::Key_F));
    actFocus->setShortcutContext(Qt::WidgetShortcut);
    hierarchy_->addAction(actFocus);
    connect(actFocus, &QAction::triggered, this, &SceneObjectsPanel::FocusRequested);

    connect(hierarchy_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int col) {
        if (col == 0 && !suppressSelectionChanged_) {
            uint64_t id = item->data(0, Qt::UserRole).toULongLong();
            if (id && !item->text(0).trimmed().isEmpty()) {
                emit RenameRequested(id, item->text(0).trimmed());
            }
        }
    });

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

    removeShortcut_ = actRemove_;
    if (removeShortcut_)
    {
        connect(removeShortcut_, &QAction::triggered, this, [this]
        {
            emit RemoveRequested();
        });
    }

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
    RebuildAddMenu();
}

void SceneObjectsPanel::RebuildAddMenu()
{
    if (!addMenu_) return;
    addMenu_->clear();

    auto* menuPrimitives = addMenu_->addMenu(Tr("Примитивы", "Primitives"));
    auto* menuLights = addMenu_->addMenu(Tr("Свет", "Lights"));
    auto* menuParticles = addMenu_->addMenu(Tr("Частицы", "Particles"));
    auto* menuEnv = addMenu_->addMenu(Tr("Окружение", "Environment"));
    auto* menuCharacters = addMenu_->addMenu(Tr("Персонажи", "Characters"));
    auto* menuCameras = addMenu_->addMenu(Tr("Камеры", "Cameras"));
    auto* menuProject = addMenu_->addMenu(Tr("Классы проекта", "Project Classes"));

    for (const auto& type : types_) {
        const auto& name = type.second;
        const auto& label = type.first;
        if (name == "Gameplay.PlaySettings") continue;

        QMenu* target = menuProject;
        if (name.startsWith("Primitive.")) target = menuPrimitives;
        else if (name.startsWith("Light.")) target = menuLights;
        else if (name.startsWith("Particle.")) target = menuParticles;
        else if (name.startsWith("Environment.")) target = menuEnv;
        else if (name.startsWith("Gameplay.Character")) target = menuCharacters;
        else if (name.startsWith("Gameplay.Camera")) target = menuCameras;

        target->addAction(label, this, [this, name, label] {
            emit AddPrimitiveRequested(name, label, SpawnPosition());
        });
    }

    // Hide empty submenus
    for (auto* sub : {menuPrimitives, menuLights, menuParticles, menuEnv, menuCharacters, menuCameras, menuProject}) {
        if (sub->actions().isEmpty()) {
            sub->menuAction()->setVisible(false);
        }
    }
}

void SceneObjectsPanel::FilterHierarchy(const QString& query)
{
    const QString filter = query.trimmed();
    for (QTreeWidgetItemIterator it(hierarchy_); *it; ++it)
    {
        QTreeWidgetItem* item = *it;
        if (filter.isEmpty())
        {
            item->setHidden(false);
        }
        else
        {
            bool match = item->text(0).contains(filter, Qt::CaseInsensitive) ||
                         item->text(1).contains(filter, Qt::CaseInsensitive);
            item->setHidden(!match);
            if (match)
            {
                QTreeWidgetItem* p = item->parent();
                while (p)
                {
                    p->setHidden(false);
                    p->setExpanded(true);
                    p = p->parent();
                }
            }
        }
    }
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
    if (addPrimitiveButton_) addPrimitiveButton_->setEnabled(primitive_->count() != 0);
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
            item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

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

    if (searchEdit_ && !searchEdit_->text().isEmpty())
    {
        FilterHierarchy(searchEdit_->text());
    }

    suppressSelectionChanged_ = false;
}

void SceneObjectsPanel::ShowTreeContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = hierarchy_->itemAt(pos);
    const uint64_t selectedId = item ? item->data(0, Qt::UserRole).toULongLong() : 0;
    if (item) {
        hierarchy_->setCurrentItem(item);
    }

    QMenu menu(this);

    // 1. Создать внутри... / Add Child...
    auto* addChildMenu = menu.addMenu(Tr("Создать внутри…", "Add Child…"));
    auto* menuPrimitives = addChildMenu->addMenu(Tr("Примитивы", "Primitives"));
    auto* menuLights = addChildMenu->addMenu(Tr("Свет", "Lights"));
    auto* menuParticles = addChildMenu->addMenu(Tr("Частицы", "Particles"));
    auto* menuEnv = addChildMenu->addMenu(Tr("Окружение", "Environment"));
    auto* menuCharacters = addChildMenu->addMenu(Tr("Персонажи", "Characters"));
    auto* menuCameras = addChildMenu->addMenu(Tr("Камеры", "Cameras"));
    auto* menuProject = addChildMenu->addMenu(Tr("Классы проекта", "Project Classes"));

    for (const auto& type : types_) {
        const auto& name = type.second;
        const auto& label = type.first;
        if (name == "Gameplay.PlaySettings") continue;

        QMenu* target = menuProject;
        if (name.startsWith("Primitive.")) target = menuPrimitives;
        else if (name.startsWith("Light.")) target = menuLights;
        else if (name.startsWith("Particle.")) target = menuParticles;
        else if (name.startsWith("Environment.")) target = menuEnv;
        else if (name.startsWith("Gameplay.Character")) target = menuCharacters;
        else if (name.startsWith("Gameplay.Camera")) target = menuCameras;

        target->addAction(label, this, [this, selectedId, name, label] {
            emit AddChildPrimitiveRequested(selectedId, name, label, SpawnPosition());
        });
    }
    for (auto* sub : {menuPrimitives, menuLights, menuParticles, menuEnv, menuCharacters, menuCameras, menuProject}) {
        if (sub->actions().isEmpty()) sub->menuAction()->setVisible(false);
    }

    // 2. Создать группу (Пустой узел) / Create Empty Group
    menu.addAction(Tr("Создать группу (Пустой узел)", "Create Empty Group"), this, [this, selectedId] {
        QString groupType = QStringLiteral("Primitive.Cube");
        for (const auto& t : types_) {
            if (t.second.startsWith(QStringLiteral("Primitive."))) {
                groupType = t.second;
                break;
            }
        }
        if (selectedId) {
            emit AddChildPrimitiveRequested(selectedId, groupType, Tr("Группа", "Group"), SpawnPosition());
        } else {
            emit AddPrimitiveRequested(groupType, Tr("Группа", "Group"), SpawnPosition());
        }
    });

    menu.addSeparator();

    // 3. Сделать родителем / Set Parent & Открепить в корень / Unparent
    if (selectedId) {
        if (item && item->parent()) {
            menu.addAction(Tr("Открепить в корень", "Unparent (Clear Parent)"), this, [this, selectedId] {
                emit SetParentRequested(selectedId, 0);
            });
        }

        auto* setParentMenu = menu.addMenu(Tr("Назначить родителем…", "Set Parent To…"));
        setParentMenu->addAction(Tr("Корень сцены (без родителя)", "Scene Root (None)"), this, [this, selectedId] {
            emit SetParentRequested(selectedId, 0);
        });
        setParentMenu->addSeparator();

        for (QTreeWidgetItemIterator it(hierarchy_); *it; ++it) {
            uint64_t candidateId = (*it)->data(0, Qt::UserRole).toULongLong();
            if (candidateId && candidateId != selectedId) {
                QString candidateName = (*it)->text(0);
                setParentMenu->addAction(candidateName, this, [this, selectedId, candidateId] {
                    emit SetParentRequested(selectedId, candidateId);
                });
            }
        }
    }

    menu.addSeparator();

    // 4. Фокус, Дублировать, Переименовать, Удалить
    auto* actFoc = menu.addAction(Tr("Фокус на объекте (F)", "Focus Selected (F)"), this, &SceneObjectsPanel::FocusRequested);
    actFoc->setEnabled(selectedId != 0);

    auto* actDup = menu.addAction(Tr("Дублировать (Ctrl+D)", "Duplicate (Ctrl+D)"), this, &SceneObjectsPanel::DuplicateRequested);
    actDup->setEnabled(selectedId != 0);

    auto* actRen = menu.addAction(Tr("Переименовать (F2)", "Rename (F2)"), this, [this, item] {
        if (item) hierarchy_->editItem(item, 0);
    });
    actRen->setEnabled(selectedId != 0);

    menu.addSeparator();

    auto* actDel = menu.addAction(Tr("Удалить (Del)", "Delete (Del)"), this, &SceneObjectsPanel::RemoveRequested);
    actDel->setEnabled(selectedId != 0);

    menu.exec(hierarchy_->viewport()->mapToGlobal(pos));
}
