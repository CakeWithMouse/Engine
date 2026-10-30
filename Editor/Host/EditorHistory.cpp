#include "EditorHistory.h"
#include <QTemporaryDir>
#include <QFile>
#include <stdexcept>

EditorHistory::EditorHistory() = default;

EditorHistory::~EditorHistory()
{
    ClearHistory();
}

bool EditorHistory::Initialize()
{
    historyDirectory_ = std::make_unique<QTemporaryDir>();
    return historyDirectory_ && historyDirectory_->isValid();
}

void EditorHistory::Reset()
{
    ClearHistory();
    lastTransformId_ = 0;
    lastTransformTime_ = {};
    gizmoDragActive_ = false;
    gizmoDragCheckpointed_ = false;
    gizmoDragObject_ = 0;
}

void EditorHistory::ClearHistory()
{
    for (const auto& file : undo_) QFile::remove(file);
    for (const auto& file : redo_) QFile::remove(file);
    undo_.clear();
    redo_.clear();
}

QString EditorHistory::Checkpoint(const EngineEditSessionApi& scene)
{
    if (!historyDirectory_ || !historyDirectory_->isValid())
        throw std::runtime_error("Undo history directory is unavailable");

    auto path = historyDirectory_->filePath(QString::number(++historySerial_) + QStringLiteral(".scene.json"));
    char error[1024]{};
    if (scene.saveScene(scene.context, path.toUtf8().constData(), error, sizeof(error)) != EngineSceneOk)
        throw std::runtime_error(error);

    return path;
}

void EditorHistory::RecordChange(const QString& checkpoint)
{
    for (const auto& file : redo_) QFile::remove(file);
    redo_.clear();
    if (checkpoint.isEmpty()) return;

    undo_.push_back(checkpoint);
    if (undo_.size() > 64)
    {
        QFile::remove(undo_.front());
        undo_.erase(undo_.begin());
    }
}

bool EditorHistory::ShouldCoalesce(const EditorCommand& cmd, std::chrono::steady_clock::time_point now) const
{
    if (cmd.type != EditorCommand::UpdateObjectTransform) return false;

    const bool isNudge = (cmd.path == QStringLiteral("nudge"));
    const bool isGizmo = (cmd.path == QStringLiteral("gizmo"));
    const bool gizmoSession = isGizmo && gizmoDragActive_ && (cmd.objectId == gizmoDragObject_);

    return (isNudge && lastTransformId_ == cmd.objectId && now - lastTransformTime_ < std::chrono::milliseconds(350))
        || (gizmoSession && gizmoDragCheckpointed_)
        || (isGizmo && !gizmoDragActive_ && lastTransformId_ == cmd.objectId && now - lastTransformTime_ < std::chrono::milliseconds(350));
}

void EditorHistory::OnTransformApplied(const EditorCommand& cmd, std::chrono::steady_clock::time_point now)
{
    const bool isNudge = (cmd.path == QStringLiteral("nudge"));
    const bool isGizmo = (cmd.path == QStringLiteral("gizmo"));
    if (isNudge || isGizmo)
    {
        lastTransformId_ = cmd.objectId;
        lastTransformTime_ = now;
        if (isGizmo && gizmoDragActive_ && cmd.objectId == gizmoDragObject_)
        {
            gizmoDragCheckpointed_ = true;
        }
    }
}

void EditorHistory::BeginGizmoDrag(uint64_t objectId)
{
    gizmoDragActive_ = true;
    gizmoDragCheckpointed_ = false;
    gizmoDragObject_ = objectId;
    lastTransformId_ = 0;
}

void EditorHistory::EndGizmoDrag()
{
    gizmoDragActive_ = false;
    gizmoDragCheckpointed_ = false;
    gizmoDragObject_ = 0;
    lastTransformId_ = 0;
}

bool EditorHistory::Undo(const EngineEditSessionApi& scene, uint32_t& result, char* error, size_t errorSize)
{
    lastTransformId_ = 0;
    if (undo_.empty()) return false;

    const auto current = Checkpoint(scene);
    uint64_t id = 0;
    result = scene.loadScene(scene.context, undo_.back().toUtf8().constData(), &id, error, static_cast<uint32_t>(errorSize));
    if (result == EngineSceneOk)
    {
        redo_.push_back(current);
        QFile::remove(undo_.back());
        undo_.pop_back();
        return true;
    }
    QFile::remove(current);
    return false;
}

bool EditorHistory::Redo(const EngineEditSessionApi& scene, uint32_t& result, char* error, size_t errorSize)
{
    lastTransformId_ = 0;
    if (redo_.empty()) return false;

    const auto current = Checkpoint(scene);
    uint64_t id = 0;
    result = scene.loadScene(scene.context, redo_.back().toUtf8().constData(), &id, error, static_cast<uint32_t>(errorSize));
    if (result == EngineSceneOk)
    {
        undo_.push_back(current);
        QFile::remove(redo_.back());
        redo_.pop_back();
        return true;
    }
    QFile::remove(current);
    return false;
}

QString EditorHistory::CreateEmptySceneFile()
{
    if (!historyDirectory_ || !historyDirectory_->isValid())
        throw std::runtime_error("Undo history directory is unavailable");

    QString path = historyDirectory_->filePath(QStringLiteral("empty.scene.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write("{\"version\":1,\"name\":\"Scene\",\"objects\":[]}") < 0)
        throw std::runtime_error("Cannot create empty scene");
    file.close();
    return path;
}

QString EditorHistory::GetTemporaryFilePath(const QString& templateName)
{
    if (!historyDirectory_ || !historyDirectory_->isValid())
        throw std::runtime_error("Undo history directory is unavailable");

    return historyDirectory_->filePath(templateName);
}

