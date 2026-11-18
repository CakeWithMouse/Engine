#define NOMINMAX
#include "UIEditorWindow.h"
#include "UICanvas.h"
#include "CollapsibleCard.h"
#include "EditorLocale.h"
#include "EditorWindowSetup.h"
#include "InfoButton.h"
#include "QtHost.h"
#include "Graph/GraphFileIo.h"
#include "IconProvider.h"
#include "UiWidgetDrag.h"
#include "DropRules.h"
#include <Engine/UI/UIMarkup.h>
#include <QTreeWidget>
#include <QListWidget>
#include <QAbstractItemView>
#include <QSplitter>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QToolBar>
#include <QAction>
#include <QFileInfo>
#include <QDir>
#include <QByteArray>
#include <QMessageBox>
#include <QDropEvent>
#include <QCloseEvent>
#include <QStatusBar>
#include <algorithm>

namespace {
QString Type(const UIElement* e) {
    return dynamic_cast<const UIPanel*>(e) ? "Panel" : dynamic_cast<const UILabel*>(e) ? "Label" :
        dynamic_cast<const UIButton*>(e) ? "Button" : dynamic_cast<const UIBar*>(e) ? "Bar" : "Group";
}

QIcon TypeIcon(const QString& type) {
    if (type == QStringLiteral("Panel")) return IconProvider::FrameIcon();
    if (type == QStringLiteral("Label")) return IconProvider::TypographyIcon();
    if (type == QStringLiteral("Button")) return IconProvider::HandClickIcon();
    if (type == QStringLiteral("Bar")) return IconProvider::ProgressIcon();
    return IconProvider::FolderIcon();
}
}

// Palette of engine widget types; dragging an item onto the canvas adds that widget at the
// drop point, dragging into the hierarchy adds it to the container under the cursor
// (DropRules UiPalette -> UiCanvas/Hierarchy, "UI.AddWidget"). Only engine-supported types.
class UIPaletteList : public QListWidget {
public:
    using QListWidget::QListWidget;
protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override {
        auto* mime = new QMimeData;
        if (!items.isEmpty()) {
            EditorCore::UiWidgetDrag::Payload payload;
            payload.type = items.first()->text();
            mime->setData(QLatin1String(EditorCore::UiWidgetDrag::kMimeType), EditorCore::UiWidgetDrag::Encode(payload));
        }
        return mime;
    }
};

