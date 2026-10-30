#include "FileBrowser.h"
#include "QtHost.h"
#include "MainWindow.h"
#include "EditorLocale.h"
#include "CodeClassDialog.h"

#include <QFileIconProvider>
#include <QPainter>
#include <QPainterPath>
#include <QImageReader>
#include <QDateTime>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QProcess>
#include <QRegularExpression>
#include <QTreeView>
#include <QListView>
#include <QStackedWidget>
#include <QSplitter>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QToolButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QSortFilterProxyModel>

#include <string>

// Кастомный провайдер иконок ассетов в стиле Unreal Engine 5:
// Генерирует миниатюры для текстур и стилизованные карточки с цветными полосками типов.
class AssetIconProvider : public QFileIconProvider
{
public:
    QIcon icon(const QFileInfo& info) const override
    {
        if (info.isDir()) {
            return GenerateFolderIcon();
        }
        return GenerateThumbnail(info);
    }

    QIcon icon(IconType type) const override
    {
        if (type == Folder) {
            return GenerateFolderIcon();
        }
        return QFileIconProvider::icon(type);
    }

private:
    QIcon GenerateFolderIcon() const
    {
        const QString key = QStringLiteral("__folder__");
        auto it = cache_.find(key);
        if (it != cache_.end()) return *it;

        QPixmap pixmap(96, 96);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        // Вкладка папки
        QPainterPath tab;
        tab.addRoundedRect(14, 24, 30, 14, 3, 3);
        p.fillPath(tab, QColor(0xD9, 0x77, 0x06));

        // Основное тело папки
        QPainterPath body;
        body.addRoundedRect(14, 32, 68, 44, 5, 5);
        p.fillPath(body, QColor(0xF5, 0x9E, 0x0B));

        // Затемнение сгиба
        p.setBrush(QColor(0, 0, 0, 40));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(14, 32, 68, 8, 3, 3);

        // Тонкий контур
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0xB4, 0x53, 0x09), 1.2));
        p.drawPath(body);

        QIcon ic(pixmap);
        cache_.insert(key, ic);
        return ic;
    }

    QIcon GenerateThumbnail(const QFileInfo& info) const
    {
        const QString absPath = info.absoluteFilePath();
        const QString ext = info.suffix().toLower();
        const QString cacheKey = absPath + QStringLiteral("_") + QString::number(info.lastModified().toMSecsSinceEpoch());

        auto it = cache_.find(cacheKey);
        if (it != cache_.end()) return *it;

        QPixmap pixmap(96, 96);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        // Фон карточки
        QRectF cardRect(4, 4, 88, 88);
        p.setBrush(QColor(0x1C, 0x1F, 0x26));
        p.setPen(QPen(QColor(0x2E, 0x34, 0x40), 1.2));
        p.drawRoundedRect(cardRect, 6, 6);

        QColor stripeColor = QColor(0x64, 0x74, 0x8B); // дефолтный серый
        QString badgeText;

        const bool isTex = (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tga" || ext == "dds" || ext == "hdr");
        const bool isMat = (ext == "mat");
        const bool isShader = (ext == "hlsl" || ext == "hlsli" || ext == "glsl");
        const bool isModel = (ext == "fbx" || ext == "obj" || ext == "blend" || ext == "gltf" || ext == "glb");
        const bool isScene = (ext == "json" && (info.fileName().endsWith(".scene.json", Qt::CaseInsensitive) || ext == "scene"));
        const bool isCpp = (ext == "cpp" || ext == "h" || ext == "hpp" || ext == "inl");

        if (isTex) {
            stripeColor = QColor(0xEC, 0x48, 0x99); // розовая полоска текстуры
            badgeText = QStringLiteral("TEX");

            // Загрузка миниатюры изображения
            QImageReader reader(absPath);
            reader.setAutoTransform(true);
            if (reader.canRead()) {
                const QSize origSize = reader.size();
                if (origSize.isValid() && (origSize.width() > 128 || origSize.height() > 128)) {
                    reader.setScaledSize(origSize.scaled(128, 128, Qt::KeepAspectRatio));
                }
                const QImage img = reader.read();
                if (!img.isNull()) {
                    const QRect targetRect(10, 10, 76, 64);
                    const QImage scaled = img.scaled(targetRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    const int ox = targetRect.x() + (targetRect.width() - scaled.width()) / 2;
                    const int oy = targetRect.y() + (targetRect.height() - scaled.height()) / 2;
                    p.drawImage(ox, oy, scaled);
                }
            }
        } else if (isMat) {
            stripeColor = QColor(0x10, 0xB9, 0x81); // изумрудная полоска материала
            badgeText = QStringLiteral("MAT");

            // Рисуем стилизованную сферу материала с PBR-бликом
            QRadialGradient grad(40, 34, 26, 38, 32);
            grad.setColorAt(0.0, QColor(0x6E, 0xEE, 0xB4));
            grad.setColorAt(0.5, QColor(0x10, 0xB9, 0x81));
            grad.setColorAt(1.0, QColor(0x06, 0x4E, 0x3B));
            p.setBrush(grad);
            p.setPen(QPen(QColor(0x04, 0x78, 0x57), 1.5));
            p.drawEllipse(QPointF(48, 42), 22, 22);

            // Блик
            p.setBrush(QColor(255, 255, 255, 180));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(42, 34), 5, 3);
        } else if (isShader) {
            stripeColor = QColor(0x06, 0xB6, 0xD4); // бирюзовая полоска шейдера
            badgeText = QStringLiteral("HLSL");

            QFont f = p.font();
            f.setBold(true);
            f.setPixelSize(22);
            p.setFont(f);
            p.setPen(QColor(0x22, 0xD3, 0xEE));
            p.drawText(QRectF(4, 18, 88, 44), Qt::AlignCenter, QStringLiteral("{ ; }"));
        } else if (isModel) {
            stripeColor = QColor(0x3B, 0x82, 0xF6); // синяя полоска 3D модели
            badgeText = (ext == "blend") ? QStringLiteral("BLEND") : (ext == "fbx") ? QStringLiteral("FBX") : QStringLiteral("3D");

            // Изометрический 3D-куб
            QPolygonF topFace, leftFace, rightFace;
            topFace << QPointF(48, 22) << QPointF(68, 32) << QPointF(48, 42) << QPointF(28, 32);
            leftFace << QPointF(28, 32) << QPointF(48, 42) << QPointF(48, 62) << QPointF(28, 52);
            rightFace << QPointF(48, 42) << QPointF(68, 32) << QPointF(68, 52) << QPointF(48, 62);

            p.setPen(QPen(QColor(0x1E, 0x3A, 0x8A), 1.0));
            p.setBrush(QColor(0x60, 0xA5, 0xFA));
            p.drawPolygon(topFace);
            p.setBrush(QColor(0x25, 0x63, 0xEB));
            p.drawPolygon(leftFace);
            p.setBrush(QColor(0x1D, 0x4E, 0xD8));
            p.drawPolygon(rightFace);
        } else if (isScene) {
            stripeColor = QColor(0xF5, 0x9E, 0x0B); // янтарная полоска сцены
            badgeText = QStringLiteral("SCN");

            // Иконка координатной сетки / мира
            p.setPen(QPen(QColor(0xFB, 0xBF, 0x24), 2.0));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(48, 42), 18, 18);
            p.drawLine(QPointF(30, 42), QPointF(66, 42));
            p.drawLine(QPointF(48, 24), QPointF(48, 60));
        } else if (isCpp) {
            stripeColor = QColor(0x8B, 0x5C, 0xF6); // фиолетовая полоска C++
            badgeText = QStringLiteral("C++");

            QFont f = p.font();
            f.setBold(true);
            f.setPixelSize(22);
            p.setFont(f);
            p.setPen(QColor(0xA7, 0x8B, 0xFA));
            p.drawText(QRectF(4, 18, 88, 44), Qt::AlignCenter, QStringLiteral("C++"));
        } else {
            badgeText = ext.toUpper().left(4);
            QFont f = p.font();
            f.setBold(true);
            f.setPixelSize(14);
            p.setFont(f);
            p.setPen(QColor(0x94, 0xA3, 0xB8));
            p.drawText(QRectF(4, 24, 88, 36), Qt::AlignCenter, badgeText);
        }

        // Нижняя цветная акцентная полоска типа ассета
        QPainterPath stripe;
        stripe.addRoundedRect(4, 86, 88, 6, 2, 2);
        p.fillPath(stripe, stripeColor);

        // Бейдж типа ассета в правом нижнем углу
        if (!badgeText.isEmpty()) {
            QFont badgeFont = p.font();
            badgeFont.setPixelSize(9);
            badgeFont.setBold(true);
            p.setFont(badgeFont);
            p.setPen(stripeColor);
            p.drawText(QRectF(8, 70, 80, 14), Qt::AlignRight | Qt::AlignVCenter, badgeText);
        }

        QIcon ic(pixmap);
        cache_.insert(cacheKey, ic);
        return ic;
    }

    mutable QHash<QString, QIcon> cache_;
};

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
    const QFileInfo projectFile(QString::fromUtf8(
        host_->host().projectFile ? host_->host().projectFile : ""));
    projectDir_ = projectFile.absoluteDir().absolutePath();
    contentDir_ = QString::fromUtf8(host_->host().contentRoot ? host_->host().contentRoot : "");
    if (projectDir_.isEmpty())
        projectDir_ = QDir::currentPath();

    setMinimumSize(80, 60);
    BuildUi();

    QString startFolder = !contentDir_.isEmpty() && QDir(contentDir_).exists() ? contentDir_ : projectDir_;
    SetCurrentFolder(startFolder);

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
    delete iconProvider_;
    iconProvider_ = nullptr;
}

