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

class EditorSettings
{
public:
    static ControlScheme GetControlScheme();
    static void SetControlScheme(ControlScheme scheme);

    static int SubscribeControlSchemeChanged(std::function<void(ControlScheme)> callback);
    static void UnsubscribeControlSchemeChanged(int id);
};

void ShowDocumentationDialog(QWidget* parent, const QString& projectRoot);

