#pragma once

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
    static EditorSettings& Instance();

    ControlScheme GetControlScheme() const;
    void SetControlScheme(ControlScheme scheme);

    int SubscribeControlSchemeChanged(std::function<void(ControlScheme)> callback);
    void UnsubscribeControlSchemeChanged(int id);

private:
    EditorSettings() = default;

    ControlScheme scheme_ = ControlScheme::UnrealUnity;
    std::vector<std::pair<int, std::function<void(ControlScheme)>>> subscribers_;
    int nextSubId_ = 1;
};

