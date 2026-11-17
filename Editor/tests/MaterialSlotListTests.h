#pragma once

// Слоты материалов модели в карточке «Материал»: строки, кнопки, перетаскивание и перекрытие общим материалом.
#include "ObjectInspectorPanel.h"
#include "AssetPickerDialog.h"
#include "CommandRegistry.h"

#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QTemporaryDir>
#include <QToolButton>
#include <QUrl>
#include <iostream>
#include <stdexcept>

namespace MaterialSlotListTests
{
inline void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(std::string("MaterialSlotList check failed: ") + message);
}

inline void Touch(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) throw std::runtime_error("MaterialSlotList: cannot create fixture file");
    f.write("{}");
}

// Бросает mime на виджет так же, как Qt при отпускании мыши: сначала DragEnter, затем Drop.
inline bool Drop(QWidget* target, QMimeData* mime)
{
    const QPoint at = target->rect().center();
    QDragEnterEvent enter(at, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &enter);
    if (!enter.isAccepted()) return false;
    QDropEvent drop(QPointF(at), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &drop);
    return drop.isAccepted();
}

inline EditorQtHost::SceneObject Model(int slotCount, const QJsonObject& assigned, const std::string& shared)
{
    static const char* names[] = {"COLOR", "VETRO", "Roof", "Wall", "Glass", "Door", "Road", "Sign", "Tree", "Lamp"};
    QJsonArray fields;
    for (int i = 0; i < slotCount; ++i)
        fields.append(QJsonObject{{"path", QStringLiteral("/materialSlots/%1").arg(i)}, {"type", "asset"},
                                  {"extensions", QJsonArray{".mat"}}, {"label", QStringLiteral("%1: %2").arg(i).arg(names[i % 10])},
                                  {"group", QStringLiteral("Материалы модели")}});
    EditorQtHost::SceneObject model;
    model.id = 41;
    model.type = "Engine.Model";
    model.name = "City";
    model.modelAsset = "/Game/Models/city.glb";
    model.materialAsset = shared;
    model.transform.rotation[3] = 1.0f;
    model.transform.scale[0] = model.transform.scale[1] = model.transform.scale[2] = 1.0f;
    model.propertySchema = QJsonDocument(QJsonObject{{"version", 1}, {"fields", fields}}).toJson(QJsonDocument::Compact).toStdString();
    model.projectData = QJsonDocument(QJsonObject{{"version", 1}, {"materialSlots", assigned}}).toJson(QJsonDocument::Compact).toStdString();
    return model;
}

