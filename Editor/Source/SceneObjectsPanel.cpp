#include "EditorTheme.h"
#include "SceneObjectsPanel.h"
#include "SceneOutlinerTree.h"
#include "EditorLocale.h"
#include "IconProvider.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
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
#include <QMenu>
#include <QGroupBox>
#include <QLabel>
#include <map>

SceneObjectsPanel::SceneObjectsPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildUi();
    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
        RetranslateUi();
    });
    themeSubId_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
        if (searchEdit_) {
            for (auto* a : searchEdit_->actions()) searchEdit_->removeAction(a);
            searchEdit_->addAction(IconProvider::SearchIcon(), QLineEdit::LeadingPosition);
        }
        if (addBtn_) addBtn_->setIcon(IconProvider::PlusIcon());
        hierarchyKey_.clear();
        hierarchy_->viewport()->update();
    });
}

SceneObjectsPanel::~SceneObjectsPanel()
{
    if (langSubId_) {
        EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        langSubId_ = 0;
    }
    if (themeSubId_) {
        EditorTheme::Instance().UnsubscribeThemeChanged(themeSubId_);
        themeSubId_ = 0;
    }
}

void SceneObjectsPanel::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // 1. Поиск и кнопка +Add
    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(4);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setObjectName(QStringLiteral("SceneSearchEdit"));
    searchEdit_->setPlaceholderText(Tr("Поиск объектов…", "Search objects…"));
    searchEdit_->setClearButtonEnabled(true);
    searchEdit_->addAction(IconProvider::SearchIcon(), QLineEdit::LeadingPosition);
    connect(searchEdit_, &QLineEdit::textChanged, this, &SceneObjectsPanel::FilterHierarchy);
    topBar->addWidget(searchEdit_);

    addBtn_ = new QPushButton(Tr("Добавить", "Add"), this);
    addBtn_->setObjectName(QStringLiteral("SceneAddButton"));
    addBtn_->setIcon(IconProvider::PlusIcon());
    addMenu_ = new QMenu(this);
    addBtn_->setMenu(addMenu_);
    RebuildAddMenu();
    topBar->addWidget(addBtn_);

    mainLayout->addLayout(topBar);

    // 2. Дерево иерархии (SceneOutlinerTree с Drag & Drop)
    auto* outlinerTree = new SceneOutlinerTree(this);
    outlinerTree->onParentChanged = [this](uint64_t childId, uint64_t parentId) {
        emit SetParentRequested(childId, parentId);
    };
    hierarchy_ = outlinerTree;
    hierarchy_->setObjectName(QStringLiteral("SceneHierarchy"));
    hierarchy_->setMinimumSize(80, 60);
    hierarchy_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    hierarchy_->setHeaderLabels({Tr("Объект", "Object"), Tr("Тип", "Type")});
    hierarchy_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    hierarchy_->setIndentation(13);
    hierarchy_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(hierarchy_, &QTreeWidget::customContextMenuRequested, this, &SceneObjectsPanel::ShowTreeContextMenu);

    actDuplicate_ = new QAction(Tr("Дублировать объект", "Duplicate Object"), hierarchy_);
    actDuplicate_->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    actDuplicate_->setShortcutContext(Qt::WidgetShortcut);
    actDuplicate_->setIcon(IconProvider::CubeIcon());
    hierarchy_->addAction(actDuplicate_);
    connect(actDuplicate_, &QAction::triggered, this, &SceneObjectsPanel::DuplicateRequested);

    actRemove_ = new QAction(Tr("Удалить объект", "Remove Object"), hierarchy_);
    actRemove_->setShortcut(QKeySequence::Delete);
    actRemove_->setShortcutContext(Qt::WidgetShortcut);
    actRemove_->setIcon(IconProvider::TrashIcon());
    hierarchy_->addAction(actRemove_);
    connect(actRemove_, &QAction::triggered, this, &SceneObjectsPanel::RemoveRequested);

    removeShortcut_ = new QAction(hierarchy_);
    removeShortcut_->setShortcut(QKeySequence(Qt::Key_Backspace));
    removeShortcut_->setShortcutContext(Qt::WidgetShortcut);
    hierarchy_->addAction(removeShortcut_);
    connect(removeShortcut_, &QAction::triggered, this, &SceneObjectsPanel::RemoveRequested);

    connect(hierarchy_, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (suppressSelectionChanged_) return;
        const auto items = hierarchy_->selectedItems();
        const quint64 objectId = items.isEmpty()
                                     ? 0
                                     : items.front()->data(0, Qt::UserRole).toULongLong();

        QVector<quint64> ids; for (auto* item : items) ids.push_back(item->data(0, Qt::UserRole).toULongLong());
        const auto active = hierarchy_->currentItem();
        emit SelectionChanged(active && active->isSelected() ? active->data(0, Qt::UserRole).toULongLong() : objectId, ids);
    });

    connect(hierarchy_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (suppressSelectionChanged_ || column != 0) return;
        const uint64_t id = item->data(0, Qt::UserRole).toULongLong();
        if (id) emit RenameRequested(id, item->text(0));
    });

    mainLayout->addWidget(hierarchy_, 1);
}

