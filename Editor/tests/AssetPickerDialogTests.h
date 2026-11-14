#pragma once

#include "AssetPickerDialog.h"
#include "AssetThumbnailCache.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <cstdio>
#include <stdexcept>

namespace AssetPickerDialogTests
{

inline void Run()
{
    int checks = 0;
    auto check = [&](bool value, const char* message) {
        if (!value)
            throw std::runtime_error(message);
        ++checks;
    };

    // 1. Tiles per resource and Kind filtering
    {
        QTemporaryDir temp;
        check(temp.isValid(), "Temporary directory created");
        const QString root = temp.path();

        QDir(root).mkpath(QStringLiteral("Models"));
        QDir(root).mkpath(QStringLiteral("Textures"));
        QDir(root).mkpath(QStringLiteral("Materials"));

        auto touch = [](const QString& path) {
            QFile f(path);
            f.open(QIODevice::WriteOnly);
            f.write("data");
            f.close();
        };

        touch(root + QStringLiteral("/Models/Hero.glb"));
        touch(root + QStringLiteral("/Models/Tree.fbx"));
        touch(root + QStringLiteral("/Textures/Albedo.png"));
        touch(root + QStringLiteral("/Textures/Normal.bmp"));
        touch(root + QStringLiteral("/Materials/Gold.mat"));
        touch(root + QStringLiteral("/Materials/Wood.mat"));

        // Models mode
        {
            AssetPickerDialog picker(EditorCore::AssetKind::Model, root);
            check(picker.objectName() == QStringLiteral("AssetPickerDialog"), "Model dialog objectName");
            auto* list = picker.findChild<QListWidget*>(QStringLiteral("AssetPickerList"));
            auto* search = picker.findChild<QLineEdit*>(QStringLiteral("AssetPickerSearch"));
            check(list != nullptr, "AssetPickerList exists for Model");
            check(search != nullptr, "AssetPickerSearch exists for Model");
            check(list->count() == 2, "Model picker lists only 2 models");
            check(list->item(0)->text() == QStringLiteral("Hero.glb"), "First model file name");
            check(list->item(1)->text() == QStringLiteral("Tree.fbx"), "Second model file name");
            check(!list->item(0)->icon().isNull(), "Model item has category icon");
        }

        // Materials mode
        {
            AssetPickerDialog picker(EditorCore::AssetKind::Material, root);
            auto* list = picker.findChild<QListWidget*>(QStringLiteral("AssetPickerList"));
            check(list != nullptr && list->count() == 2, "Material picker lists only 2 materials");
            check(list->item(0)->text() == QStringLiteral("Gold.mat"), "First material file name");
            check(list->item(1)->text() == QStringLiteral("Wood.mat"), "Second material file name");
            check(!list->item(0)->icon().isNull(), "Material item has category icon");
        }

        // Textures mode (MaterialTexturePicker compatibility)
        {
            AssetPickerDialog picker(EditorCore::AssetKind::Texture, root, QString(), nullptr,
                                    QStringLiteral("BaseMap"), true, true);
            check(picker.objectName() == QStringLiteral("MaterialTexturePicker"), "Texture dialog objectName");
            auto* list = picker.findChild<QListWidget*>(QStringLiteral("MaterialTextureList"));
            auto* search = picker.findChild<QLineEdit*>(QStringLiteral("MaterialTextureSearch"));
            auto* importBtn = picker.findChild<QPushButton*>(QStringLiteral("MaterialTextureImport"));
            check(list != nullptr, "MaterialTextureList exists");
            check(search != nullptr, "MaterialTextureSearch exists");
            check(importBtn != nullptr, "MaterialTextureImport exists");
            check(list->count() == 3, "Texture picker lists None + 2 textures");
            check(list->item(0)->text() == Tr("Нет текстуры", "No Texture"), "First item is No Texture");
            check(list->item(1)->text() == QStringLiteral("Albedo.png"), "First texture file name");
            check(list->item(2)->text() == QStringLiteral("Normal.bmp"), "Second texture file name");
        }
    }

    // 2. Search filtering
    {
        QTemporaryDir temp;
        const QString root = temp.path();
        QDir(root).mkpath(QStringLiteral("Models"));
        auto touch = [](const QString& path) {
            QFile f(path); f.open(QIODevice::WriteOnly); f.write("x"); f.close();
        };
        touch(root + QStringLiteral("/Models/Hero.glb"));
        touch(root + QStringLiteral("/Models/Tree.fbx"));

        AssetPickerDialog picker(EditorCore::AssetKind::Model, root);
        auto* list = picker.ListWidget();
        auto* search = picker.SearchEdit();

        search->setText(QStringLiteral("Tree"));
        check(list->item(0)->isHidden(), "Hero.glb hidden by filter");
        check(!list->item(1)->isHidden(), "Tree.fbx matches filter");

        search->clear();
        check(!list->item(0)->isHidden(), "Hero.glb visible after clear");
        check(!list->item(1)->isHidden(), "Tree.fbx visible after clear");
    }

    // 3. Selection returns path and preselects current resource
    {
        QTemporaryDir temp;
        const QString root = temp.path();
        QDir(root).mkpath(QStringLiteral("Materials"));
        auto touch = [](const QString& path) {
            QFile f(path); f.open(QIODevice::WriteOnly); f.write("x"); f.close();
        };
        touch(root + QStringLiteral("/Materials/Gold.mat"));
        touch(root + QStringLiteral("/Materials/Silver.mat"));

        AssetPickerDialog picker(EditorCore::AssetKind::Material, root, QStringLiteral("/Game/Materials/Silver.mat"));
        check(picker.ListWidget()->currentItem() != nullptr, "Current item preselected");
        check(picker.ListWidget()->currentItem()->text() == QStringLiteral("Silver.mat"), "Preselected matches current virtual path");

        AssetPickerDialog::SetExecHookForTesting([](AssetPickerDialog* dlg) {
            dlg->accept();
            return QDialog::Accepted;
        });

        check(picker.exec() == QDialog::Accepted, "Exec hook accepted");
        check(picker.SelectedVirtualPath() == QStringLiteral("/Game/Materials/Silver.mat"), "Selected virtual path returned");
        AssetPickerDialog::SetExecHookForTesting(nullptr);
    }

    // 4. UI does not block with 500 resources
    {
        QTemporaryDir temp;
        const QString root = temp.path();
        QDir(root).mkpath(QStringLiteral("Textures"));

        for (int i = 0; i < 500; ++i)
        {
            QFile f(root + QStringLiteral("/Textures/tex_%1.png").arg(i, 4, 10, QChar('0')));
            f.open(QIODevice::WriteOnly);
            f.write("PNG");
            f.close();
        }

        QElapsedTimer timer;
        timer.start();
        AssetPickerDialog picker(EditorCore::AssetKind::Texture, root, QString(), nullptr, QString(), false, false);
        const qint64 elapsedMs = timer.elapsed();
        std::printf("[Diag] 500 resources dialog open elapsed: %lld ms\n", elapsedMs);

        check(picker.ListWidget()->count() == 500, "Dialog contains all 500 resources");
        check(elapsedMs < 200, "Dialog opens instantly (< 200 ms) with 500 resources");

        QCoreApplication::processEvents();
    }

    // 5. Cache does not grow beyond limit
    {
        auto& cache = AssetThumbnailCache::Instance();
        cache.Clear();
        check(cache.Count() == 0, "Cache cleared");

        QPixmap dummy(16, 16);
        dummy.fill(Qt::red);

        for (int i = 0; i < 300; ++i)
        {
            cache.Insert(QStringLiteral("/path/to/test_%1.png").arg(i), 12345, 96, 1.0, dummy);
        }

        check(cache.Count() <= 256, "Cache capacity is bounded (<= 256 items)");
    }

    // 6. Safe destruction cancels pending async tasks
    {
        QTemporaryDir temp;
        const QString root = temp.path();
        QDir(root).mkpath(QStringLiteral("Textures"));
        for (int i = 0; i < 20; ++i)
        {
            QFile f(root + QStringLiteral("/Textures/async_%1.png").arg(i));
            f.open(QIODevice::WriteOnly);
            f.write("RAW");
            f.close();
        }

        auto* dlg = new AssetPickerDialog(EditorCore::AssetKind::Texture, root);
        delete dlg; // Immediate deletion
        QCoreApplication::processEvents();
        check(true, "Destroying dialog with pending async tasks does not crash");
    }

    // 7. Uniform margins around tile grid (Bug 2)
    {
        QTemporaryDir temp;
        check(temp.isValid(), "Temporary directory for margin tests created");
        const QString root = temp.path();
        QDir(root).mkpath(QStringLiteral("Models"));
        QFile f(root + QStringLiteral("/Models/Hero.glb"));
        f.open(QIODevice::WriteOnly);
        f.write("data");
        f.close();

        AssetPickerDialog picker(EditorCore::AssetKind::Model, root);
        picker.show();
        QCoreApplication::processEvents();

        auto* list = picker.ListWidget();
        check(list != nullptr, "List widget exists");
        check(list->count() > 0, "Item exists in list widget");

        const QRect r = list->visualItemRect(list->item(0));
        const int topM = r.top();
        const int leftM = r.left();
        check(topM > 0, "Top margin is non-zero (not stuck at zero)");
        check(leftM > 0, "Left margin is non-zero");
        check(topM == leftM, "Top margin equals left margin");

        // Verify top/left/right margin equality relative to the first tile when viewport fits tile
        const int tileWidth = r.width();
        const int frameWidth = list->frameWidth();
        const int targetVpWidth = tileWidth + leftM * 2;
        list->setFixedWidth(targetVpWidth + frameWidth * 2);
        QCoreApplication::processEvents();

        const QRect rSized = list->visualItemRect(list->item(0));
        const int sizedTop = rSized.top();
        const int sizedLeft = rSized.left();
        const int sizedRight = list->viewport()->width() - rSized.right() - 1;
        check(sizedTop == sizedLeft, "Uniform margin: top equals left");
        check(sizedLeft == sizedRight, "Uniform margin: left equals right");
        check(sizedTop == sizedRight, "Uniform margin: top equals right");
    }

    std::printf("AssetPickerDialog tests: %d checks passed\n", checks);
}

} // namespace AssetPickerDialogTests
