#include "EditorTheme.h"
#include "FileBrowser.h"
#include "FileService.h"
#include "EditorLocale.h"
#include "QtHost.h"
#include "MainWindow.h"
#include "IconProvider.h"
#include "CommandRegistry.h"
#include <QAbstractFileIconProvider>

#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QProcess>
#include <QRegularExpression>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QToolButton>
#include <QSortFilterProxyModel>

#include <string>

class TablerFileIconProvider final : public QAbstractFileIconProvider
{
public:
    QIcon icon(IconType type) const override
    {
        if (type == Folder) return IconProvider::FolderIcon();
        return IconProvider::FileIcon();
    }
    QIcon icon(const QFileInfo& info) const override
    {
        if (info.isDir()) return IconProvider::FolderIcon();
        return IconProvider::FileIcon();
    }
};

FileBrowserDock::FileBrowserDock(EditorQtHost* host, QWidget* parent)
    : QDockWidget(Tr("Контент", "Content Browser"), parent)
    , host_(host)
{
    setObjectName(QStringLiteral("ContentDock"));
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    setToolTip(Tr("Потяните заголовок или вкладку, чтобы переместить панель. Верхняя граница меняет высоту. Команды размещения — в меню «Вид».", "Drag the title or tab to move the panel. Top edge changes height. Layout commands are in the View menu."));
    const QFileInfo projectFile(QString::fromUtf8(
        host_->host().projectFile ? host_->host().projectFile : ""));
    projectDir_ = projectFile.absoluteDir().absolutePath();
    contentDir_ = QString::fromUtf8(host_->host().contentRoot ? host_->host().contentRoot : "");
    if (projectDir_.isEmpty())
        projectDir_ = QDir::currentPath();

    BuildUi();
    RetranslateUi();
    SetRootPath(projectDir_);

    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
        RetranslateUi();
    });
}

FileBrowserDock::~FileBrowserDock()
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

void FileBrowserDock::BuildUi()
{
    auto* panel = new QWidget(this);
    panel->setMinimumSize(40, 30);
    panel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(2, 2, 2, 2);
    imports_ = new QToolButton(panel);
    imports_->setObjectName(QStringLiteral("ImportAssets"));
    imports_->setPopupMode(QToolButton::InstantPopup);
    imports_->setIcon(IconProvider::ImportIcon());
    imports_->setFixedHeight(24);
    auto* importMenu = new QMenu(imports_);

    actImportModel_ = importMenu->addAction(QString());
    actImportModel_->setEnabled(host_->HasScene());
    connect(actImportModel_, &QAction::triggered, this, [this] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.Import"), {{QStringLiteral("kind"), 0}});
    });

    actImportTexture_ = importMenu->addAction(QString());
    actImportTexture_->setEnabled(host_->HasScene());
    connect(actImportTexture_, &QAction::triggered, this, [this] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.Import"), {{QStringLiteral("kind"), 1}});
    });

    actImportAudio_ = importMenu->addAction(QString());
    actImportAudio_->setEnabled(host_->HasScene());
    connect(actImportAudio_, &QAction::triggered, this, [this] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.Import"), {{QStringLiteral("kind"), 2}});
    });

    imports_->setMenu(importMenu);

    // --- верхняя панель: фильтр + выбор корня ---
    auto* topBar = new QHBoxLayout;
    topBar->setContentsMargins(4, 4, 4, 4);
    topBar->setSpacing(6);
    filterEdit_ = new QLineEdit(panel);
    filterEdit_->setFixedHeight(24);
    filterEdit_->addAction(IconProvider::SearchIcon(), QLineEdit::LeadingPosition);
    connect(filterEdit_, &QLineEdit::textChanged, this,
            [this](const QString& t) { ApplyFilter(t); });
    topBar->addWidget(imports_);
    topBar->addWidget(filterEdit_, 1);

    rootCombo_ = new QComboBox(panel);
    rootCombo_->setObjectName("FileVisibility");
    rootCombo_->setFixedHeight(24);
    rootCombo_->addItem(QString(), projectDir_);
    rootCombo_->addItem(QString(), projectDir_);
    connect(rootCombo_, &QComboBox::currentIndexChanged, this,
            [this](int) { SetRootPath(rootCombo_->currentData().toString()); });
    topBar->addWidget(rootCombo_);
    layout->addLayout(topBar);

    themeSubId_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
        if (imports_) imports_->setIcon(IconProvider::ImportIcon());
        if (filterEdit_) {
            for (auto* a : filterEdit_->actions()) filterEdit_->removeAction(a);
            filterEdit_->addAction(IconProvider::SearchIcon(), QLineEdit::LeadingPosition);
        }
        if (view_) view_->viewport()->update();
    });

    // --- модель и дерево ---
    model_ = new QFileSystemModel(this);
    model_->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    model_->setNameFilterDisables(false);
    model_->setIconProvider(new TablerFileIconProvider());

    proxy_ = new ProjectFilesFilter(this);
    proxy_->project = QDir::cleanPath(projectDir_); proxy_->content = QDir::cleanPath(contentDir_);
    proxy_->setSourceModel(model_);
    view_ = new QTreeView(panel);
    view_->setObjectName("ProjectFiles");
    view_->setModel(proxy_);
    view_->hideColumn(3);
    view_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    view_->setMinimumSize(40, 20);
    view_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    view_->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    view_->header()->setFixedHeight(20);
    QFont fileHeaderFont = view_->header()->font();
    fileHeaderFont.setPixelSize(11);
    view_->header()->setFont(fileHeaderFont);
    view_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 3; ++c)
        view_->header()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    view_->setDragEnabled(true);            // текстуры бросают на слоты редактора материала
    view_->setDragDropMode(QAbstractItemView::DragOnly);
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view_, &QTreeView::doubleClicked, this,
            [this](const QModelIndex& i) { OnDoubleClicked(i); });
    connect(view_, &QTreeView::customContextMenuRequested, this,
            [this](const QPoint& p) { OnContextMenu(p); });

    // Handle Delete via event filter: a WidgetShortcut action stops firing once Windows deactivates the window.
    view_->installEventFilter(this);
    view_->viewport()->installEventFilter(this);

    layout->addWidget(view_);

    setWidget(panel);
}