void FileBrowserDock::RetranslateUi()
{
    setWindowTitle(Tr("Контент", "Content Browser"));
    if (btnCreate_) btnCreate_->setText(Tr("+ Создать ▾", "+ Create ▾"));
    if (importsBtn_) importsBtn_->setText(Tr("Импорт…", "Import…"));
    if (importTextureAct_) importTextureAct_->setText(Tr("Текстура…", "Texture…"));
    if (importModelAct_) importModelAct_->setText(Tr("Модель…", "Model…"));
    if (filterEdit_) filterEdit_->setPlaceholderText(Tr("Поиск ассетов...", "Search assets..."));
    if (btnViewGrid_) btnViewGrid_->setToolTip(Tr("Режим плитки (Grid)", "Grid View"));
    if (btnViewList_) btnViewList_->setToolTip(Tr("Режим списка (List)", "List View"));
    if (sliderTileSize_) sliderTileSize_->setToolTip(Tr("Размер плиток", "Tile Size"));
    if (rootCombo_) {
        const int cur = rootCombo_->currentIndex();
        rootCombo_->setItemText(0, Tr("Content и Source", "Content and Source"));
        rootCombo_->setItemText(1, Tr("Весь проект", "Entire Project"));
        rootCombo_->setToolTip(Tr("Binaries — DLL, Intermediate — временные файлы сборки. Доступны в режиме «Весь проект».",
                                  "Binaries — DLL, Intermediate — temporary build files. Available in 'Entire Project' mode."));
        rootCombo_->setCurrentIndex(cur);
    }
}