UIEditorWindow::UIEditorWindow(EditorQtHost* host, const QString& file, QWidget* parent)
    : QMainWindow(nullptr, Qt::Window), host_(host), file_(file)
{
    EditorWindowSetup::ConfigureSubWindow(this, parent);
    setObjectName("UIEditorWindow");
    setAttribute(Qt::WA_DeleteOnClose);
    resize(1200, 760);

    auto* tools = addToolBar(QStringLiteral("UI"));
    tools->setObjectName("UIToolBar");
    tools->setIconSize(QSize(16, 16));
    tools->setToolButtonStyle(Qt::ToolButtonIconOnly);

    saveAction_ = tools->addAction(IconProvider::SaveIcon(), Tr("Сохранить", "Save"), this, [this] { Save(); });
    saveAction_->setShortcut(QKeySequence::Save);
    saveAction_->setObjectName("UISave");
    saveAction_->setToolTip(Tr("Сохранить (Ctrl+S)", "Save (Ctrl+S)"));

    reloadAction_ = tools->addAction(IconProvider::RevertIcon(), Tr("Перечитать", "Reload"), this, [this] { if (ConfirmClose()) Reload(); });
    reloadAction_->setObjectName("UIReload");
    reloadAction_->setToolTip(Tr("Перечитать (сброс несохранённых правок)", "Reload (discard unsaved changes)"));

    tools->addSeparator();

    undoAction_ = tools->addAction(IconProvider::UndoIcon(), Tr("Отменить", "Undo"), this, [this] { Undo(); });
    undoAction_->setShortcut(QKeySequence::Undo);
    undoAction_->setObjectName("UIUndo");
    undoAction_->setToolTip(Tr("Отменить (Ctrl+Z)", "Undo (Ctrl+Z)"));

    redoAction_ = tools->addAction(IconProvider::RedoIcon(), Tr("Повторить", "Redo"), this, [this] { Redo(); });
    redoAction_->setShortcut(QKeySequence::Redo);
    redoAction_->setObjectName("UIRedo");
    redoAction_->setToolTip(Tr("Повторить (Ctrl+Y)", "Redo (Ctrl+Y)"));

    tools->addSeparator();

    deleteAction_ = tools->addAction(IconProvider::TrashIcon(), Tr("Удалить", "Delete"), this, [this] { DeleteSelected(); });
    deleteAction_->setObjectName("UIDelete");
    deleteAction_->setToolTip(Tr("Удалить выбранный элемент (Delete)", "Delete selected element (Delete)"));

    tools->addSeparator();

    auto* size = new QComboBox(this);
    size->setObjectName("UIPreviewSize");
    size->addItems({"1280 × 720", "1920 × 1080", "800 × 600", "720 × 1280"});
    size->setToolTip(Tr("Разрешение холста предпросмотра", "Preview canvas resolution"));
    tools->addWidget(size);

    tools->addSeparator();

    gridAction_ = tools->addAction(IconProvider::GetIcon(QStringLiteral("layout-grid")), Tr("Сетка", "Grid"), this, [this](bool on) { if (canvas_) canvas_->SetGrid(on); });
    gridAction_->setCheckable(true);
    gridAction_->setChecked(true);
    gridAction_->setObjectName("UIGrid");
    gridAction_->setToolTip(Tr("Включить/выключить сетку", "Toggle Grid"));

    snapAction_ = tools->addAction(IconProvider::SnapIcon(), Tr("Привязка", "Snap"), this, [this](bool on) { if (canvas_) canvas_->SetSnap(on); });
    snapAction_->setCheckable(true);
    snapAction_->setChecked(false);
    snapAction_->setObjectName("UISnap");
    snapAction_->setToolTip(Tr("Привязка к сетке при перетаскивании (шаг 40)", "Snap to grid while dragging (step 40)"));

    tools->addSeparator();

    helpBtn_ = new InfoButton(this);
    helpBtn_->setObjectName("UIEditorHelpButton");
    helpBtn_->SetToolbarStyle(true);
    helpBtn_->SetInfoText(
        Tr("Редактор интерфейса", "UI Editor"),
        Tr("• СКМ или Пробел+ЛКМ: панорамирование холста\n• Колесо мыши: масштабирование к курсору\n• Палитра: перетащите тип на холст или в иерархию; двойной щелчок добавляет в выбранную панель\n• Магнит: привязка к сетке при перетаскивании (шаг 40)\n• Дерево иерархии: переупорядочивание и вложенность элементов",
           "• MMB or Space+LMB: pan canvas\n• Mouse wheel: zoom to cursor\n• Palette: drag a type onto the canvas or into the hierarchy; double-click adds to the selected panel\n• Magnet: snap to grid while dragging (step 40)\n• Hierarchy tree: reorder and nest elements"),
        QString());
    tools->addWidget(helpBtn_);

    auto* split = new QSplitter(this);
    setCentralWidget(split);

    auto* left = new QWidget(split);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(2);

    hierarchyHeader_ = EditorWindowSetup::MakePanelHeader(Tr("Иерархия", "Hierarchy"), left);
    leftLayout->addWidget(hierarchyHeader_);
    auto* hierarchy = new UIHierarchyTree(left);
    hierarchy->editor = this;
    tree_ = hierarchy;
    tree_->setHeaderHidden(true);
    tree_->setObjectName("UIHierarchy");
    tree_->setAlternatingRowColors(true);
    tree_->setIndentation(16);
    tree_->setAnimated(true);
    tree_->setDragEnabled(true);
    tree_->setAcceptDrops(true);
    tree_->setDropIndicatorShown(true);
    tree_->setDragDropMode(QAbstractItemView::InternalMove);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(tree_, 1);

    paletteHeader_ = EditorWindowSetup::MakePanelHeader(Tr("Палитра", "Palette"), left);
    leftLayout->addWidget(paletteHeader_);
    auto* palette = new UIPaletteList(left);
    palette->setObjectName("UIPalette");
    palette->setIconSize(QSize(16, 16));
    palette->setSpacing(1);
    const struct { const char* type; } elemTypes[] = {{"Panel"}, {"Label"}, {"Button"}, {"Bar"}, {"Group"}};
    for (const auto& item : elemTypes) {
        auto* pItem = new QListWidgetItem(TypeIcon(QString::fromLatin1(item.type)), QString::fromLatin1(item.type), palette);
        pItem->setSizeHint(QSize(0, 28));
        pItem->setToolTip(Tr("Перетащите на холст или в иерархию; двойной щелчок — добавить в выбранную панель",
                             "Drag onto the canvas or into the hierarchy; double-click adds to the selected panel"));
    }
    palette->setDragEnabled(true);
    palette->setDragDropMode(QAbstractItemView::DragOnly);
    palette->setMaximumHeight(160);
    palette->setToolTip(Tr("Перетащите тип на холст или в иерархию", "Drag a type onto the canvas or into the hierarchy"));
    connect(palette, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) { AddElement(item->text()); });
    leftLayout->addWidget(palette);

    canvas_ = new UICanvas(this);
    split->addWidget(canvas_);

    auto* right = new QWidget(split);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    propsHeader_ = EditorWindowSetup::MakePanelHeader(Tr("Свойства", "Properties"), right);
    rightLayout->addWidget(propsHeader_);
    auto* scroll = new QScrollArea(right);
    auto* props = new QWidget(scroll);
    scroll->setWidget(props);
    BuildInspector(props, scroll);
    rightLayout->addWidget(scroll, 1);
    split->addWidget(right);

    split->setSizes({220, 700, 280});
    split->setStretchFactor(1, 1);
    status_ = new QLabel(this);
    status_->setWordWrap(true);
    statusBar()->addWidget(status_, 1);

    connect(size, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QSize sizes[] = {{1280, 720}, {1920, 1080}, {800, 600}, {720, 1280}};
        canvas_->resolution = sizes[index];
        canvas_->update();
    });
    connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        if (!refreshing_ && item) Select(reinterpret_cast<UIElement*>(item->data(0, Qt::UserRole).value<quintptr>()));
    });

    Reload();
    if (!canvas_->Ready()) status_->setText(Tr("Не удалось создать атлас шрифта для предпросмотра.", "Failed to create font atlas for preview."));
    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
        RetranslateUi();
    });
}

