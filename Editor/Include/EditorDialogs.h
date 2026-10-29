#pragma once

#include <QString>
#include <QWidget>
#include <functional>
#include <vector>

enum class ControlScheme
{
    UnrealUnity,
    Blender
};

enum class SpawnLocationMode
{
    UnderCursor,
    WorldOrigin
};

class EditorSettings
{
public:
    static ControlScheme GetControlScheme();
    static void SetControlScheme(ControlScheme scheme);

    static SpawnLocationMode GetSpawnLocationMode();
    static void SetSpawnLocationMode(SpawnLocationMode mode);

    static int SubscribeControlSchemeChanged(std::function<void(ControlScheme)> callback);
    static void UnsubscribeControlSchemeChanged(int id);
};

void ShowDocumentationDialog(QWidget* parent, const QString& projectRoot);