void FileBrowserDock::RetranslateUi()
{
    if (view_) view_->viewport()->update();
    setWindowTitle(Tr("Контент-браузер", "Content Browser"));
    if (imports_) imports_->setText(Tr("Импорт…", "Import…"));
    if (actImportModel_) actImportModel_->setText(Tr("Модель…", "Model…"));
    if (actImportTexture_) actImportTexture_->setText(Tr("Текстура…", "Texture…"));
    if (actImportAudio_) actImportAudio_->setText(Tr("Аудио…", "Audio…"));
    if (filterEdit_) filterEdit_->setPlaceholderText(Tr("Фильтр: *.json;*.png ...", "Filter: *.json;*.png ..."));
    if (rootCombo_) {
        const int cur = rootCombo_->currentIndex();
        rootCombo_->setItemText(0, Tr("Content и Source", "Content and Source"));
        rootCombo_->setItemText(1, Tr("Весь проект", "Entire Project"));
        rootCombo_->setCurrentIndex(cur >= 0 ? cur : 0);
        rootCombo_->setToolTip(Tr("Binaries — DLL, Intermediate — временные файлы сборки. Доступны в режиме «Весь проект».",
                                  "Binaries — DLL, Intermediate — temporary build files. Available in «Entire Project» mode."));
    }
}

void FileBrowserDock::SetRootPath(const QString& path)
{
    proxy_->ShowFull(rootCombo_->currentIndex() == 1);
    const QModelIndex srcIndex = model_->setRootPath(path);
    view_->setRootIndex(proxy_->mapFromSource(srcIndex));
}

void FileBrowserDock::ApplyFilter(const QString& text)
{
    QStringList filters;
    for (const QString& part : text.split(QRegularExpression(QStringLiteral("[;\\s]+")), Qt::SkipEmptyParts))
        filters << part.trimmed();
    if (filters.isEmpty())
        filters << QStringLiteral("*");
    model_->setNameFilters(filters);
}

void FileBrowserDock::OnDoubleClicked(const QModelIndex& proxyIndex)
{
    const auto col0 = proxyIndex.sibling(proxyIndex.row(), 0);
    const auto index = proxy_->mapToSource(col0);
    if (!index.isValid() || model_->isDir(index))
        return;

    const QString path = model_->filePath(index);
    if (path.endsWith(QStringLiteral(".scene.json"), Qt::CaseInsensitive))
    {
        if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->OpenScene(path);
        return;
    }
    if (path.endsWith(QStringLiteral(".ui"), Qt::CaseInsensitive))
    {
        if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->OpenUIEditor(path);
        return;
    }
    if (path.endsWith(QStringLiteral(".mat"), Qt::CaseInsensitive))
    {
        if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->OpenMaterialEditor(path);
        return;
    }
    const QByteArray utf8 = path.toUtf8();
    const std::string msg = "EditorOpenAsset: " + std::string(utf8.constData(), utf8.size());
    host_->Log(msg.c_str());

    if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->UseAsset(path);
}