void FileBrowserDock::BuildUi()
{
    auto* mainWidget = new QWidget(this);
    mainWidget->setObjectName("ContentBrowserPanel");
    mainWidget->setMinimumSize(80, 60);
    auto* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    iconProvider_ = new AssetIconProvider();

    // ==========================================
    // 1. Верхняя панель навигации (Top Bar)
    // ==========================================
    auto* topBar = new QHBoxLayout();
    topBar->setContentsMargins(2, 2, 2, 2);
    topBar->setSpacing(4);

    btnNavBack_ = new QPushButton(QStringLiteral("←"), mainWidget);
    btnNavBack_->setFixedSize(26, 24);
    btnNavBack_->setToolTip(Tr("Назад", "Back"));
    btnNavBack_->setEnabled(false);
    connect(btnNavBack_, &QPushButton::clicked, this, [this] { NavigateBack(); });
    topBar->addWidget(btnNavBack_);

    btnNavUp_ = new QPushButton(QStringLiteral("↑"), mainWidget);
    btnNavUp_->setFixedSize(26, 24);
    btnNavUp_->setToolTip(Tr("На уровень вверх", "Up"));
    connect(btnNavUp_, &QPushButton::clicked, this, [this] { NavigateUp(); });
    topBar->addWidget(btnNavUp_);

    lblCurrentPath_ = new QLabel(QStringLiteral("/Game"), mainWidget);
    lblCurrentPath_->setObjectName("BreadcrumbPath");
    lblCurrentPath_->setStyleSheet("color: #94A3B8; font-weight: bold; padding: 0 4px;");
    topBar->addWidget(lblCurrentPath_);

    topBar->addSpacing(6);

    // Кнопка "+ Создать ▾"
    btnCreate_ = new QToolButton(mainWidget);
    btnCreate_->setObjectName("BtnCreateAsset");
    btnCreate_->setText(Tr("+ Создать ▾", "+ Create ▾"));
    btnCreate_->setPopupMode(QToolButton::InstantPopup);
    btnCreate_->setStyleSheet("QToolButton#BtnCreateAsset { background-color: #2563EB; color: white; font-weight: bold; border-radius: 4px; padding: 3px 8px; }"
                             "QToolButton#BtnCreateAsset:hover { background-color: #1D4ED8; }");
    menuCreate_ = new QMenu(btnCreate_);

    auto* actNewFolder = menuCreate_->addAction(Tr("📁 Новая папка", "📁 New Folder"));
    connect(actNewFolder, &QAction::triggered, this, [this] { CreateNewFolder(); });

    menuCreate_->addSeparator();

    auto* actNewMat = menuCreate_->addAction(Tr("🔮 Новый Материал (.mat)", "🔮 New Material (.mat)"));
    connect(actNewMat, &QAction::triggered, this, [this] { CreateNewMaterial(); });

    auto* actNewMatInst = menuCreate_->addAction(Tr("✨ Новый Material Instance", "✨ New Material Instance"));
    connect(actNewMatInst, &QAction::triggered, this, [this] { CreateNewMaterialInstance(); });

    auto* actNewScene = menuCreate_->addAction(Tr("🌐 Новая сцена (.scene.json)", "🌐 New Scene (.scene.json)"));
    connect(actNewScene, &QAction::triggered, this, [this] { CreateNewScene(); });

    auto* actNewCpp = menuCreate_->addAction(Tr("💻 C++ Класс...", "💻 C++ Class..."));
    connect(actNewCpp, &QAction::triggered, this, [this] { CreateNewCppClass(); });

    btnCreate_->setMenu(menuCreate_);
    topBar->addWidget(btnCreate_);

    // Кнопка "Импорт..."
    importsBtn_ = new QToolButton(mainWidget);
    importsBtn_->setText(Tr("Импорт…", "Import…"));
    importsBtn_->setPopupMode(QToolButton::InstantPopup);
    auto* importMenu = new QMenu(importsBtn_);

    importModelAct_ = importMenu->addAction(Tr("Модель…", "Model…"));
    importModelAct_->setEnabled(host_->HasScene());
    connect(importModelAct_, &QAction::triggered, this, [this] {
        if (auto* editor = qobject_cast<EditorMainWindow*>(window())) editor->ImportAsset(false);
    });

    importTextureAct_ = importMenu->addAction(Tr("Текстура…", "Texture…"));
    importTextureAct_->setEnabled(host_->HasScene());
    connect(importTextureAct_, &QAction::triggered, this, [this] {
        if (auto* editor = qobject_cast<EditorMainWindow*>(window())) editor->ImportAsset(true);
    });
    importsBtn_->setMenu(importMenu);
    topBar->addWidget(importsBtn_);

    // Поле быстрого поиска / фильтра
    filterEdit_ = new QLineEdit(mainWidget);
    filterEdit_->setObjectName("AssetFilterEdit");
    filterEdit_->setPlaceholderText(Tr("Поиск ассетов...", "Search assets..."));
    filterEdit_->setMinimumWidth(80);
    filterEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(filterEdit_, &QLineEdit::textChanged, this, [this](const QString& t) { ApplyFilter(t); });
    topBar->addWidget(filterEdit_, 1);

    // Кнопки переключения вида: Grid / List
    btnViewGrid_ = new QToolButton(mainWidget);
    btnViewGrid_->setText(QStringLiteral("⊞"));
    btnViewGrid_->setToolTip(Tr("Режим плитки (Grid)", "Grid View"));
    btnViewGrid_->setCheckable(true);
    btnViewGrid_->setChecked(true);

    btnViewList_ = new QToolButton(mainWidget);
    btnViewList_->setText(QStringLiteral("≡"));
    btnViewList_->setToolTip(Tr("Режим списка (List)", "List View"));
    btnViewList_->setCheckable(true);
    btnViewList_->setChecked(false);

    connect(btnViewGrid_, &QToolButton::clicked, this, [this] {
        viewStack_->setCurrentIndex(0);
        btnViewGrid_->setChecked(true);
        btnViewList_->setChecked(false);
    });
    connect(btnViewList_, &QToolButton::clicked, this, [this] {
        viewStack_->setCurrentIndex(1);
        btnViewGrid_->setChecked(false);
        btnViewList_->setChecked(true);
    });

    topBar->addWidget(btnViewGrid_);
    topBar->addWidget(btnViewList_);

    // Ползунок размера плиток (Tile Size: 56..120px)
    sliderTileSize_ = new QSlider(Qt::Horizontal, mainWidget);
    sliderTileSize_->setRange(56, 120);
    sliderTileSize_->setValue(tileSize_);
    sliderTileSize_->setFixedWidth(64);
    sliderTileSize_->setToolTip(Tr("Размер плиток", "Tile Size"));
    connect(sliderTileSize_, &QSlider::valueChanged, this, [this](int val) { UpdateTileSize(val); });
    topBar->addWidget(sliderTileSize_);

    // Селектор видимости корня (Content и Source vs Весь проект)
    rootCombo_ = new QComboBox(mainWidget);
    rootCombo_->setObjectName("FileVisibility");
    rootCombo_->setMinimumWidth(100);
    rootCombo_->addItem(Tr("Content и Source", "Content and Source"), projectDir_);
    rootCombo_->addItem(Tr("Весь проект", "Entire Project"), projectDir_);
    connect(rootCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        SetRootPath(rootCombo_->currentData().toString());
    });
    topBar->addWidget(rootCombo_);

    mainLayout->addLayout(topBar);

    // ==========================================
    // 2. Дерево папок слева + QStackedWidget справа
    // ==========================================
    splitter_ = new QSplitter(Qt::Horizontal, mainWidget);
    splitter_->setHandleWidth(3);

    // Левое дерево: только директории
    folderModel_ = new QFileSystemModel(this);
    folderModel_->setIconProvider(iconProvider_);
    folderModel_->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);

    folderProxy_ = new ProjectFilesFilter(this);
    folderProxy_->project = QDir::cleanPath(projectDir_);
    folderProxy_->content = QDir::cleanPath(contentDir_);
    folderProxy_->setSourceModel(folderModel_);

    folderTree_ = new QTreeView(splitter_);
    folderTree_->setObjectName("FolderTree");
    folderTree_->setModel(folderProxy_);
    folderTree_->setHeaderHidden(true);
    for (int c = 1; c < 4; ++c) folderTree_->hideColumn(c);
    folderTree_->setMinimumWidth(110);
    folderTree_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    connect(folderTree_, &QTreeView::clicked, this, [this](const QModelIndex& proxyIdx) {
        const auto srcIdx = folderProxy_->mapToSource(proxyIdx);
        if (srcIdx.isValid()) {
            const QString path = folderModel_->filePath(srcIdx);
            if (QFileInfo(path).isDir()) {
                SetCurrentFolder(path);
            }
        }
    });
    splitter_->addWidget(folderTree_);

    // Правая рабочая область: QStackedWidget (Grid View и List View)
    viewStack_ = new QStackedWidget(splitter_);

    // Модель для правой области (файлы + папки)
    model_ = new QFileSystemModel(this);
    model_->setIconProvider(iconProvider_);
    model_->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    model_->setNameFilterDisables(false);

    proxy_ = new ProjectFilesFilter(this);
    proxy_->project = QDir::cleanPath(projectDir_);
    proxy_->content = QDir::cleanPath(contentDir_);
    proxy_->setSourceModel(model_);

    // Вид 1: Grid View (QListView в режиме IconMode)
    gridView_ = new QListView(viewStack_);
    gridView_->setObjectName("ContentGridView");
    gridView_->setViewMode(QListView::IconMode);
    gridView_->setResizeMode(QListView::Adjust);
    gridView_->setUniformItemSizes(true);
    gridView_->setWordWrap(true);
    gridView_->setDragEnabled(true);
    gridView_->setAcceptDrops(true);
    gridView_->setDropIndicatorShown(true);
    gridView_->setContextMenuPolicy(Qt::CustomContextMenu);
    gridView_->setModel(proxy_);
    gridView_->setIconSize(QSize(tileSize_, tileSize_));
    gridView_->setGridSize(QSize(tileSize_ + 24, tileSize_ + 36));
    gridView_->setSpacing(6);
    connect(gridView_, &QListView::doubleClicked, this, [this](const QModelIndex& idx) { OnDoubleClicked(idx); });
    connect(gridView_, &QListView::customContextMenuRequested, this, [this](const QPoint& pt) { OnContextMenu(pt, true); });
    viewStack_->addWidget(gridView_);

    // Вид 2: List / Tree View
    view_ = new QTreeView(viewStack_);
    view_->setObjectName("ProjectFiles");
    view_->setModel(proxy_);
    view_->hideColumn(3); // Date Modified не нужен
    view_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 3; ++c)
        view_->header()->setSectionResizeMode(c, QHeaderView::Interactive);
    view_->header()->setStretchLastSection(true);
    view_->setDragEnabled(true);
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view_, &QTreeView::doubleClicked, this, [this](const QModelIndex& idx) { OnDoubleClicked(idx); });
    connect(view_, &QTreeView::customContextMenuRequested, this, [this](const QPoint& pt) { OnContextMenu(pt, false); });
    viewStack_->addWidget(view_);

    splitter_->addWidget(viewStack_);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({ 160, 500 });

    mainLayout->addWidget(splitter_, 1);
    setWidget(mainWidget);
}

