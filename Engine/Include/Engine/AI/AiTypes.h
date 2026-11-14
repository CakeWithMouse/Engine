#pragma once
#include <Engine/Scene/Scene.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <type_traits>
#include <variant>

class SaveArchive;

// Common AI types. Everything lives in namespace Ai, like Input and Trace.
// Guide: docs/AI-CPP-GUIDE.md; architecture: docs/AI-DESIGN.md.
namespace Ai
{
enum class Status { Running, Succeeded, Failed };
enum class MoveStatus { Idle, Waiting, Moving, Arrived, Unreachable, Stuck, Unavailable };
enum class Attitude { Friendly, Neutral, Hostile };

const char* ToString(Status status);
const char* ToString(MoveStatus status);

/** Reference to a scene object. Stored as an ID and resolved on use; saves remap it. */
struct ObjectRef
{
    ComponentId id = 0;
    explicit operator bool() const { return id != 0; }
    friend bool operator==(ObjectRef a, ObjectRef b) { return a.id == b.id; }
    friend bool operator!=(ObjectRef a, ObjectRef b) { return a.id != b.id; }
};

/** Name and type of a memory value. A value of another type does not compile. */
template<class T> struct Key { const char* name; };

struct Event { const char* name; };
inline bool operator==(Event a, Event b) { return a.name && b.name && std::strcmp(a.name, b.name) == 0; }

/** Team 0 is neutral to everyone; different teams are hostile unless set otherwise. */
struct Team
{
    uint8_t id = 0;
    friend bool operator==(Team a, Team b) { return a.id == b.id; }
    friend bool operator!=(Team a, Team b) { return a.id != b.id; }
};

namespace Events
{
inline constexpr Event Spotted{"Ai.Spotted"};   // a hostile became visible
inline constexpr Event Lost{"Ai.Lost"};         // the last visible hostile is no longer visible
inline constexpr Event Heard{"Ai.Heard"};
inline constexpr Event Damaged{"Ai.Damaged"};
}

/** Named values of one controller. Supported: bool, int, float, glm::vec3, std::string, ObjectRef. */
class Blackboard
{
public:
    using Value = std::variant<bool, int, float, glm::vec3, std::string, ObjectRef>;
    template<class T> static constexpr bool Supported =
        std::is_same_v<T, bool> || std::is_same_v<T, int> || std::is_same_v<T, float> ||
        std::is_same_v<T, glm::vec3> || std::is_same_v<T, std::string> || std::is_same_v<T, ObjectRef>;

    template<class T> void Set(Key<T> key, const T& value)
    {
        static_assert(Supported<T>, "Blackboard supports bool, int, float, glm::vec3, std::string and Ai::ObjectRef");
        values_[key.name] = value;
    }
    /** The stored value, or fallback when the key is missing or holds another type. */
    template<class T> T Get(Key<T> key, const T& fallback = {}) const
    {
        static_assert(Supported<T>, "Blackboard supports bool, int, float, glm::vec3, std::string and Ai::ObjectRef");
        const auto found = values_.find(key.name);
        if (found == values_.end()) return fallback;
        if (const T* value = std::get_if<T>(&found->second)) return *value;
        return fallback;
    }
    template<class T> bool Has(Key<T> key) const
    {
        const auto found = values_.find(key.name);
        return found != values_.end() && std::holds_alternative<T>(found->second);
    }
    template<class T> void Clear(Key<T> key) { values_.erase(key.name); }
    void ClearAll() { values_.clear(); }
    size_t Size() const { return values_.size(); }
    const std::map<std::string, Value, std::less<>>& Values() const { return values_; }
    /** One line per value, for the debugger and logs. */
    std::string Describe() const;
    void SerializeSaveGame(SaveArchive& archive);

private:
    std::map<std::string, Value, std::less<>> values_;
};
} // namespace Ai