void FileBrowserDock::OnContextMenu(const QPoint& pos)
{
    const QModelIndex clicked = view_->indexAt(pos);
    const QModelIndex col0 = clicked.isValid() ? clicked.sibling(clicked.row(), 0) : QModelIndex();
    const QModelIndex index = proxy_->mapToSource(col0);
    const bool hasFile = index.isValid();
    const QString path = hasFile ? model_->filePath(index) : QString();

    QMenu menu(this);
    const bool isMaterial = hasFile && path.endsWith(QStringLiteral(".mat"), Qt::CaseInsensitive);
    QAction* use = hasFile && !model_->isDir(index) ? menu.addAction(isMaterial ? Tr("Назначить материал", "Assign Material")
                                                                               : Tr("Добавить модель / назначить текстуру", "Add Model / Assign Texture")) : nullptr;
    QAction* editMaterial = isMaterial && host_->HasMaterials() ? menu.addAction(Tr("Редактировать материал", "Edit Material")) : nullptr;
    const bool isAudio = hasFile && !model_->isDir(index) &&
        QStringList{"wav", "ogg", "mp3", "flac"}.contains(QFileInfo(path).suffix().toLower());
    const bool contentAudio = isAudio && !contentDir_.isEmpty() &&
        QDir::cleanPath(path).startsWith(QDir::cleanPath(contentDir_));
    QAction* playAudio = contentAudio && host_->AudioApi() ? menu.addAction(Tr("Прослушать", "Play Audio Preview")) : nullptr;
    QAction* stopAudio = host_->AudioApi() ? menu.addAction(Tr("Остановить звук", "Stop Audio Preview")) : nullptr;
    QAction* newMaterial = host_->HasMaterials() ? menu.addAction(Tr("Новый материал…", "New Material…")) : nullptr;
    QAction* open = hasFile ? menu.addAction(Tr("Открыть в приложении", "Open in External App")) : nullptr;
    QAction* reveal = hasFile ? menu.addAction(Tr("Показать в проводнике", "Reveal in Explorer")) : nullptr;
    QAction* copy = hasFile ? menu.addAction(Tr("Копировать путь", "Copy Path")) : nullptr;
    QAction* refresh = menu.addAction(Tr("Обновить", "Refresh"));
    const bool isModel = hasFile && !model_->isDir(index) && QStringList{"fbx", "obj", "gltf", "glb", "blend"}.contains(QFileInfo(path).suffix().toLower());
    QAction* editSettings = isModel && host_->ReadSnapshot().authoring ? menu.addAction(Tr("Настройки импорта…", "Import Settings…")) : nullptr;
    const bool asset=hasFile&&!model_->isDir(index)&&QStringList{"fbx","obj","gltf","glb","png","jpg","jpeg","bmp","tga","mat"}.contains(QFileInfo(path).suffix().toLower());
    QAction* reimport=asset&&!isMaterial&&host_->ReadSnapshot().authoring?menu.addAction(Tr("Переимпортировать…", "Reimport…")):nullptr;
    QAction* move=asset&&host_->ReadSnapshot().authoring?menu.addAction(Tr("Переместить / переименовать…", "Move / Rename…")):nullptr;
    QAction* deleteAction = hasFile && !IsProtectedPath(path) ? menu.addAction(Tr("Удалить", "Delete")) : nullptr;

    QAction* picked = menu.exec(view_->viewport()->mapToGlobal(pos));
    if (!picked)
        return;

    if (picked == editSettings)
    {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.EditImportSettings"), {{QStringLiteral("path"), path}});
    }
    else if (picked == deleteAction)
    {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.Delete"), {{QStringLiteral("path"), path}});
    }
    else if (picked == playAudio || picked == stopAudio)
    {
        const QString relative = QDir(contentDir_).relativeFilePath(path);
        EditorCommand c;
        c.type = picked == playAudio ? EditorCommand::PlayAudio : EditorCommand::StopAudio;
        c.path = QStringLiteral("/Game/") + QDir::fromNativeSeparators(relative);
        host_->PostCommand(c);
    }
    else if(picked==reimport || picked==move) {
        if(auto* editor=qobject_cast<EditorMainWindow*>(parentWidget()))editor->UpdateAsset(path,picked==move);
    }
    else if (picked == use)
    {
        if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->UseAsset(path);
    }
    else if (picked == editMaterial)
    {
        if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->OpenMaterialEditor(path);
    }
    else if (picked == newMaterial)
    {
        const QString directory = !hasFile ? QString() : model_->isDir(index) ? path : QFileInfo(path).absolutePath();
        if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->CreateMaterial(directory);
    }
    else if (picked == open)
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    else if (picked == reveal)
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                { QStringLiteral("/select,") + QDir::toNativeSeparators(path) });
    else if (picked == copy)
        QGuiApplication::clipboard()->setText(path);
    else if (picked == refresh)
        model_->setRootPath(model_->rootPath());
}

void FileBrowserDock::DeleteCurrentAsset()
{
    const QModelIndex viewIndex = view_->currentIndex();
    if (!viewIndex.isValid())
        return;

    const QModelIndex col0 = viewIndex.sibling(viewIndex.row(), 0);
    const QModelIndex current = proxy_->mapToSource(col0);
    if (!current.isValid())
        return;

    const QString path = model_->filePath(current);
    if (!path.isEmpty() && !IsProtectedPath(path))
    {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.Delete"), {{QStringLiteral("path"), path}});
    }
}

bool FileBrowserDock::IsProtectedPath(const QString& path) const
{
    return path.isEmpty() || FileService::IsProtectedDirectory(path, projectDir_, contentDir_);
}

bool FileBrowserDock::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == view_ || watched == view_->viewport()) && event->type() == QEvent::KeyPress)
    {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Delete)
        {
            DeleteCurrentAsset();
            return true;
        }
    }
    return QDockWidget::eventFilter(watched, event);
}
