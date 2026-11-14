#include "AssetPickerDialog.h"
#include "AssetThumbnailCache.h"
#include "EditorLocale.h"
#include "EditorTheme.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

AssetPickerDialog::ExecHook AssetPickerDialog::s_execHook = nullptr;

void AssetPickerDialog::SetExecHookForTesting(ExecHook hook)
{
    s_execHook = std::move(hook);
}

AssetPickerDialog::AssetPickerDialog(
    EditorCore::AssetKind kind,
    const QString& contentRoot,
    const QString& currentVirtualPath,
    QWidget* parent,
    const QString& slotName,
    bool allowEmpty,
    bool allowImport
)
    : QDialog(parent)
    , kind_(kind)
    , contentRoot_(contentRoot)
    , currentVirtualPath_(currentVirtualPath)
    , slotName_(slotName)
    , allowEmpty_(allowEmpty)
    , allowImport_(allowImport)
{
    if (kind_ == EditorCore::AssetKind::Texture)
    {
        setObjectName(QStringLiteral("MaterialTexturePicker"));
        if (slotName_.isEmpty())
            setWindowTitle(Tr("Выбор текстуры", "Choose Texture"));
        else
            setWindowTitle(Tr("Текстура: %1", "Texture: %1").arg(slotName_));
    }
    else if (kind_ == EditorCore::AssetKind::Model)
    {
        setObjectName(QStringLiteral("AssetPickerDialog"));
        setWindowTitle(Tr("Выбор модели", "Choose Model"));
    }
    else
    {
        setObjectName(QStringLiteral("AssetPickerDialog"));
        setWindowTitle(Tr("Выбор материала", "Choose Material"));
    }

    resize(640, 480);

    auto* layout = new QVBoxLayout(this);

    searchEdit_ = new QLineEdit(this);
    if (kind_ == EditorCore::AssetKind::Texture)
        searchEdit_->setObjectName(QStringLiteral("MaterialTextureSearch"));
    else
        searchEdit_->setObjectName(QStringLiteral("AssetPickerSearch"));
    searchEdit_->setPlaceholderText(Tr("Поиск по имени", "Search by name"));
    layout->addWidget(searchEdit_);

    listWidget_ = new QListWidget(this);
    if (kind_ == EditorCore::AssetKind::Texture)
        listWidget_->setObjectName(QStringLiteral("MaterialTextureList"));
    else
        listWidget_->setObjectName(QStringLiteral("AssetPickerList"));

    listWidget_->setViewMode(QListView::IconMode);
    listWidget_->setResizeMode(QListView::Adjust);
    listWidget_->setMovement(QListView::Static);
    listWidget_->setUniformItemSizes(true);
    listWidget_->setIconSize({96, 96});
    listWidget_->setGridSize({124, 132});
    listWidget_->setWordWrap(true);
    layout->addWidget(listWidget_, 1);

    auto* bottom = new QHBoxLayout;
    if (allowImport_)
    {
        importBtn_ = new QPushButton(Tr("Импортировать файл…", "Import File…"), this);
        importBtn_->setObjectName(QStringLiteral("MaterialTextureImport"));
        importBtn_->setToolTip(Tr("Файл вне проекта будет скопирован в Content/Textures",
                                  "File outside project will be copied to Content/Textures"));
        bottom->addWidget(importBtn_);
        connect(importBtn_, &QPushButton::clicked, this, &AssetPickerDialog::OnImportClicked);
    }

    bottom->addStretch(1);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto* okBtn = buttonBox_->button(QDialogButtonBox::Ok))
        okBtn->setObjectName(QStringLiteral("AssetPickerOk"));
    if (auto* cancelBtn = buttonBox_->button(QDialogButtonBox::Cancel))
        cancelBtn->setObjectName(QStringLiteral("AssetPickerCancel"));

    bottom->addWidget(buttonBox_);
    layout->addLayout(bottom);

    connect(buttonBox_, &QDialogButtonBox::accepted, this, &AssetPickerDialog::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(listWidget_, &QListWidget::itemActivated, this, &AssetPickerDialog::accept);

    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        const QString query = text.trimmed();
        for (int i = 0; i < listWidget_->count(); ++i)
        {
            auto* item = listWidget_->item(i);
            const QString virt = item->data(Qt::UserRole).toString();
            if (virt.isEmpty())
            {
                item->setHidden(!query.isEmpty());
            }
            else
            {
                const bool match = query.isEmpty() ||
                    item->text().contains(query, Qt::CaseInsensitive) ||
                    item->toolTip().contains(query, Qt::CaseInsensitive);
                item->setHidden(!match);
            }
        }
    });

    PopulateItems();
}

AssetPickerDialog::~AssetPickerDialog() = default;

int AssetPickerDialog::exec()
{
    if (s_execHook)
        return s_execHook(this);
    return QDialog::exec();
}

void AssetPickerDialog::accept()
{
    if (const auto* item = listWidget_->currentItem())
    {
        selectedVirtualPath_ = item->data(Qt::UserRole).toString();
        selectedDiskPath_ = item->data(Qt::UserRole + 1).toString();
        QDialog::accept();
    }
}

void AssetPickerDialog::PopulateItems()
{
    listWidget_->setUpdatesEnabled(false);
    const qreal dpr = devicePixelRatioF();
    const QIcon fallbackIcon(AssetThumbnailCache::CategoryIcon(kind_, 96, dpr));

    if (allowEmpty_)
    {
        auto* none = new QListWidgetItem(Tr("Нет текстуры", "No Texture"), listWidget_);
        none->setData(Qt::UserRole, QString());
        none->setData(Qt::UserRole + 1, QString());
        none->setIcon(fallbackIcon);
        if (currentVirtualPath_.isEmpty())
            listWidget_->setCurrentItem(none);
    }

    const auto items = EditorCore::ListProjectAssets(contentRoot_, kind_);
    for (const auto& item : items)
    {
        auto* listItem = new QListWidgetItem(item.fileName, listWidget_);
        listItem->setToolTip(item.relativePath);
        listItem->setData(Qt::UserRole, item.virtualPath);
        listItem->setData(Qt::UserRole + 1, item.fullPath);

        if (kind_ == EditorCore::AssetKind::Texture)
        {
            const QPixmap initialIcon = AssetThumbnailCache::Instance().GetOrCreate(
                kind_,
                item.fullPath,
                item.lastModified,
                96,
                dpr,
                this,
                [listItem](const QPixmap& loaded) {
                    listItem->setIcon(QIcon(loaded));
                }
            );
            listItem->setIcon(QIcon(initialIcon));
        }
        else
        {
            listItem->setIcon(fallbackIcon);
        }

        if (!currentVirtualPath_.isEmpty())
        {
            if (item.virtualPath.compare(currentVirtualPath_, Qt::CaseInsensitive) == 0 ||
                item.fullPath.compare(currentVirtualPath_, Qt::CaseInsensitive) == 0 ||
                item.relativePath.compare(currentVirtualPath_, Qt::CaseInsensitive) == 0)
            {
                listWidget_->setCurrentItem(listItem);
                listWidget_->scrollToItem(listItem);
            }
        }
    }
    listWidget_->setUpdatesEnabled(true);
}

void AssetPickerDialog::OnImportClicked()
{
    const QString filter = EditorCore::DialogFilter(EditorCore::AssetKind::Texture);
    const QString file = QFileDialog::getOpenFileName(
        this,
        Tr("Импорт текстуры", "Import Texture"),
        QString(),
        filter
    );

    if (file.isEmpty())
        return;

    selectedDiskPath_ = file;
    selectedVirtualPath_ = file;
    QDialog::accept();
}
