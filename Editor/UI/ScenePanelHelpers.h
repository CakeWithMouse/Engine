#pragma once

#include "EditorLocale.h"
#include "IconProvider.h"
#include <QString>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QVector>
#include <string>
#include <utility>
#include <functional>

namespace EditorUI
{

inline QString LocalizedTypeLabel(const QString& name, const QString& fallbackLabel)
{
    if (name == QStringLiteral("Primitive.Cube")) return Tr("Куб", "Cube");
    if (name == QStringLiteral("Primitive.Sphere")) return Tr("Сфера", "Sphere");
    if (name == QStringLiteral("Primitive.Plane")) return Tr("Плоскость", "Plane");
    if (name == QStringLiteral("Primitive.Cylinder")) return Tr("Цилиндр", "Cylinder");
    if (name == QStringLiteral("Primitive.Cone")) return Tr("Конус", "Cone");
    if (name == QStringLiteral("Primitive.Pyramid")) return Tr("Пирамида", "Pyramid");
    if (name == QStringLiteral("Scene.Folder")) return Tr("Папка", "Folder");
    if (name == QStringLiteral("Scene.Empty")) return Tr("Пустой объект", "Empty");
    if (name == QStringLiteral("Light.Point")) return Tr("Точечный свет", "Point Light");
    if (name == QStringLiteral("Light.Directional")) return Tr("Направленный свет", "Directional Light");
    if (name == QStringLiteral("Particle.Sparks")) return Tr("Искры", "Sparks");
    if (name == QStringLiteral("Particle.Smoke")) return Tr("Дым", "Smoke");
    if (name == QStringLiteral("Environment.Sky")) return Tr("Базовое небо", "Basic Sky");
    if (name == QStringLiteral("Gameplay.Character")) return Tr("Персонаж", "Character");
    if (name == QStringLiteral("Gameplay.Camera")) return Tr("Камера Play", "Play Camera");
    if (name == QStringLiteral("Gameplay.PlaySettings")) return Tr("Настройки Play", "Play Settings");
    if (name == QStringLiteral("Gameplay.Player")) return Tr("Игрок", "Player");
    return fallbackLabel;
}

inline QIcon GetSceneObjectIcon(const std::string& type)
{
    if (type == "Scene.Folder") return IconProvider::FolderIcon();
    if (type == "Scene.Empty") return IconProvider::GetIcon(QStringLiteral("circle-dotted"));
    if (type.find("Light") != std::string::npos) return IconProvider::BulbIcon();
    if (type.find("Camera") != std::string::npos) return IconProvider::GetIcon(QStringLiteral("camera"));
    if (type.find("Particles") != std::string::npos) return IconProvider::GetIcon(QStringLiteral("particles"));
    return IconProvider::CubeIcon();
}

inline bool IsEnvironmentObject(const std::string& type, uint32_t gameplayKind)
{
    return gameplayKind == 3 || type == "Gameplay.PlaySettings" || type == "Light.Directional" || type == "Environment.Sky";
}

inline void PopulateCategorizedCreateMenu(
    QMenu* menu,
    const QVector<std::pair<QString, QString>>& types,
    std::function<void(const QString& typeName, const QString& label)> onSelect,
    const QString& objectNamePrefix = QStringLiteral("CreateObject."))
{
    if (!menu) return;

    auto* actFolder = menu->addAction(IconProvider::FolderIcon(), Tr("Папка", "Folder"), [onSelect] {
        onSelect(QStringLiteral("Scene.Folder"), Tr("Папка", "Folder"));
    });
    actFolder->setObjectName(objectNamePrefix + QStringLiteral("Scene.Folder"));

    auto* actEmpty = menu->addAction(IconProvider::GetIcon(QStringLiteral("circle-dotted")), Tr("Пустой объект", "Empty"), [onSelect] {
        onSelect(QStringLiteral("Scene.Empty"), Tr("Пустой объект", "Empty"));
    });
    actEmpty->setObjectName(objectNamePrefix + QStringLiteral("Scene.Empty"));
    menu->addSeparator();

    auto* menuPrimitives = menu->addMenu(Tr("Примитивы", "Primitives"));
    auto* menuLights = menu->addMenu(Tr("Свет", "Lights"));
    auto* menuParticles = menu->addMenu(Tr("Частицы", "Particles"));
    auto* menuEnv = menu->addMenu(Tr("Окружение", "Environment"));
    auto* menuCharacters = menu->addMenu(Tr("Персонажи", "Characters"));
    auto* menuCameras = menu->addMenu(Tr("Камеры", "Cameras"));
    auto* menuProject = menu->addMenu(Tr("Классы проекта", "Project Classes"));

    for (const auto& type : types) {
        const auto& name = type.second;
        if (name == QStringLiteral("Gameplay.PlaySettings") || name == QStringLiteral("Scene.Folder") || name == QStringLiteral("Scene.Empty"))
            continue;
        const auto label = LocalizedTypeLabel(name, type.first);

        QMenu* target = menuProject;
        if (name.startsWith(QStringLiteral("Primitive."))) target = menuPrimitives;
        else if (name.startsWith(QStringLiteral("Light."))) target = menuLights;
        else if (name.startsWith(QStringLiteral("Particle."))) target = menuParticles;
        else if (name.startsWith(QStringLiteral("Environment."))) target = menuEnv;
        else if (name.startsWith(QStringLiteral("Gameplay.Character")) || name == QStringLiteral("Gameplay.Player")) target = menuCharacters;
        else if (name.startsWith(QStringLiteral("Gameplay.Camera"))) target = menuCameras;

        auto* action = target->addAction(label, [onSelect, name, label] {
            onSelect(name, label);
        });
        action->setObjectName(objectNamePrefix + name);
        action->setData(name);
    }

    for (auto* sub : {menuPrimitives, menuLights, menuParticles, menuEnv, menuCharacters, menuCameras, menuProject}) {
        if (sub->actions().isEmpty()) {
            sub->menuAction()->setVisible(false);
        }
    }
}

} // namespace EditorUI
