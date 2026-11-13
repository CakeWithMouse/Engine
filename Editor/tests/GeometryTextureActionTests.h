#pragma once

#include "GeometryCard.h"
#include "MaterialCard.h"
#include <QApplication>
#include <cstdio>
#include <stdexcept>

namespace GeometryTextureActionTests
{
inline void Run()
{
    int checks = 0;
    auto check = [&](bool value, const char* message) { if (!value) throw std::runtime_error(message); ++checks; };
    GeometryCard geometry;
    geometry.SetTexture(QStringLiteral("/Game/Textures/existing.png"));
    geometry.show();
    QApplication::processEvents();
    check(!geometry.findChild<QPushButton*>(QStringLiteral("ChooseTexture")), "GeometryCard has no redundant texture picker");
    check(!geometry.findChild<QPushButton*>(QStringLiteral("ClearTexture")), "GeometryCard has no redundant texture clear action");
    auto* texture = geometry.findChild<QLineEdit*>(QStringLiteral("TextureAsset"));
    check(texture && texture->isReadOnly(), "legacy object texture remains visible read-only");
    check(texture->text() == QStringLiteral("/Game/Textures/existing.png"), "existing object texture display preserved");
    check(geometry.findChild<QPushButton*>(QStringLiteral("ChooseModel")) && geometry.findChild<QPushButton*>(QStringLiteral("ClearModel")), "model actions preserved");
    geometry.Clear();
    check(texture->text().isEmpty(), "clearing inspector still clears texture display");
    geometry.SetTexture(QStringLiteral("/Game/Textures/restored.png"));
    geometry.RetranslateUi();
    check(texture->text() == QStringLiteral("/Game/Textures/restored.png"), "refresh and retranslation retain object texture");
    check(!geometry.grab().isNull(), "native geometry card paints without removed controls");
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