inline void Run()
{
    std::cout << "[Test] Running MaterialSlotListTests..." << std::endl;
    int checks = 0;
    QTemporaryDir root;
    Check(root.isValid(), "temp content root");
    QDir().mkpath(root.path() + "/Materials");
    QDir().mkpath(root.path() + "/Textures");
    Touch(root.path() + "/Materials/a.mat");
    Touch(root.path() + "/Materials/b.mat");
    Touch(root.path() + "/Textures/t.png");

    EditorQtHost::Snapshot snap;
    snap.objects = {Model(3, QJsonObject{{"0", "/Game/Materials/a.mat"}, {"1", ""}, {"2", "/Game/Materials/missing.mat"}}, "")};
    ObjectInspectorPanel panel;
    panel.setProperty("contentRoot", root.path());
    panel.resize(360, 900);
    panel.show();
    panel.Refresh(snap, 41, true);
    QApplication::processEvents();

    auto* row0 = panel.findChild<QFrame*>("MaterialSlot_0");
    auto* row1 = panel.findChild<QFrame*>("MaterialSlot_1");
    auto* row2 = panel.findChild<QFrame*>("MaterialSlot_2");
    Check(row0 && row1 && row2 && row0->isVisible(), "one compact row per model slot inside the Material card");
    Check(!panel.findChild<QLineEdit*>("Property/materialSlots/0"), "slots are not duplicated in the generic property form");
    Check(static_cast<MaterialElidedLabel*>(panel.findChild<QLabel*>("MaterialSlotName_1"))->FullText() == "VETRO", "row shows part name");
    Check(static_cast<MaterialElidedLabel*>(panel.findChild<QLabel*>("MaterialSlotPath_0"))->FullText() == "a", "row shows material name");
    auto* edit0 = panel.findChild<QToolButton*>("MaterialSlotEdit_0");
    Check(edit0 && edit0->isEnabled(), "assigned slot can be opened");
    Check(!panel.findChild<QToolButton*>("MaterialSlotEdit_1")->isEnabled(), "empty slot cannot be opened");
    Check(!panel.findChild<QToolButton*>("MaterialSlotEdit_2")->isEnabled(), "missing material cannot be opened");
    Check(!panel.findChild<QToolButton*>("MaterialSlotClear_1")->isEnabled(), "empty slot has nothing to clear");
    Check(!panel.findChild<QLineEdit*>("MaterialSlotFilter")->isVisibleTo(&panel), "filter hidden for few slots");
    Check(!panel.findChild<QLabel*>("MaterialSlotOverrideHint")->isVisibleTo(&panel), "no override hint without shared material");
    ++checks;

    QString opened;
    QObject::connect(&panel, &ObjectInspectorPanel::EditMaterialRequested, &panel, [&](const QString& p) { opened = p; });
    quint64 id = 0; QString path, value;
    QObject::connect(&panel, &ObjectInspectorPanel::PropertyEditingFinished, &panel, [&](quint64 i, const QString& p, const QString& v) { id = i; path = p; value = v; });
    edit0->click();
    Check(opened == "/Game/Materials/a.mat", "edit opens the slot material");
    panel.findChild<QToolButton*>("MaterialSlotClear_0")->click();
    Check(id == 41 && path == "/materialSlots/0" && value == "\"\"", "clear resets slot to default");
    ++checks;

    // Контент-браузер отдаёт файлы как URL.
    path.clear();
    auto* fromBrowser = new QMimeData;
    fromBrowser->setUrls({QUrl::fromLocalFile(root.path() + "/Materials/b.mat")});
    Check(Drop(row1, fromBrowser), "row accepts .mat from content browser");
    Check(path == "/materialSlots/1" && value == "\"/Game/Materials/b.mat\"", "drop assigns /Game path to that slot");
    delete fromBrowser;
    path.clear();
    auto* texture = new QMimeData;
    texture->setUrls({QUrl::fromLocalFile(root.path() + "/Textures/t.png")});
    Check(!Drop(row1, texture) && path.isEmpty(), "non-material files are rejected");
    delete texture;
    auto* fromSlot = new QMimeData;
    fromSlot->setData(MaterialSlotDrag::MimeType(), QByteArray("/Game/Materials/a.mat"));
    Check(Drop(row2, fromSlot) && path == "/materialSlots/2" && value == "\"/Game/Materials/a.mat\"", "row dragged onto row copies material");
    delete fromSlot;
    ++checks;

    // «…» открывает выбор ассетов с текущим материалом слота.
    path.clear();
    AssetPickerDialog::SetExecHookForTesting([](AssetPickerDialog* d) {
        for (int i = 0; i < d->ListWidget()->count(); ++i)
            if (d->ListWidget()->item(i)->data(Qt::UserRole).toString() == "/Game/Materials/b.mat") d->ListWidget()->setCurrentRow(i);
        d->accept();
        return int(QDialog::Accepted);
    });
    panel.findChild<QToolButton*>("MaterialSlotChoose_0")->click();
    AssetPickerDialog::SetExecHookForTesting(nullptr);
    Check(path == "/materialSlots/0" && value == "\"/Game/Materials/b.mat\"", "picker assigns chosen material to slot");
    ++checks;

    // Обновление с теми же слотами не пересоздаёт строки (нет мерцания, не теряется наведение).
    panel.Refresh(snap, 41, false);
    Check(panel.findChild<QFrame*>("MaterialSlot_0") == row0, "refresh keeps rows");
    snap.objects[0] = Model(3, QJsonObject{{"0", "/Game/Materials/b.mat"}}, "/Game/Materials/a.mat");
    panel.Refresh(snap, 41, false);
    QApplication::processEvents();
    Check(panel.findChild<QFrame*>("MaterialSlot_0") == row0, "value change keeps rows");
    Check(static_cast<MaterialElidedLabel*>(panel.findChild<QLabel*>("MaterialSlotPath_0"))->FullText() == "b", "value change updates row");
    Check(panel.findChild<QLabel*>("MaterialSlotOverrideHint")->isVisibleTo(&panel), "shared material shows override hint");
    ++checks;

    // Общее поле тоже принимает .mat.
    QString shared;
    CommandRegistry::Instance().RegisterCommand({QStringLiteral("Scene.AssignMaterial"), QStringLiteral("Assign"), QString(), QString(),
        [&](const CommandContext& ctx) { shared = ctx.parameters.value(QStringLiteral("material")).toString(); return true; }, [] { return true; }});
    auto* sharedMime = new QMimeData;
    sharedMime->setUrls({QUrl::fromLocalFile(root.path() + "/Materials/b.mat")});
    Check(Drop(panel.findChild<QLineEdit*>("MaterialAsset"), sharedMime) && shared == "/Game/Materials/b.mat", "shared field accepts dropped material");
    delete sharedMime;
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Scene.AssignMaterial"));
    ++checks;

    // Много слотов: фильтр по имени.
    auto* filter = panel.findChild<QLineEdit*>("MaterialSlotFilter");
    snap.objects[0] = Model(MaterialSlotList::kFilterThreshold, QJsonObject{}, "");
    panel.Refresh(snap, 41, false);
    QApplication::processEvents();
    Check(!filter->isVisibleTo(&panel), "filter hidden at the threshold");
    snap.objects[0] = Model(MaterialSlotList::kFilterThreshold + 1, QJsonObject{}, "");
    panel.Refresh(snap, 41, false);
    QApplication::processEvents();
    Check(filter->isVisibleTo(&panel), "filter shown above the threshold");
    filter->setText("vetro");
    Check(panel.findChild<QFrame*>("MaterialSlot_1")->isVisibleTo(&panel) && !panel.findChild<QFrame*>("MaterialSlot_0")->isVisibleTo(&panel), "filter hides other rows");
    filter->clear();
    ++checks;

    // Не модель: слоты остаются в общей форме свойств, карточка слотов пуста.
    auto probe = Model(1, QJsonObject{{"0", "/Game/Materials/a.mat"}}, "");
    probe.type = "UiProbe";
    snap.objects = {probe};
    panel.Refresh(snap, 41, true);
    QApplication::processEvents();
    Check(panel.findChild<QLineEdit*>("Property/materialSlots/0") != nullptr, "custom classes keep generic slot fields");
    Check(!panel.findChild<QFrame*>("MaterialSlot_0"), "no slot rows for non-model objects");
    ++checks;

    std::cout << "MaterialSlotListTests: " << checks << " checks passed." << std::endl;
}
} // namespace MaterialSlotListTests