UIEditorWindow::~UIEditorWindow() {
    refreshing_ = true;
    selected_ = nullptr;
    for (auto* child : findChildren<QObject*>())
        QObject::disconnect(child, nullptr, this, nullptr);
    if (langSubId_) {
        EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        langSubId_ = 0;
    }
}

QString UIEditorWindow::ProjectRoot() const {
    return QFileInfo(QString::fromUtf8(host_->host().projectFile)).dir().absolutePath();
}

QString UIEditorWindow::ContentRoot() const {
    return QString::fromUtf8(host_->host().contentRoot);
}

bool UIEditorWindow::Reload() {
    QByteArray data; QString readError;
    if (!EditorCore::GraphFileIo::Read(ProjectRoot(), ContentRoot(), file_, data, &readError)) { status_->setText(readError); return false; }
    std::unique_ptr<UIElement> next; std::string error; std::vector<std::string> warnings;
    if (!UIMarkup::Parse(data.toStdString(), file_.toStdString(), next, error, &warnings)) { status_->setText(QString::fromStdString(error)); return false; }
    root_ = std::move(next); selected_ = root_.get(); dirty_ = false; undo_.clear(); redo_.clear(); UpdateHistoryActions(); setWindowTitle(Tr("Редактор UI — ", "UI Editor — ") + QFileInfo(file_).fileName());
    RebuildTree(); RefreshInspector(); canvas_->update(); status_->setText(warnings.empty() ? file_ : QString::fromStdString(warnings.front())); return true;
}

bool UIEditorWindow::Save() {
    if (!root_) return false;
    if (!ApplyInspector()) return false;
    const auto text = UIMarkup::Serialize(*root_); std::unique_ptr<UIElement> check; std::string error;
    if (!UIMarkup::Parse(text, file_.toStdString(), check, error)) { status_->setText(QString::fromStdString(error)); return false; }
    QString writeError;
    if (!EditorCore::GraphFileIo::Write(ProjectRoot(), ContentRoot(), file_, QByteArray(text.data(), int(text.size())), &writeError)) { status_->setText(writeError); return false; }
    dirty_ = false; setWindowTitle(Tr("Редактор UI — ", "UI Editor — ") + QFileInfo(file_).fileName()); status_->setText(Tr("Сохранено: ", "Saved: ") + file_); return true;
}

