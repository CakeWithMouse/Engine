#pragma once
#include <Engine/Runtime/ModuleApi.h>
#include <Engine/Runtime/EditSessionApi.h>
#include <Engine/Runtime/ProjectDataApi.h>
#include <Engine/Runtime/SelectionApi.h>
#include <Engine/Runtime/ViewportInputApi.h>
#include <Engine/Runtime/ViewportOverlayApi.h>

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <QString>

enum EditorGizmoMode : uint32_t
{
    EditorGizmoTranslate = 0,
    EditorGizmoRotate = 1,
    EditorGizmoScale = 2
};

struct EditorCommand
{
    enum Type : uint32_t
    {
        None = 0, RequestExit, ReloadProject, LoadAsset, SetModel,
        ViewportAttach, ViewportResize, ViewportDetach, ViewportCamera,
        TogglePlay, TogglePause, InputFocus, InputKey, InputPointer, ConfigurePlay,
        FetchScene, UpdateObjectTransform, RefreshScene, SelectObject, CreateObject, RemoveObject,
        RenameObject, DuplicateObject, SaveScene, LoadScene, NewScene, Undo, Redo, PickObject, FocusObject,
        SetGizmoMode, BeginGizmoDrag, EndGizmoDrag, SetParent,
        ImportModel, ImportTexture, CreateModel, SetTexture, SetPointLight, SetMaterial, SetProjectData,
        RunTask,
    };

    Type type = None;
    float right = 0, up = 0, forward = 0, yaw = 0, pitch = 0, zoom = 0;
    QString path;
    void* hwnd = nullptr;
    uint32_t w = 0, h = 0;

    // Поля для манипуляций с объектами
    uint64_t objectId = 0;
    uint64_t parentId = 0;
    EngineSceneTransform transform{{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}};
    EngineScenePointLight pointLight{};
    EditorGizmoMode gizmo = EditorGizmoTranslate;
    QString name;

    // RunTask: выполняется в потоке движка, если есть сессия сцены (редактор материалов)
    std::function<void(const EngineEditSessionApi&)> task;
};

class IEditorUiListener
{
public:
    virtual ~IEditorUiListener() = default;
    virtual void OnLogMessage(const QString& line) = 0;
    virtual void OnCommandFinished(const EditorCommand& command, uint32_t status, const QString& error = {}) = 0;
};

class IEditorHost
{
public:
    virtual ~IEditorHost() = default;

    enum EditorState : int
    {
        Edit = 0,
        Play = 1,
        Pause = 2
    };

    struct SceneObject
    {
        uint64_t id = 0, parent = 0;
        bool inheritScale = true;
        std::string name, type;
        EngineSceneTransform transform{};
        std::string modelAsset, textureAsset;
        bool hasPointLight = false;
        EngineScenePointLight pointLight{};
        std::string materialAsset;
        std::string projectData;
        uint32_t gameplayKind = 0;
    };

    struct SceneType
    {
        std::string name, label;
    };

    struct Snapshot
    {
        std::vector<SceneObject> objects;
        std::vector<SceneType> types;
        uint64_t selected = 0, serial = 0;
        bool dirty = false, canUndo = false, canRedo = false;
    };

    virtual void PostCommand(const EditorCommand& c) = 0;
    virtual Snapshot ReadSnapshot() = 0;
    virtual void Log(const char* text) = 0;

    virtual bool HasScene() const = 0;
    virtual bool HasMaterials() const = 0;
    virtual bool HasViewport() const = 0;
    virtual bool ViewportActive() const = 0;
    virtual EditorState PlayState() const = 0;

    virtual double LastDt() const = 0;
    virtual uint64_t UpdateCount() const = 0;
    virtual const EngineModuleHost& host() const = 0;
    virtual const EngineEditSessionApi& scene() const = 0;

    virtual void MoveSelectedObject(float right, float up, float forward) = 0;
    virtual void FetchSceneAndSelectNext() = 0;
    virtual bool HasSelection() const = 0;
    virtual std::string GetSelectedObjectName() = 0;

    virtual void SetGizmoMode(EditorGizmoMode mode) = 0;
    virtual EditorGizmoMode GizmoMode() const = 0;
    virtual void SetParent(uint64_t childId, uint64_t parentId) = 0;

    virtual bool GetCamera(EngineCameraInfo& info) const = 0;
    virtual void SetOverlayLines(const std::vector<EngineOverlayLine>& lines, float thickness = 1.0f) = 0;
    virtual void SetOverlayIcon(uint32_t iconId, uint32_t width, uint32_t height, const uint8_t* rgbaPixels) = 0;
    virtual void SetOverlayBillboards(const std::vector<EngineOverlayBillboard>& billboards) = 0;

    virtual void RunOnUi(std::function<void()> action) = 0;
    virtual void SetUiListener(IEditorUiListener* listener) = 0;
};