void SceneObjectsPanel::RebuildAddMenu()
{
    if (!addMenu_) return;
    qDeleteAll(addMenu_->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly));
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

        auto* action = target->addAction(label, this, [this, name, label] {
            emit AddPrimitiveRequested(name, label, SpawnPosition());
        });
        action->setObjectName("CreateObject." + name);
        action->setData(name);
    }

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

void SceneObjectsPanel::SetEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);
}

void SceneObjectsPanel::SetPrimitiveTypes(const QVector<std::pair<QString, QString>>& types)
{
    if (types_ == types) return;
    types_ = types;
    RebuildAddMenu();
}

void SceneObjectsPanel::SetShowServiceObjects(bool visible)
{
    showServiceObjects_ = visible;
    hierarchyKey_.clear();
}

std::array<double, 3> SceneObjectsPanel::SpawnPosition() const
{
    return {0.0, 0.0, 0.0};
}

static bool IsEnvironmentObject(const IEditorHost::SceneObject& object)
{
    return object.gameplayKind == 3 || object.type == "Gameplay.PlaySettings" ||
           object.type == "Light.Directional" || object.type == "Environment.Sky";
}

void SceneObjectsPanel::RefreshHierarchy(const EditorQtHost::Snapshot& view)
{
    suppressSelectionChanged_ = true;
    hierarchy_->setSelectionMode(view.batchEditing ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);

    QString key;
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
        std::vector<const IEditorHost::SceneObject*> normalObjects;
        std::vector<const IEditorHost::SceneObject*> envObjects;

        for (const auto& object : view.objects)
        {
            if (IsEnvironmentObject(object))
                envObjects.push_back(&object);
            else
                normalObjects.push_back(&object);
        }

        if (!envObjects.empty())
        {
            auto* envGroup = new QTreeWidgetItem(QStringList{Tr("Окружение", "Environment")});
            envGroup->setIcon(0, IconProvider::GetIcon(QStringLiteral("sun")));
            QFont f = envGroup->font(0);
            f.setBold(true);
            envGroup->setFont(0, f);
            envGroup->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            for (const auto* obj : envObjects)
            {
                QString label = QString::fromStdString(obj->name);
                if (obj->type == "Light.Directional") {
                    label = Tr("Направленный свет", "Directional Light");
                } else if (obj->type == "Gameplay.PlaySettings" || obj->gameplayKind == 3) {
                    label = Tr("Настройки игры", "Play Settings");
                } else if (obj->type == "Environment.Sky") {
                    label = Tr("Небо", "Sky");
                }
                auto* envItem = new QTreeWidgetItem(QStringList{label, QString::fromStdString(obj->type)});
                QIcon ic = (obj->type == "Light.Directional") ? IconProvider::GetIcon(QStringLiteral("sun")) :
                           (obj->type == "Environment.Sky") ? IconProvider::GetIcon(QStringLiteral("cloud")) :
                           IconProvider::SettingsIcon();
                envItem->setIcon(0, ic);
                envItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                envItem->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(obj->id));
                envGroup->addChild(envItem);
                items[obj->id] = envItem;
            }
            hierarchy_->addTopLevelItem(envGroup);
            envGroup->setExpanded(true);
        }

        for (const auto* object : normalObjects)
        {
            auto* item = new QTreeWidgetItem(QStringList{
                QString::fromStdString(object->name),
                QString::fromStdString(object->type)
            });
            QIcon objIcon;
            if (object->type.find("Light") != std::string::npos) objIcon = IconProvider::BulbIcon();
            else if (object->type.find("Camera") != std::string::npos) objIcon = IconProvider::GetIcon(QStringLiteral("camera"));
            else if (object->type.find("Particles") != std::string::npos) objIcon = IconProvider::GetIcon(QStringLiteral("particles"));
            else objIcon = IconProvider::CubeIcon();
            item->setIcon(0, objIcon);
            item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
            item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(object->id));
            items[object->id] = item;
        }

        for (const auto* object : normalObjects)
        {
            if (!items.count(object->id)) continue;
            if (object->parent && items.count(object->parent))
                items[object->parent]->addChild(items[object->id]);
            else
                hierarchy_->addTopLevelItem(items[object->id]);
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
    if (selectedId) {
        auto* addChildMenu = menu.addMenu(Tr("Создать внутри…", "Add Child…"));
        for (const auto& type : types_) {
            const auto& name = type.second;
            const auto& label = type.first;
            if (name == "Gameplay.PlaySettings") continue;
            addChildMenu->addAction(label, this, [this, selectedId, name, label] {
                emit AddChildPrimitiveRequested(selectedId, name, label, SpawnPosition());
            });
        }
        menu.addSeparator();

        // 2. Открепить / Назначить родителем
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
        menu.addSeparator();
    }

    if (selectedId == 0)
    {
        auto* addSubMenu = menu.addMenu(Tr("Добавить", "Add"));
        addSubMenu->setIcon(IconProvider::PlusIcon());
        for (auto* act : addMenu_->actions())
        {
            if (act->menu())
            {
                auto* sub = addSubMenu->addMenu(act->icon(), act->text());
                for (auto* subAct : act->menu()->actions())
                    sub->addAction(subAct);
            }
            else
            {
                addSubMenu->addAction(act);
            }
        }
        menu.addSeparator();
        menu.addAction(Tr("Развернуть всё", "Expand All"), hierarchy_, &QTreeWidget::expandAll);
        menu.addAction(Tr("Свернуть всё", "Collapse All"), hierarchy_, &QTreeWidget::collapseAll);
    }
    else
    {
        auto* actFoc = menu.addAction(Tr("Фокус на объекте (F)", "Focus Selected (F)"), this, &SceneObjectsPanel::FocusRequested);
        actFoc->setEnabled(true);

        auto* actDup = menu.addAction(Tr("Дублировать (Ctrl+D)", "Duplicate (Ctrl+D)"), this, &SceneObjectsPanel::DuplicateRequested);
        actDup->setEnabled(true);

        auto* actRen = menu.addAction(Tr("Переименовать (F2)", "Rename (F2)"), this, [this, item] {
            if (item) hierarchy_->editItem(item, 0);
        });
        actRen->setEnabled(true);

        menu.addSeparator();
        auto* actDel = menu.addAction(Tr("Удалить (Delete)", "Delete (Delete)"), this, &SceneObjectsPanel::RemoveRequested);
        actDel->setEnabled(true);
    }

    menu.exec(hierarchy_->viewport()->mapToGlobal(pos));
}

void SceneObjectsPanel::RetranslateUi()
{
    if (searchEdit_) searchEdit_->setPlaceholderText(Tr("Поиск объектов…", "Search objects…"));
    if (addBtn_) addBtn_->setText(Tr("Добавить", "Add"));
    if (hierarchy_) hierarchy_->setHeaderLabels({Tr("Объект", "Object"), Tr("Тип", "Type")});
    if (actDuplicate_) actDuplicate_->setText(Tr("Дублировать объект", "Duplicate Object"));
    if (actRemove_) actRemove_->setText(Tr("Удалить объект", "Remove Object"));
    RebuildAddMenu();
}
