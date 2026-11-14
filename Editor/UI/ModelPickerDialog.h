#pragma once

#include "AssetExtensions.h"
#include "EditorLocale.h"
#include "EditorTheme.h"
#include "IconProvider.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <functional>

class ModelPickerDialog : public QDialog
{
public:
    using ExecHook = std::function<int(ModelPickerDialog*)>;
    static void SetExecHookForTesting(ExecHook hook) { sExecHook = std::move(hook); }

    explicit ModelPickerDialog(const QString& contentRoot, const QString& currentVirtualPath = QString(), QWidget* parent = nullptr)
        : QDialog(parent)
        , contentRoot_(contentRoot)
    {
        setObjectName(QStringLiteral("ModelPickerDialog"));
        setWindowTitle(Tr("Заменить модель", "Replace Model"));
        resize(480, 360);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        search_ = new QLineEdit(this);
        search_->setObjectName(QStringLiteral("ModelPickerSearch"));
        search_->setPlaceholderText(Tr("Поиск модели…", "Search model…"));
        search_->setClearButtonEnabled(true);
        layout->addWidget(search_);

        list_ = new QListWidget(this);
        list_->setObjectName(QStringLiteral("ModelPickerList"));
        list_->setAlternatingRowColors(true);
        layout->addWidget(list_, 1);

        buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons_->setObjectName(QStringLiteral("ModelPickerButtons"));
        if (auto* okBtn = buttons_->button(QDialogButtonBox::Ok))
            okBtn->setObjectName(QStringLiteral("ModelPickerOk"));
        if (auto* cancelBtn = buttons_->button(QDialogButtonBox::Cancel))
            cancelBtn->setObjectName(QStringLiteral("ModelPickerCancel"));
        layout->addWidget(buttons_);

        PopulateList(currentVirtualPath);

        connect(search_, &QLineEdit::textChanged, this, [this](const QString& text) {
            for (int i = 0; i < list_->count(); ++i) {
                auto* item = list_->item(i);
                item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
            }
        });

        auto acceptSelection = [this] {
            if (auto* item = list_->currentItem()) {
                selectedModel_ = item->data(Qt::UserRole).toString();
                if (!selectedModel_.isEmpty())
                    accept();
            }
        };

        connect(buttons_, &QDialogButtonBox::accepted, this, acceptSelection);
        connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(list_, &QListWidget::itemDoubleClicked, this, [acceptSelection](QListWidgetItem*) {
            acceptSelection();
        });
    }

    int exec() override
    {
        if (sExecHook)
            return sExecHook(this);
        return QDialog::exec();
    }

    QString SelectedModel() const { return selectedModel_; }

    QLineEdit* SearchEdit() const { return search_; }
    QListWidget* ListWidget() const { return list_; }
    QDialogButtonBox* ButtonBox() const { return buttons_; }

private:
    void PopulateList(const QString& currentVirtualPath)
    {
        const auto models = EditorCore::ListProjectAssets(contentRoot_, EditorCore::AssetKind::Model);
        const QIcon modelIcon = IconProvider::CubeIcon();
        for (const auto& m : models) {
            auto* item = new QListWidgetItem(modelIcon, m.relativePath, list_);
            item->setData(Qt::UserRole, m.virtualPath);
            item->setToolTip(m.virtualPath);
            if (!currentVirtualPath.isEmpty() &&
                (m.virtualPath.compare(currentVirtualPath, Qt::CaseInsensitive) == 0 ||
                 m.relativePath.compare(currentVirtualPath, Qt::CaseInsensitive) == 0))
            {
                list_->setCurrentItem(item);
            }
        }
        if (!list_->currentItem() && list_->count() > 0) {
            list_->setCurrentRow(0);
        }
    }

    QString contentRoot_;
    QString selectedModel_;
    QLineEdit* search_ = nullptr;
    QListWidget* list_ = nullptr;
    QDialogButtonBox* buttons_ = nullptr;

    inline static ExecHook sExecHook;
};
