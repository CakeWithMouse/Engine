#pragma once

#include "EditorLocale.h"
#include "IconProvider.h"
#include <QString>
#include <QIcon>
#include <string>

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

} // namespace EditorUI