void UIEditorWindow::Changed() {
    dirty_ = true;
    setWindowTitle(Tr("Редактор UI — ", "UI Editor — ") + QFileInfo(file_).fileName() + QStringLiteral(" *"));
    canvas_->update();
}

void UIEditorWindow::RebuildTree() {
    refreshing_ = true;
    tree_->clear();
    std::function<void(UIElement*, QTreeWidgetItem*)> add = [&](UIElement* e, QTreeWidgetItem* parent) {
        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_);
        const QString typeStr = Type(e);
        item->setIcon(0, TypeIcon(typeStr));
        item->setText(0, typeStr + QStringLiteral("  ") + QString::fromStdString(e->Name));
        item->setToolTip(0, QStringLiteral("%1: %2").arg(typeStr, QString::fromStdString(e->Name)));
        item->setData(0, Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(e)));
        if (e == selected_) tree_->setCurrentItem(item);
        for (const auto& child : e->GetChildren()) add(child.get(), item);
    };
    if (root_) add(root_.get(), nullptr);
    tree_->expandAll();
    refreshing_ = false;
}

void UIEditorWindow::Select(UIElement* element) {
    selected_ = element;
    RefreshInspector();
    refreshing_ = true;
    QTreeWidgetItemIterator it(tree_);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).value<quintptr>() == reinterpret_cast<quintptr>(element)) {
            tree_->setCurrentItem(*it);
            break;
        }
        ++it;
    }
    refreshing_ = false;
    canvas_->update();
}

void UIEditorWindow::AddElement(const QString& type) { InsertNewElement(type, 16, 16, nullptr); }
void UIEditorWindow::AddElementAt(const QString& type, const QPointF& pos) { InsertNewElement(type, float(pos.x()), float(pos.y()), nullptr); }
void UIEditorWindow::AddElementTo(const QString& type, UIElement* parent) { if (parent) InsertNewElement(type, 16, 16, parent); }

void UIEditorWindow::InsertNewElement(const QString& type, float x, float y, UIElement* parent) {
    if (!root_) return;
    std::unique_ptr<UIElement> next;
    if (type == QStringLiteral("Panel")) next = std::make_unique<UIPanel>();
    else if (type == QStringLiteral("Label")) { auto p = std::make_unique<UILabel>(); p->SetText(Tr("Текст", "Text").toUtf8().toStdString()); next = std::move(p); }
    else if (type == QStringLiteral("Button")) { auto p = std::make_unique<UIButton>(); p->SetText(Tr("Кнопка", "Button").toUtf8().toStdString()); next = std::move(p); }
    else if (type == QStringLiteral("Bar")) { auto p = std::make_unique<UIBar>(); p->Fill = 0.5f; next = std::move(p); }
    else if (type == QStringLiteral("Group")) next = std::make_unique<UIElement>();
    else return;

    int number = 1;
    do { next->Name = type.toLower().toStdString() + std::to_string(number++); } while (root_->Find(next->Name));
    const bool box = (type == QStringLiteral("Panel") || type == QStringLiteral("Group"));
    next->Rect = {UIAnchor::TopLeft, x, y, box ? 320.0f : 200.0f, box ? 180.0f : 36.0f};

    if (!parent) {
        parent = selected_ ? selected_ : root_.get();
        while (parent != root_.get() && Type(parent) != "Panel" && Type(parent) != "Group") parent = parent->GetParent();
    }
    Checkpoint();
    selected_ = parent->Add(std::move(next));
    Changed();
    RebuildTree();
    RefreshInspector();
}

void UIEditorWindow::DeleteSelected() {
    if (!selected_ || selected_ == root_.get()) return;
    auto* parent = selected_->GetParent();
    Checkpoint();
    parent->RemoveChild(selected_);
    selected_ = parent;
    Changed();
    RebuildTree();
    RefreshInspector();
}

bool UIEditorWindow::ConfirmClose() {
    if (!dirty_) return true;
    const auto choice = QMessageBox::question(this, Tr("Редактор UI", "UI Editor"),
        Tr("Сохранить изменения в ", "Save changes to ") + QFileInfo(file_).fileName() + QStringLiteral("?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    return choice == QMessageBox::Discard || (choice == QMessageBox::Save && Save());
}

void UIEditorWindow::closeEvent(QCloseEvent* event) {
    if (ConfirmClose()) event->accept(); else event->ignore();
}
