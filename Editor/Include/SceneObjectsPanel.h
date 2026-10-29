#pragma once

#include <QWidget>
#include <QString>
#include <QVector>
#include <utility>
#include <array>
#include <cstdint>

#include "QtHost.h"

class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QTreeWidget;
class QAction;
class QGroupBox;
class QLabel;

struct EditorSnapshot;

class SceneObjectsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SceneObjectsPanel(QWidget* parent = nullptr);
    ~SceneObjectsPanel() override;

    void SetEnabled(bool enabled);

    // pair: first = label, second = internal type name
    void SetPrimitiveTypes(const QVector<std::pair<QString, QString>>& types);

    void SetShowServiceObjects(bool visible);

    void RefreshHierarchy(const EditorQtHost::Snapshot& view);

    std::array<double, 3> SpawnPosition() const;

    void RetranslateUi();
    QSize minimumSizeHint() const override { return QSize(140, 80); }

signals:
    void AddPrimitiveRequested(
        const QString& typeName,
        const QString& typeLabel,
        const std::array<double, 3>& position
    );

    void AddModelRequested();
    void ImportModelRequested();

    void DuplicateRequested();
    void RemoveRequested();

    void SelectionChanged(quint64 objectId);
    void FocusRequested();

private:
    void BuildUi();
    void FillTypes();

    QGroupBox* placementGroup_ = nullptr;
    QLabel* categoryLabel_ = nullptr;
    QLabel* objectLabel_ = nullptr;
    QLabel* spawnPointLabel_ = nullptr;
    QAction* actDuplicate_ = nullptr;
    QAction* actRemove_ = nullptr;

    QComboBox* typeCategory_ = nullptr;
    QVector<std::pair<QString, QString>> types_;
    bool showServiceObjects_ = false;

    QComboBox* primitive_ = nullptr;
    QDoubleSpinBox* spawn_[3]{};

    QPushButton* addPrimitiveButton_ = nullptr;

    QTreeWidget* hierarchy_ = nullptr;

    QAction* removeShortcut_ = nullptr;

    QString hierarchyKey_;
    QString primitiveKey_;

    bool suppressSelectionChanged_ = false;
    int langSubId_ = 0;
};
