#include "SceneObjectsPanel.h"
#include "SceneOutlinerTree.h"
#include "EditorLocale.h"

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
}

SceneObjectsPanel::~SceneObjectsPanel()
{
    if (langSubId_) {
        EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        langSubId_ = 0;
    }
}

void SceneObjectsPanel::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // 1. Поиск и кнопка +Add
    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(4);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setObjectName(QStringLiteral("SceneSearchEdit"));
    searchEdit_->setPlaceholderText(Tr("Поиск объектов…", "Search objects…"));
    searchEdit_->setClearButtonEnabled(true);
    connect(searchEdit_, &QLineEdit::textChanged, this, &SceneObjectsPanel::FilterHierarchy);
    topBar->addWidget(searchEdit_);

    addBtn_ = new QPushButton(Tr("+ Добавить", "+ Add"), this);
    addBtn_->setObjectName(QStringLiteral("SceneAddButton"));
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

    removeShortcut_ = new QAction(hierarchy_);
    removeShortcut_->setShortcut(QKeySequence(Qt::Key_Backspace));
    removeShortcut_->setShortcutContext(Qt::WidgetShortcut);
    hierarchy_->addAction(removeShortcut_);
    connect(removeShortcut_, &QAction::triggered, this, &SceneObjectsPanel::RemoveRequested);

    connect(hierarchy_, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (suppressSelectionChanged_) return;
        const auto items = hierarchy_->selectedItems();
        emit SelectionChanged(items.empty() ? 0 : items.front()->data(0, Qt::UserRole).toULongLong());
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

        target->addAction(label, this, [this, name, label] {
            emit AddPrimitiveRequested(name, label, SpawnPosition());
        });
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

    auto* actFoc = menu.addAction(Tr("Фокус на объекте (F)", "Focus Selected (F)"), this, &SceneObjectsPanel::FocusRequested);
    actFoc->setEnabled(selectedId != 0);

    auto* actDup = menu.addAction(Tr("Дублировать (Ctrl+D)", "Duplicate (Ctrl+D)"), this, &SceneObjectsPanel::DuplicateRequested);
    actDup->setEnabled(selectedId != 0);

    auto* actRen = menu.addAction(Tr("Переименовать (F2)", "Rename (F2)"), this, [this, item] {
        if (item) hierarchy_->editItem(item, 0);
    });
    actRen->setEnabled(selectedId != 0);

    menu.addSeparator();
    auto* actDel = menu.addAction(Tr("Удалить (Delete)", "Delete (Delete)"), this, &SceneObjectsPanel::RemoveRequested);
    actDel->setEnabled(selectedId != 0);

    menu.exec(hierarchy_->viewport()->mapToGlobal(pos));
}

void SceneObjectsPanel::RetranslateUi()
{
    if (searchEdit_) searchEdit_->setPlaceholderText(Tr("Поиск объектов…", "Search objects…"));
    if (addBtn_) addBtn_->setText(Tr("+ Добавить", "+ Add"));
    if (hierarchy_) hierarchy_->setHeaderLabels({Tr("Объект", "Object"), Tr("Тип", "Type")});
    if (actDuplicate_) actDuplicate_->setText(Tr("Дублировать объект", "Duplicate Object"));
    if (actRemove_) actRemove_->setText(Tr("Удалить объект", "Remove Object"));
    RebuildAddMenu();
}