void FileBrowserDock::SetRootPath(const QString& path)
{
    if (path.isEmpty()) return;
    const bool showFull = (rootCombo_ && rootCombo_->currentIndex() == 1);
    if (proxy_) proxy_->ShowFull(showFull);
    if (folderProxy_) folderProxy_->ShowFull(showFull);

    folderModel_->setRootPath(path);
    folderTree_->setRootIndex(folderProxy_->mapFromSource(folderModel_->index(path)));

    SetCurrentFolder(path, false);
}

void FileBrowserDock::SetCurrentFolder(const QString& path, bool recordHistory)
{
    if (path.isEmpty() || !QDir(path).exists())
        return;

    currentFolder_ = QDir::cleanPath(path);

    if (recordHistory) {
        if (historyIndex_ >= 0 && historyIndex_ < history_.size() - 1) {
            history_ = history_.mid(0, historyIndex_ + 1);
        }
        if (history_.isEmpty() || history_.last() != currentFolder_) {
            history_.append(currentFolder_);
            historyIndex_ = history_.size() - 1;
        }
    }

    if (btnNavBack_)
        btnNavBack_->setEnabled(historyIndex_ > 0);

    if (lblCurrentPath_) {
        QString rel = QDir(projectDir_).relativeFilePath(currentFolder_);
        if (rel.isEmpty() || rel == QStringLiteral("."))
            rel = QStringLiteral("Root");
        lblCurrentPath_->setText(QStringLiteral("/Game/%1").arg(rel));
        lblCurrentPath_->setToolTip(currentFolder_);
    }

    const QModelIndex srcIdx = model_->setRootPath(currentFolder_);
    const QModelIndex proxyIdx = proxy_->mapFromSource(srcIdx);
    if (gridView_)
        gridView_->setRootIndex(proxyIdx);
    if (view_)
        view_->setRootIndex(proxyIdx);

    const QModelIndex fSrcIdx = folderModel_->index(currentFolder_);
    if (folderTree_ && fSrcIdx.isValid()) {
        const QModelIndex fProxyIdx = folderProxy_->mapFromSource(fSrcIdx);
        folderTree_->setCurrentIndex(fProxyIdx);
        folderTree_->scrollTo(fProxyIdx);
    }
}

