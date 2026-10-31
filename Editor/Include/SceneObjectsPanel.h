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

struct EditorSnapshot;

class SceneObjectsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SceneObjectsPanel(QWidget* parent = nullptr);

    void SetEnabled(bool enabled);

    // pair: first = label, second = internal type name
    void SetPrimitiveTypes(const QVector<std::pair<QString, QString>>& types);

    void SetShowServiceObjects(bool visible);

    void RefreshHierarchy(const EditorQtHost::Snapshot& view);

    std::array<double, 3> SpawnPosition() const;

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

    void SelectionChanged(quint64 objectId, const QVector<quint64>& objectIds);
    void FocusRequested();

private:
    void BuildUi();
    void FillTypes();
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
};
