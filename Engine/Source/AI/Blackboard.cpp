#include <Engine/AI/AiTypes.h>
#include <Engine/SaveGame/SaveArchive.h>
#include <sstream>
#include <stdexcept>

namespace Ai
{
const char* ToString(Status status)
{
    switch (status)
    {
    case Status::Running: return "Running";
    case Status::Succeeded: return "Succeeded";
    case Status::Failed: return "Failed";
    }
    return "?";
}

const char* ToString(MoveStatus status)
{
    switch (status)
    {
    case MoveStatus::Idle: return "Idle";
    case MoveStatus::Waiting: return "Waiting";
    case MoveStatus::Moving: return "Moving";
    case MoveStatus::Arrived: return "Arrived";
    case MoveStatus::Unreachable: return "Unreachable";
    case MoveStatus::Stuck: return "Stuck";
    case MoveStatus::Unavailable: return "Unavailable";
    }
    return "?";
}

std::string Blackboard::Describe() const
{
    std::ostringstream out;
    for (const auto& [name, value] : values_)
    {
        out << name << " = ";
        std::visit([&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) out << (v ? "true" : "false");
            else if constexpr (std::is_same_v<T, glm::vec3>) out << '(' << v.x << ", " << v.y << ", " << v.z << ')';
            else if constexpr (std::is_same_v<T, ObjectRef>) out << "object " << v.id;
            else if constexpr (std::is_same_v<T, std::string>) out << '"' << v << '"';
            else out << v;
        }, value);
        out << '\n';
    }
    return out.str();
}

// Names and types first, then one section per value: SaveArchive reads only known fields.
void Blackboard::SerializeSaveGame(SaveArchive& archive)
{
    archive.Version(1);
    std::vector<std::string> names;
    std::vector<int> types;
    if (archive.IsWriting())
        for (const auto& [name, value] : values_) { names.push_back(name); types.push_back(static_cast<int>(value.index())); }
    archive.Field("names", names);
    archive.Field("types", types);
    if (names.size() != types.size()) throw std::runtime_error("Blackboard: names and types differ");
    if (archive.IsReading()) values_.clear();
    for (size_t i = 0; i < names.size(); ++i)
        archive.Section(names[i].c_str(), [&](SaveArchive& item) {
            Value value = archive.IsWriting() ? values_.at(names[i]) : Value{};
            switch (types[i])
            {
            case 0: { bool v = archive.IsWriting() && std::get<bool>(value); item.Field("value", v); value = v; break; }
            case 1: { int v = archive.IsWriting() ? std::get<int>(value) : 0; item.Field("value", v); value = v; break; }
            case 2: { float v = archive.IsWriting() ? std::get<float>(value) : 0.f; item.Field("value", v); value = v; break; }
            case 3: { glm::vec3 v = archive.IsWriting() ? std::get<glm::vec3>(value) : glm::vec3(0); item.Field("value", v); value = v; break; }
            case 4: { std::string v = archive.IsWriting() ? std::get<std::string>(value) : std::string(); item.Field("value", v); value = v; break; }
            case 5: { uint64_t v = archive.IsWriting() ? std::get<ObjectRef>(value).id : 0; item.Reference("value", v); value = ObjectRef{v}; break; }
            default: throw std::runtime_error("Blackboard: unknown value type");
            }
            if (archive.IsReading()) values_[names[i]] = value;
        });
}
} // namespace Ai