void FileBrowserDock::NavigateBack()
{
    if (historyIndex_ > 0) {
        --historyIndex_;
        SetCurrentFolder(history_[historyIndex_], false);
    }
}

void FileBrowserDock::NavigateUp()
{
    QDir dir(currentFolder_);
    if (dir.cdUp()) {
        const QString parentPath = dir.absolutePath();
        if (parentPath.startsWith(projectDir_)) {
            SetCurrentFolder(parentPath, true);
        }
    }
}

void FileBrowserDock::UpdateTileSize(int size)
{
    tileSize_ = qBound(48, size, 140);
    if (gridView_) {
        gridView_->setIconSize(QSize(tileSize_, tileSize_));
        gridView_->setGridSize(QSize(tileSize_ + 24, tileSize_ + 36));
    }
}

void FileBrowserDock::ApplyFilter(const QString& text)
{
    QStringList filters;
    for (const QString& part : text.split(QRegularExpression(QStringLiteral("[;\\s]+")), Qt::SkipEmptyParts))
        filters << (part.contains('*') ? part.trimmed() : QStringLiteral("*") + part.trimmed() + QStringLiteral("*"));
    if (filters.isEmpty())
        filters << QStringLiteral("*");
    model_->setNameFilters(filters);
}

void FileBrowserDock::OnDoubleClicked(const QModelIndex& proxyIndex)
{
    const auto index = proxy_->mapToSource(proxyIndex);
    if (!index.isValid()) return;

    if (model_->isDir(index)) {
        SetCurrentFolder(model_->filePath(index));
        return;
    }

    const QString path = model_->filePath(index);
    if (path.endsWith(QStringLiteral(".scene.json"), Qt::CaseInsensitive)) {
        if (auto* editor = qobject_cast<EditorMainWindow*>(window())) editor->OpenScene(path);
        return;
    }
    if (path.endsWith(QStringLiteral(".mat"), Qt::CaseInsensitive)) {
        if (auto* editor = qobject_cast<EditorMainWindow*>(window())) editor->OpenMaterialEditor(path);
        return;
    }

    const QByteArray utf8 = path.toUtf8();
    const std::string msg = "EditorOpenAsset: " + std::string(utf8.constData(), utf8.size());
    host_->Log(msg.c_str());

    if (auto* editor = qobject_cast<EditorMainWindow*>(window())) editor->UseAsset(path);
}

