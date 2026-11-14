#pragma once

#include "AssetExtensions.h"

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QString>
#include <functional>

class QPushButton;
class QDialogButtonBox;

class AssetPickerDialog : public QDialog
{
public:
    using ExecHook = std::function<int(AssetPickerDialog*)>;
    static void SetExecHookForTesting(ExecHook hook);

    AssetPickerDialog(
        EditorCore::AssetKind kind,
        const QString& contentRoot,
        const QString& currentVirtualPath = QString(),
        QWidget* parent = nullptr,
        const QString& slotName = QString(),
        bool allowEmpty = false,
        bool allowImport = false
    );
    ~AssetPickerDialog() override;

    int exec() override;
    void accept() override;

    const QString& SelectedVirtualPath() const { return selectedVirtualPath_; }
    const QString& SelectedDiskPath() const { return selectedDiskPath_; }

    // Direct access for testing
    QLineEdit* SearchEdit() const { return searchEdit_; }
    QListWidget* ListWidget() const { return listWidget_; }
    QPushButton* ImportButton() const { return importBtn_; }

private:
    void PopulateItems();
    void OnImportClicked();

    EditorCore::AssetKind kind_;
    QString contentRoot_;
    QString currentVirtualPath_;
    QString slotName_;
    bool allowEmpty_ = false;
    bool allowImport_ = false;

    QLineEdit* searchEdit_ = nullptr;
    QListWidget* listWidget_ = nullptr;
    QPushButton* importBtn_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;

    QString selectedVirtualPath_;
    QString selectedDiskPath_;

    static ExecHook s_execHook;
};
