#pragma once

#include "GeometryCard.h"
#include "MaterialCard.h"
#include "AssetPickerDialog.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>
#include <stdexcept>

namespace GeometryTextureActionTests
{
inline void Run()
{
    int checks = 0;
    auto check = [&](bool value, const char* message) { if (!value) throw std::runtime_error(message); ++checks; };

    GeometryCard geometry;
    geometry.show();
    QApplication::processEvents();

    check(!geometry.findChild<QPushButton*>(QStringLiteral("ChooseTexture")), "GeometryCard has no texture picker");
    check(!geometry.findChild<QPushButton*>(QStringLiteral("ClearTexture")), "GeometryCard has no texture clear action");
    check(!geometry.findChild<QPushButton*>(QStringLiteral("ClearModel")), "GeometryCard has no ClearModel action");
    check(!geometry.findChild<QLineEdit*>(QStringLiteral("TextureAsset")), "GeometryCard has no TextureAsset row");

    auto* chooseModel = geometry.findChild<QPushButton*>(QStringLiteral("ChooseModel"));
    auto* modelAsset = geometry.findChild<QLineEdit*>(QStringLiteral("ModelAsset"));
    check(chooseModel != nullptr, "GeometryCard retains ChooseModel button");
    check(modelAsset != nullptr, "GeometryCard retains ModelAsset field");

    // Primitive data display and blocked button
    geometry.SetPrimitiveData(QStringLiteral("Cube"));
    check(modelAsset->text() == Tr("Примитив: Cube", "Primitive: Cube"), "Primitive displayed as Примитив: <тип>");
    check(!chooseModel->isEnabled(), "ChooseModel is disabled for primitives");
    check(!chooseModel->toolTip().isEmpty(), "ChooseModel has blocked tooltip for primitives");

    // Model data display and enabled button
    geometry.SetModelData(QStringLiteral("/Game/Models/CesiumMan.glb"), true);
    check(modelAsset->text() == QStringLiteral("Models/CesiumMan.glb"), "Model displayed as relative project path");
    check(chooseModel->isEnabled(), "ChooseModel is enabled for models");

    geometry.Clear();
    check(modelAsset->text().isEmpty(), "Clearing geometry card clears model text");
    check(!chooseModel->isEnabled(), "Clearing geometry card disables ChooseModel");

    geometry.SetModelData(QStringLiteral("/Game/Models/Hero.fbx"), true);
    geometry.RetranslateUi();
    check(chooseModel->text() == Tr("Заменить модель…", "Replace Model…"), "Button text is Replace Model");
    check(!geometry.grab().isNull(), "Native geometry card paints cleanly");

    // AssetPickerDialog (Model mode) tests (replaces legacy ModelPickerDialog)
    {
        QTemporaryDir temp;
        check(temp.isValid(), "Temporary directory created for picker test");
        const QString root = temp.path();

        QDir(root).mkpath(QStringLiteral("Models"));
        QDir(root).mkpath(QStringLiteral("Textures"));

        auto touch = [](const QString& filePath) {
            QFile f(filePath);
            f.open(QIODevice::WriteOnly);
            f.write("test");
            f.close();
        };

        touch(root + QStringLiteral("/Models/CesiumMan.glb"));
        touch(root + QStringLiteral("/Models/Hero.fbx"));
        touch(root + QStringLiteral("/Textures/Albedo.png"));

        AssetPickerDialog picker(EditorCore::AssetKind::Model, root, QStringLiteral("/Game/Models/CesiumMan.glb"));
        check(picker.ListWidget() != nullptr, "Picker list widget exists");
        check(picker.ListWidget()->count() == 2, "Picker lists only project models");
        check(picker.ListWidget()->currentItem() &&
              picker.ListWidget()->currentItem()->data(Qt::UserRole).toString() == QStringLiteral("/Game/Models/CesiumMan.glb"),
              "Current model preselected");

        // Search filtering
        picker.SearchEdit()->setText(QStringLiteral("Hero"));
        check(picker.ListWidget()->item(0)->isHidden(), "Non-matching model is hidden");
        check(!picker.ListWidget()->item(1)->isHidden(), "Matching model remains visible");

        picker.SearchEdit()->clear();
        check(!picker.ListWidget()->item(0)->isHidden(), "Clearing search restores visibility");

        // Test hook
        AssetPickerDialog::SetExecHookForTesting([](AssetPickerDialog* dlg) {
            dlg->ListWidget()->setCurrentRow(1);
            return QDialog::Accepted;
        });
        check(picker.exec() == QDialog::Accepted, "Picker test hook accepts");
        AssetPickerDialog::SetExecHookForTesting(nullptr);
    }

    // MaterialCard tests preserved
    MaterialCard material;
    material.SetMaterialsAvailable(true);
    material.SetData(QStringLiteral("/Game/Materials/test.mat"), false);
    material.show();
    auto* edit = material.findChild<QPushButton*>(QStringLiteral("EditMaterial"));
    check(edit && edit->isEnabled(), "MaterialCard retains enabled editor entry");
    QString requested;
    int requests = 0;
    material.onEditMaterial = [&](const QString& path) { requested = path; ++requests; };
    edit->click();
    check(requested == QStringLiteral("/Game/Materials/test.mat") && requests == 1, "existing material editor path forwarded");
    material.SetData({}, false);
    edit->click();
    check(requested.isEmpty() && requests == 2, "empty material retains create-and-edit entry");
    material.SetMaterialsAvailable(false);
    check(!edit->isEnabled(), "legacy module capability guard retained");
    edit->click();
    check(requests == 2, "unavailable material editing cannot dispatch");

    std::printf("Geometry texture action tests: %d checks passed\n", checks);
}
}