void FileBrowserDock::OnContextMenu(const QPoint& pos, bool isGrid)
{
    QAbstractItemView* currentView = isGrid ? static_cast<QAbstractItemView*>(gridView_) : static_cast<QAbstractItemView*>(view_);
    const QModelIndex proxyIndex = currentView->indexAt(pos);
    const QModelIndex index = proxy_->mapToSource(proxyIndex);
    const bool hasFile = index.isValid();
    const QString path = hasFile ? model_->filePath(index) : QString();

    QMenu menu(this);
    if (hasFile) {
        const QFileInfo fi(path);
        const QString ext = fi.suffix().toLower();
        const bool isDir = model_->isDir(index);
        const bool isMat = (ext == "mat");
        const bool isModel = (ext == "fbx" || ext == "obj" || ext == "blend" || ext == "gltf" || ext == "glb");
        const bool isTex = (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tga" || ext == "dds");
        const bool isScene = (ext == "json" && path.endsWith(".scene.json", Qt::CaseInsensitive));

        if (isDir) {
            auto* actOpenFolder = menu.addAction(Tr("Открыть папку", "Open Folder"));
            connect(actOpenFolder, &QAction::triggered, this, [this, path] { SetCurrentFolder(path); });
        } else {
            if (isMat) {
                auto* actUse = menu.addAction(Tr("Назначить материал на выбранное", "Assign Material to Selection"));
                connect(actUse, &QAction::triggered, this, [this, path] {
                    if (auto* editor = qobject_cast<EditorMainWindow*>(window())) editor->UseAsset(path);
                });
                auto* actEdit = menu.addAction(Tr("Редактировать материал…", "Edit Material…"));
                connect(actEdit, &QAction::triggered, this, [this, path] {
                    if (auto* editor = qobject_cast<EditorMainWindow*>(window())) editor->OpenMaterialEditor(path);
                });
            } else if (isModel) {
                auto* actUse = menu.addAction(Tr("Добавить модель в сцену", "Add Model to Scene"));
                connect(actUse, &QAction::triggered, this, [this, path] {
                    if (auto* editor = qobject_cast<EditorMainWindow*>(window())) editor->UseAsset(path);
                });
            } else if (isTex) {
                auto* actUse = menu.addAction(Tr("Назначить текстуру", "Assign Texture"));
                connect(actUse, &QAction::triggered, this, [this, path] {
                    if (auto* editor = qobject_cast<EditorMainWindow*>(window())) editor->UseAsset(path);
                });
            } else if (isScene) {
                auto* actOpenScene = menu.addAction(Tr("Открыть сцену", "Open Scene"));
                connect(actOpenScene, &QAction::triggered, this, [this, path] {
                    if (auto* editor = qobject_cast<EditorMainWindow*>(window())) editor->OpenScene(path);
                });
            }
            auto* actOpenApp = menu.addAction(Tr("Открыть во внешней программе", "Open in External App"));
            connect(actOpenApp, &QAction::triggered, this, [path] {
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            });
        }

        menu.addSeparator();
        auto* actRename = menu.addAction(Tr("Переименовать (F2)", "Rename (F2)"));
        connect(actRename, &QAction::triggered, this, [this, path] { RenameItem(path); });

        if (!isDir) {
            auto* actDuplicate = menu.addAction(Tr("Дублировать (Ctrl+D)", "Duplicate (Ctrl+D)"));
            connect(actDuplicate, &QAction::triggered, this, [this, path] { DuplicateItem(path); });
        }

        auto* actDelete = menu.addAction(Tr("Удалить (Del)", "Delete (Del)"));
        connect(actDelete, &QAction::triggered, this, [this, path] { DeleteItem(path); });

        menu.addSeparator();
        auto* actReveal = menu.addAction(Tr("Показать в Проводнике", "Show in Explorer"));
        connect(actReveal, &QAction::triggered, this, [path] {
            QProcess::startDetached(QStringLiteral("explorer.exe"),
                { QStringLiteral("/select,") + QDir::toNativeSeparators(path) });
        });
        auto* actCopy = menu.addAction(Tr("Копировать путь", "Copy Path"));
        connect(actCopy, &QAction::triggered, this, [path] {
            QGuiApplication::clipboard()->setText(path);
        });
    } else {
        // Пустое место
        auto* actNewFolder = menu.addAction(Tr("📁 Новая папка", "📁 New Folder"));
        connect(actNewFolder, &QAction::triggered, this, [this] { CreateNewFolder(); });

        auto* actNewMat = menu.addAction(Tr("🔮 Новый Материал (.mat)", "🔮 New Material (.mat)"));
        connect(actNewMat, &QAction::triggered, this, [this] { CreateNewMaterial(); });

        auto* actNewMatInst = menu.addAction(Tr("✨ Новый Material Instance", "✨ New Material Instance"));
        connect(actNewMatInst, &QAction::triggered, this, [this] { CreateNewMaterialInstance(); });

        auto* actNewScene = menu.addAction(Tr("🌐 Новая сцена (.scene.json)", "🌐 New Scene (.scene.json)"));
        connect(actNewScene, &QAction::triggered, this, [this] { CreateNewScene(); });

        auto* actNewCpp = menu.addAction(Tr("💻 C++ Класс...", "💻 C++ Class..."));
        connect(actNewCpp, &QAction::triggered, this, [this] { CreateNewCppClass(); });

        menu.addSeparator();
        auto* actImport = menu.addAction(Tr("Импорт ассета...", "Import Asset..."));
        connect(actImport, &QAction::triggered, this, [this] {
            if (auto* editor = qobject_cast<EditorMainWindow*>(window())) editor->ImportAsset(false);
        });

        menu.addSeparator();
        auto* actRevealCur = menu.addAction(Tr("Показать текущую папку в Проводнике", "Show Current Folder in Explorer"));
        connect(actRevealCur, &QAction::triggered, this, [this] {
            QProcess::startDetached(QStringLiteral("explorer.exe"), { QDir::toNativeSeparators(currentFolder_) });
        });
        auto* actRefresh = menu.addAction(Tr("Обновить", "Refresh"));
        connect(actRefresh, &QAction::triggered, this, [this] {
            model_->setRootPath(model_->rootPath());
            folderModel_->setRootPath(folderModel_->rootPath());
        });
    }

    menu.exec(currentView->viewport()->mapToGlobal(pos));
}

void FileBrowserDock::CreateNewFolder()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, Tr("Новая папка", "New Folder"),
        Tr("Имя папки:", "Folder Name:"), QLineEdit::Normal, Tr("NewFolder", "NewFolder"), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        QDir(currentFolder_).mkdir(name.trimmed());
    }
}

