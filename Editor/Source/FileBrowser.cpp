#include "FileBrowser.h"
#include "EditorLocale.h"
#include "QtHost.h"
#include "MainWindow.h"

#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
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

class ProjectFilesFilter final : public QSortFilterProxyModel
{
public:
    QString project, content;
    bool full = false;
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void ShowFull(bool value) { full = value; invalidateFilter(); }
protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        const auto* files = static_cast<QFileSystemModel*>(sourceModel());
        if (!full && QDir::cleanPath(files->filePath(parent)) == project) {
            const auto index = files->index(row, 0, parent);
            return files->isDir(index) && (files->fileName(index) == "Source" || QDir::cleanPath(files->filePath(index)) == content);
        }
        return true;
    }
};

FileBrowserDock::FileBrowserDock(EditorQtHost* host, QWidget* parent)
    : QDockWidget(Tr("Контент", "Content Browser"), parent)
    , host_(host)
{
    setObjectName(QStringLiteral("ContentDock"));
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    setToolTip(QStringLiteral("Потяните заголовок или вкладку, чтобы переместить панель. Верхняя граница меняет высоту. Команды размещения — в меню «Вид»."));
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
    auto* importMenu = new QMenu(imports_);

    actImportModel_ = importMenu->addAction(QString());
    actImportModel_->setEnabled(host_->HasScene());
    connect(actImportModel_, &QAction::triggered, this, [this] {
        if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->ImportAsset(false);
    });

    actImportTexture_ = importMenu->addAction(QString());
    actImportTexture_->setEnabled(host_->HasScene());
    connect(actImportTexture_, &QAction::triggered, this, [this] {
        if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->ImportAsset(true);
    });

    imports_->setMenu(importMenu);

    // --- верхняя панель: фильтр + выбор корня ---
    auto* topBar = new QHBoxLayout;
    filterEdit_ = new QLineEdit(panel);
    connect(filterEdit_, &QLineEdit::textChanged, this,
            [this](const QString& t) { ApplyFilter(t); });
    topBar->addWidget(imports_);
    topBar->addWidget(filterEdit_, 1);

    rootCombo_ = new QComboBox(panel);
    rootCombo_->setObjectName("FileVisibility");
    rootCombo_->addItem(QString(), projectDir_);
    rootCombo_->addItem(QString(), projectDir_);
    connect(rootCombo_, &QComboBox::currentIndexChanged, this,
            [this](int) { SetRootPath(rootCombo_->currentData().toString()); });
    topBar->addWidget(rootCombo_);
    layout->addLayout(topBar);

    // --- модель и дерево ---
    model_ = new QFileSystemModel(this);
    model_->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    model_->setNameFilterDisables(false);

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
    view_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 3; ++c)
        view_->header()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view_, &QTreeView::doubleClicked, this,
            [this](const QModelIndex& i) { OnDoubleClicked(i); });
    connect(view_, &QTreeView::customContextMenuRequested, this,
            [this](const QPoint& p) { OnContextMenu(p); });
    layout->addWidget(view_);

    setWidget(panel);
}

void FileBrowserDock::RetranslateUi()
{
    setWindowTitle(Tr("Контент", "Content Browser"));
    if (imports_) imports_->setText(Tr("Импорт…", "Import…"));
    if (actImportModel_) actImportModel_->setText(Tr("Модель…", "Model…"));
    if (actImportTexture_) actImportTexture_->setText(Tr("Текстура…", "Texture…"));
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
    const auto index = proxy_->mapToSource(proxyIndex);
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
    const QModelIndex index = proxy_->mapToSource(view_->indexAt(pos));
    const bool hasFile = index.isValid();
    const QString path = hasFile ? model_->filePath(index) : QString();

    QMenu menu(this);
    const bool isMaterial = hasFile && path.endsWith(QStringLiteral(".mat"), Qt::CaseInsensitive);
    QAction* use = hasFile && !model_->isDir(index) ? menu.addAction(isMaterial ? Tr("Назначить материал", "Assign Material")
                                                                               : Tr("Добавить модель / назначить текстуру", "Add Model / Assign Texture")) : nullptr;
    QAction* editMaterial = isMaterial && host_->HasMaterials() ? menu.addAction(Tr("Редактировать материал", "Edit Material")) : nullptr;
    QAction* newMaterial = host_->HasMaterials() ? menu.addAction(Tr("Новый материал…", "New Material…")) : nullptr;
    QAction* open = hasFile ? menu.addAction(Tr("Открыть в приложении", "Open in External App")) : nullptr;
    QAction* reveal = hasFile ? menu.addAction(Tr("Показать в проводнике", "Reveal in Explorer")) : nullptr;
    QAction* copy = hasFile ? menu.addAction(Tr("Копировать путь", "Copy Path")) : nullptr;
    QAction* refresh = menu.addAction(Tr("Обновить", "Refresh"));
    const bool asset=hasFile&&!model_->isDir(index)&&QStringList{"fbx","obj","gltf","glb","png","jpg","jpeg","bmp","tga","mat"}.contains(QFileInfo(path).suffix().toLower());
    QAction* reimport=asset&&!isMaterial&&host_->ReadSnapshot().authoring?menu.addAction(QStringLiteral("Переимпортировать…")):nullptr;
    QAction* move=asset&&host_->ReadSnapshot().authoring?menu.addAction(QStringLiteral("Переместить / переименовать…")):nullptr;

    QAction* picked = menu.exec(view_->viewport()->mapToGlobal(pos));
    if (!picked)
        return;

    if(picked==reimport || picked==move) {
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