void FileBrowserDock::CreateNewMaterial()
{
    if (auto* editor = qobject_cast<EditorMainWindow*>(window())) {
        editor->CreateMaterial(currentFolder_);
    }
}

void FileBrowserDock::CreateNewMaterialInstance()
{
    QString baseName = QStringLiteral("M_Instance_");
    int idx = 1;
    QString fileName;
    while (true) {
        fileName = QStringLiteral("%1%2.mat").arg(baseName).arg(idx, 2, 10, QChar('0'));
        if (!QFile::exists(QDir(currentFolder_).filePath(fileName)))
            break;
        ++idx;
    }
    QString fullPath = QDir(currentFolder_).filePath(fileName);
    QFile f(fullPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonObject root;
        root["version"] = 1;
        root["name"] = QFileInfo(fullPath).baseName();
        root["parent"] = QStringLiteral("Content/Materials/Default.mat");
        root["isInstance"] = true;
        QJsonDocument doc(root);
        f.write(doc.toJson(QJsonDocument::Indented));
        f.close();
    }
    if (auto* editor = qobject_cast<EditorMainWindow*>(window())) {
        editor->OpenMaterialEditor(fullPath);
    }
}

void FileBrowserDock::CreateNewScene()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, Tr("Новая сцена", "New Scene"),
        Tr("Имя сцены:", "Scene Name:"), QLineEdit::Normal, Tr("NewLevel", "NewLevel"), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        if (!name.endsWith(QStringLiteral(".scene.json"), Qt::CaseInsensitive))
            name += QStringLiteral(".scene.json");
        QString fullPath = QDir(currentFolder_).filePath(name);
        QFile f(fullPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QJsonObject root;
            root["version"] = 1;
            root["objects"] = QJsonArray();
            QJsonDocument doc(root);
            f.write(doc.toJson(QJsonDocument::Indented));
            f.close();
        }
        if (auto* editor = qobject_cast<EditorMainWindow*>(window())) {
            editor->OpenScene(fullPath);
        }
    }
}

void FileBrowserDock::CreateNewCppClass()
{
    CodeClassDialog dialog(projectDir_, this);
    dialog.exec();
}

void FileBrowserDock::RenameItem(const QString& path)
{
    QFileInfo fi(path);
    if (!fi.exists()) return;
    bool ok = false;
    QString newName = QInputDialog::getText(this, Tr("Переименовать", "Rename"),
        Tr("Новое имя:", "New Name:"), QLineEdit::Normal, fi.fileName(), &ok);
    if (ok && !newName.trimmed().isEmpty() && newName != fi.fileName()) {
        QString newPath = fi.dir().filePath(newName.trimmed());
        QFile::rename(path, newPath);
    }
}

void FileBrowserDock::DuplicateItem(const QString& path)
{
    QFileInfo fi(path);
    if (!fi.exists() || fi.isDir()) return;
    QString baseName = fi.baseName() + QStringLiteral("_Copy");
    QString suffix = fi.completeSuffix();
    QString newPath = fi.dir().filePath(baseName + (suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix));
    int i = 1;
    while (QFile::exists(newPath)) {
        newPath = fi.dir().filePath(QStringLiteral("%1_%2.%3").arg(baseName).arg(i++).arg(suffix));
    }
    QFile::copy(path, newPath);
}

void FileBrowserDock::DeleteItem(const QString& path)
{
    QFileInfo fi(path);
    if (!fi.exists()) return;
    auto res = QMessageBox::question(this, Tr("Удалить ассет", "Delete Asset"),
        Tr("Вы уверены, что хотите удалить «%1»?", "Are you sure you want to delete '%1'?").arg(fi.fileName()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (res == QMessageBox::Yes) {
        if (fi.isDir()) {
            QDir(path).removeRecursively();
        } else {
            QFile::remove(path);
        }
    }
}
