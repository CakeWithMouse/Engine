// Blocks of .fsm machines: the registry, the built-in "Ai.*" blocks and the catalog for the editor.
#include <Engine/AI/Controller.h>
#include <Engine/AI/MachineFile.h>
#include <Engine/AI/Tasks.h>
#include <Engine/AI/World.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Script/ScriptRuntime.h>
#include <Engine/Components/GameComponents.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <stdexcept>

namespace Ai
{
namespace
{
using Json = nlohmann::json;

bool IsKey(ParamType type) { return type >= ParamType::Key && type <= ParamType::KeyPlace; }

void SetMemory(Blackboard& memory, const std::string& key, const Blackboard::Value& value)
{
    std::visit([&](const auto& typed) {
        using T = std::decay_t<decltype(typed)>;
        memory.Set(Key<T>{key.c_str()}, typed);
    }, value);
}

bool HasOfType(const Blackboard& memory, const std::string& key, VariableType type)
{
    const auto found = memory.Values().find(key);
    return found != memory.Values().end() && found->second.index() == static_cast<size_t>(type);
}

// Clears a key of any type.
class ClearAnyKey final : public Task<>
{
public:
    explicit ClearAnyKey(std::string key) : key_(std::move(key)) {}
    Status Enter(Context& ctx, NoData&) const override { ctx.Memory().Clear(Key<bool>{key_.c_str()}); return Status::Succeeded; }
    std::string Name() const override { return "ClearKey " + key_; }
private:
    std::string key_;
};

class SetValue final : public Task<>
{
public:
    SetValue(std::string key, Blackboard::Value value) : key_(std::move(key)), value_(std::move(value)) {}
    Status Enter(Context& ctx, NoData&) const override { SetMemory(ctx.Memory(), key_, value_); return Status::Succeeded; }
    std::string Name() const override { return "SetValue " + key_; }
private:
    std::string key_;
    Blackboard::Value value_;
};

// Writes the scene object with this name to memory; fails when there is none.
class FindObject final : public Task<>
{
public:
    FindObject(std::string object, std::string key) : object_(std::move(object)), key_(std::move(key)) {}
    Status Enter(Context& ctx, NoData&) const override
    {
        Scene* scene = ctx.GetWorld().GetScene();
        GameComponent* found = scene ? scene->FindComponentByName(object_) : nullptr;
        if (!found)
        {
            ctx.Memory().Clear(Key<ObjectRef>{key_.c_str()});
            return Status::Failed;
        }
        ctx.Memory().Set(Key<ObjectRef>{key_.c_str()}, ObjectRef{found->GetComponentId()});
        return Status::Succeeded;
    }
    std::string Name() const override { return "FindObject " + object_; }
private:
    std::string object_, key_;
};

// The object a script block works on: the key's object, or the character itself when the key is not set.
GameComponent* ScriptObject(Context& ctx, const std::string& key)
{
    if (key.empty()) return ctx.OwnerComponent();
    return ctx.ResolveComponent(ctx.Memory().Get(Key<ObjectRef>{key.c_str()}));
}

// Runs a Custom event of the object's visual script; fails when the object, its script or the event is missing.
class CallScriptEvent final : public Task<>
{
public:
    CallScriptEvent(std::string event, std::string target) : event_(std::move(event)), target_(std::move(target)) {}
    Status Enter(Context& ctx, NoData&) const override
    {
        GameComponent* object = ScriptObject(ctx, target_);
        Script::Instance* script = object ? object->GetScriptInstance() : nullptr;
        return script && !script->Failed() && script->Trigger(event_) ? Status::Succeeded : Status::Failed;
    }
    std::string Name() const override { return "CallScriptEvent " + event_; }
private:
    std::string event_, target_;
};

using Registry = std::map<std::string, BlockSpec, std::less<>>;
void AddBuiltIns(Registry& blocks);

// A function-local static: registrations from other files' static objects may run before main.
Registry& Blocks_()
{
    static Registry blocks = [] { Registry built; AddBuiltIns(built); return built; }();
    return blocks;
}

void Check(const BlockSpec& block, bool builtIn)
{
    const std::string where = "Ai::RegisterBlock(\"" + block.type + "\")";
    if (block.type.empty()) throw std::invalid_argument("Ai::RegisterBlock: empty block name");
    if (!builtIn && (block.type.rfind("Ai.", 0) == 0 || block.type.rfind("Fsm.", 0) == 0))
        throw std::invalid_argument(where + ": names starting with \"Ai.\" or \"Fsm.\" belong to the engine");
    const bool factory = block.kind == BlockKind::Task ? bool(block.task) : block.kind == BlockKind::Condition ? bool(block.condition) : bool(block.service);
    if (!factory) throw std::invalid_argument(where + ": no factory for its kind");
    bool hasKey = false, hasValue = false;
    for (size_t i = 0; i < block.params.size(); ++i)
    {
        const ParamSpec& param = block.params[i];
        if (param.name.empty()) throw std::invalid_argument(where + ": a parameter without a name");
        for (const char* reserved : {"type", "not", "items", "interval"})
            if (param.name == reserved) throw std::invalid_argument(where + ": parameter name \"" + param.name + "\" is reserved");
        for (size_t j = 0; j < i; ++j)
            if (block.params[j].name == param.name) throw std::invalid_argument(where + ": two parameters are named \"" + param.name + "\"");
        if (param.name == "key" && IsKey(param.type)) hasKey = true;
        if (param.type == ParamType::Value) hasValue = true;
        if (param.minimum > param.maximum) throw std::invalid_argument(where + ": \"" + param.name + "\" has minimum above maximum");
    }
    if (hasValue && !hasKey) throw std::invalid_argument(where + ": a Value parameter needs a key parameter named \"key\"");
}

ParamSpec Make(std::string name, ParamType type, std::string ru, std::string en, Blackboard::Value fallback, double minimum, double maximum, bool required)
{
    ParamSpec spec;
    spec.name = std::move(name);
    spec.type = type;
    spec.labelRu = std::move(ru);
    spec.labelEn = std::move(en);
    spec.fallback = std::move(fallback);
    spec.minimum = minimum;
    spec.maximum = maximum;
    spec.required = required;
    return spec;
}

void Add(Registry& blocks, BlockSpec block)
{
    Check(block, true);
    std::string type = block.type;
    blocks[std::move(type)] = std::move(block);
}

void AddBuiltIns(Registry& blocks)
{
    using namespace Param;
    const ParamSpec seconds = Float("seconds", "Секунды", "Seconds", 1, 0);

    // Tasks
    Add(blocks, TaskBlock("Ai.Wait", "Ждать", "Wait", {seconds},
        [](const Params& p) { return std::make_shared<Tasks::Wait>(p.Float("seconds")); },
        "Ждёт заданное время и завершается успешно.", "Waits, then succeeds."));
    Add(blocks, TaskBlock("Ai.WaitRandom", "Ждать случайное время", "Wait random",
        {Float("minimum", "От, с", "From, s", 1, 0), Float("maximum", "До, с", "To, s", 2, 0)},
        [](const Params& p) -> std::shared_ptr<const TaskBase> {
            if (p.Float("minimum") > p.Float("maximum")) throw std::invalid_argument("\"minimum\" is greater than \"maximum\"");
            return std::make_shared<Tasks::WaitRandom>(p.Float("minimum"), p.Float("maximum"));
        },
        "Ждёт случайное время между «От» и «До».", "Waits a random time between From and To."));
    Add(blocks, TaskBlock("Ai.MoveTo", "Идти к", "Move to",
        {Param::Key("target", ParamType::KeyPlace, "Куда (объект или точка)", "Where (object or point)"), Float("radius", "Радиус прибытия", "Acceptance radius", 1, 0)},
        [](const Params& p) -> std::shared_ptr<const TaskBase> {
            const std::string& key = p.String("target");
            if (p.KeyType("target") == VariableType::Object) return std::make_shared<Tasks::MoveTo>(Ai::Key<ObjectRef>{key.c_str()}, p.Float("radius"));
            return std::make_shared<Tasks::MoveTo>(Ai::Key<glm::vec3>{key.c_str()}, p.Float("radius"));
        },
        "Идёт к объекту или точке из памяти. Провал — ключ пуст или цель недостижима.",
        "Goes to an object or a point from memory. Fails when the key is empty or the goal is unreachable."));
    Add(blocks, TaskBlock("Ai.FollowRoute", "Следующая точка маршрута", "Next route point", {Float("radius", "Радиус прибытия", "Acceptance radius", 0.5f, 0)},
        [](const Params& p) { return std::make_shared<Tasks::FollowRoute>(p.Float("radius")); },
        "Идёт к следующей точке маршрута (маршрут задаёт C++: Controller::SetRoute).",
        "Goes to the next point of the route set in C++ with Controller::SetRoute."));
    Add(blocks, TaskBlock("Ai.LookAt", "Смотреть на", "Look at", {Param::Key("target", ParamType::KeyObject, "На кого", "Target")},
        [](const Params& p) { return std::make_shared<Tasks::LookAt>(Ai::Key<ObjectRef>{p.String("target").c_str()}); },
        "Поворачивается к объекту, пока состояние активно. Сама не завершается.",
        "Looks at an object while the state is active. Never finishes by itself."));
    Add(blocks, TaskBlock("Ai.SendEvent", "Отправить событие", "Send event", {Param::Event("event", "Событие", "Event")},
        [](const Params& p) { return std::make_shared<Tasks::SendEvent>(Ai::Event{p.String("event").c_str()}); },
        "Отправляет событие своему автомату; переход «по событию» сработает в следующем кадре.",
        "Sends an event to its own machine; an event transition fires at the next think."));
    Add(blocks, TaskBlock("Ai.SetValue", "Записать в память", "Set value",
        {Param::Key("key", ParamType::Key, "Ключ", "Key"), Value("value", "Значение", "Value")},
        [](const Params& p) -> std::shared_ptr<const TaskBase> {
            if (p.KeyType("key") == VariableType::Object) throw std::invalid_argument("an object key cannot be set to a value; use Ai.FindObject");
            return std::make_shared<SetValue>(p.String("key"), p.Get("value"));
        },
        "Записывает значение в память и сразу завершается.", "Writes a value to memory and succeeds."));
    Add(blocks, TaskBlock("Ai.ClearKey", "Очистить ключ", "Clear key", {Param::Key("key", ParamType::Key, "Ключ", "Key")},
        [](const Params& p) { return std::make_shared<ClearAnyKey>(p.String("key")); },
        "Удаляет значение из памяти.", "Removes the value from memory."));
    Add(blocks, TaskBlock("Ai.FindObject", "Найти объект", "Find object",
        {String("object", "Имя объекта в сцене", "Object name in the scene", {}, true), Param::Key("key", ParamType::KeyObject, "Записать в", "Write to")},
        [](const Params& p) { return std::make_shared<FindObject>(p.String("object"), p.String("key")); },
        "Записывает объект сцены с этим именем в память. Провал — такого объекта нет.",
        "Writes the scene object with this name to memory. Fails when there is none."));
    Add(blocks, TaskBlock("Ai.Log", "Записать в историю", "Log", {String("text", "Текст", "Text")},
        [](const Params& p) { return std::make_shared<Tasks::Log>(p.String("text")); },
        "Добавляет строку в историю контроллера (видна в отладчике).", "Adds a line to the controller history (shown by the debugger)."));
    Add(blocks, TaskBlock("Ai.UseSmartObject", "Использовать умный объект", "Use smart object",
        {String("tag", "Метка", "Tag", {}, true), Float("seconds", "Сколько секунд", "Seconds", 2, 0), Float("radius", "Радиус поиска", "Search radius", 50, 0)},
        [](const Params& p) { return std::make_shared<Tasks::UseSmartObject>(p.String("tag"), p.Float("seconds"), p.Float("radius")); },
        "Находит ближайший свободный умный объект с меткой, идёт к нему и использует.",
        "Finds the nearest free smart object with the tag, walks there and uses it."));

    Add(blocks, TaskBlock("Ai.CallScriptEvent", "Выполнить событие скрипта", "Call script event",
        {Param::String("event", "Событие скрипта («Своё событие»)", "Script event (Custom event)", {}, true),
         Param::Key("target", ParamType::KeyObject, "Чей скрипт (пусто — свой)", "Whose script (empty — own)", false)},
        [](const Params& p) { return std::make_shared<CallScriptEvent>(p.String("event"), p.String("target")); },
        "Запускает «Своё событие» с этим именем в визуальном скрипте персонажа или объекта из памяти и сразу завершается. "
        "Провал — у объекта нет скрипта или такого события.",
        "Runs the Custom event of this name in the visual script of the character or of an object from memory and finishes at once. "
        "Fails when the object has no script or no such event."));

    // Conditions
    Add(blocks, ConditionBlock("Ai.ScriptVariable", "Переменная скрипта", "Script variable",
        {Param::String("variable", "Переменная скрипта", "Script variable", {}, true),
         Param::String("op", "Сравнение (<, <=, ==, !=, >=, >)", "Comparison (<, <=, ==, !=, >=, >)", "=="),
         Param::Float("value", "С чем (логическое: 1 — да)", "Value (bool: 1 — true)", 1),
         Param::Key("target", ParamType::KeyObject, "Чей скрипт (пусто — свой)", "Whose script (empty — own)", false)},
        [](const Params& p) {
            const std::string op = p.String("op");
            int code = -1;
            const char* ops[] = {"<", "<=", "==", "!=", ">=", ">"};
            for (int i = 0; i < 6; ++i) if (op == ops[i]) code = i;
            if (code < 0) throw std::invalid_argument("\"op\" must be one of <, <=, ==, !=, >=, >");
            return ConditionFn([variable = p.String("variable"), target = p.String("target"), code, value = p.Float("value")](Context& ctx) {
                GameComponent* object = ScriptObject(ctx, target);
                const Script::Instance* script = object ? object->GetScriptInstance() : nullptr;
                if (!script) return false;
                const Script::Value current = script->GetVariable(variable);
                float x = 0;
                if (const float* f = std::get_if<float>(&current)) x = *f;
                else if (const bool* b = std::get_if<bool>(&current)) x = *b ? 1.0f : 0.0f;
                else return false;
                switch (code)
                {
                case 0: return x < value;
                case 1: return x <= value;
                case 2: return x == value;
                case 3: return x != value;
                case 4: return x >= value;
                default: return x > value;
                }
            });
        },
        "Число или логическое значение переменной визуального скрипта персонажа (или объекта из памяти) сравнивается со значением. "
        "Нет скрипта или переменной — ложь.",
        "Compares a number or bool variable of the visual script of the character (or of an object from memory) with a value. "
        "No script or no variable — false."));
    Add(blocks, ConditionBlock("Ai.HasKey", "Ключ задан", "Has key", {Param::Key("key", ParamType::Key, "Ключ", "Key")},
        [](const Params& p) {
            return ConditionFn([key = p.String("key"), type = p.KeyType("key")](Context& ctx) { return HasOfType(ctx.Memory(), key, type); });
        },
        "В памяти есть значение этого ключа.", "Memory holds a value for the key."));
    Add(blocks, ConditionBlock("Ai.IsTrue", "Истина", "Is true", {Param::Key("key", ParamType::KeyBool, "Ключ", "Key")},
        [](const Params& p) { return Conditions::IsTrue(Ai::Key<bool>{p.String("key").c_str()}); },
        "Логический ключ равен «да».", "The bool key is true."));
    Add(blocks, ConditionBlock("Ai.Compare", "Сравнить число", "Compare number",
        {Param::Key("key", ParamType::KeyNumber, "Ключ", "Key"), String("op", "Сравнение (<, <=, ==, !=, >=, >)", "Comparison (<, <=, ==, !=, >=, >)", ">"),
         Float("value", "С чем", "Value", 0)},
        [](const Params& p) {
            const std::string op = p.String("op");
            int code = -1;
            const char* ops[] = {"<", "<=", "==", "!=", ">=", ">"};
            for (int i = 0; i < 6; ++i) if (op == ops[i]) code = i;
            if (code < 0) throw std::invalid_argument("\"op\" must be one of <, <=, ==, !=, >=, >");
            return ConditionFn([key = p.String("key"), type = p.KeyType("key"), code, value = p.Float("value")](Context& ctx) {
                if (!HasOfType(ctx.Memory(), key, type)) return false;
                const float x = type == VariableType::Int ? static_cast<float>(ctx.Memory().Get(Ai::Key<int>{key.c_str()}))
                                                          : ctx.Memory().Get(Ai::Key<float>{key.c_str()});
                switch (code)
                {
                case 0: return x < value;
                case 1: return x <= value;
                case 2: return x == value;
                case 3: return x != value;
                case 4: return x >= value;
                default: return x > value;
                }
            });
        },
        "Числовой ключ сравнивается со значением. Ключ не задан — ложь.", "Compares a number key with a value. False when the key is empty."));
    Add(blocks, ConditionBlock("Ai.Near", "Рядом", "Near",
        {Param::Key("target", ParamType::KeyPlace, "С кем (объект или точка)", "Target (object or point)"), Float("distance", "Расстояние", "Distance", 2, 0)},
        [](const Params& p) {
            const std::string& key = p.String("target");
            if (p.KeyType("target") == VariableType::Object) return Conditions::Near(Ai::Key<ObjectRef>{key.c_str()}, p.Float("distance"));
            return Conditions::Near(Ai::Key<glm::vec3>{key.c_str()}, p.Float("distance"));
        },
        "Расстояние по горизонтали до объекта или точки не больше заданного.", "The horizontal distance to the object or point is at most this."));
    Add(blocks, ConditionBlock("Ai.CanSee", "Видит", "Can see", {Param::Key("target", ParamType::KeyObject, "Кого", "Whom")},
        [](const Params& p) { return Conditions::CanSee(Ai::Key<ObjectRef>{p.String("target").c_str()}); },
        "Объект из памяти виден прямо сейчас (нужно зрение у персонажа).", "The object from memory is visible right now (the character needs sight)."));
    Add(blocks, ConditionBlock("Ai.TimeInState", "Время в состоянии", "Time in state", {seconds},
        [](const Params& p) { return Conditions::TimeInState(p.Float("seconds")); },
        "Состояние активно не меньше заданного времени.", "The state has been active at least this long."));
    Add(blocks, ConditionBlock("Ai.Chance", "Случайно", "Chance", {Float("probability", "Вероятность (0–1)", "Probability (0–1)", 0.5f, 0, 1)},
        [](const Params& p) { return Conditions::Chance(p.Float("probability")); },
        "Истина с заданной вероятностью при каждой проверке.", "True with this probability on each check."));
}

const char* KindName(BlockKind kind) { return kind == BlockKind::Task ? "task" : kind == BlockKind::Condition ? "condition" : "service"; }

Json ValueJson(const Blackboard::Value& value)
{
    return std::visit([](const auto& typed) -> Json {
        using T = std::decay_t<decltype(typed)>;
        if constexpr (std::is_same_v<T, glm::vec3>) return Json::array({typed.x, typed.y, typed.z});
        else if constexpr (std::is_same_v<T, ObjectRef>) return nullptr;
        else return typed;
    }, value);
}
} // namespace

const char* ToString(VariableType type)
{
    switch (type)
    {
    case VariableType::Bool: return "bool";
    case VariableType::Int: return "int";
    case VariableType::Float: return "float";
    case VariableType::Vec3: return "vec3";
    case VariableType::String: return "string";
    default: return "object";
    }
}

const char* ToString(ParamType type)
{
    switch (type)
    {
    case ParamType::Bool: return "bool";
    case ParamType::Int: return "int";
    case ParamType::Float: return "float";
    case ParamType::Vec3: return "vec3";
    case ParamType::String: return "string";
    case ParamType::Event: return "event";
    case ParamType::Key: return "key";
    case ParamType::KeyBool: return "key:bool";
    case ParamType::KeyInt: return "key:int";
    case ParamType::KeyFloat: return "key:float";
    case ParamType::KeyVec3: return "key:vec3";
    case ParamType::KeyString: return "key:string";
    case ParamType::KeyObject: return "key:object";
    case ParamType::KeyNumber: return "key:number";
    case ParamType::KeyPlace: return "key:place";
    default: return "value";
    }
}

namespace Param
{
ParamSpec Bool(std::string name, std::string ru, std::string en, bool fallback)
{ return Make(std::move(name), ParamType::Bool, std::move(ru), std::move(en), fallback, -INFINITY, INFINITY, false); }
ParamSpec Int(std::string name, std::string ru, std::string en, int fallback, double minimum, double maximum)
{ return Make(std::move(name), ParamType::Int, std::move(ru), std::move(en), fallback, minimum, maximum, false); }
ParamSpec Float(std::string name, std::string ru, std::string en, float fallback, double minimum, double maximum)
{ return Make(std::move(name), ParamType::Float, std::move(ru), std::move(en), fallback, minimum, maximum, false); }
ParamSpec Vec3(std::string name, std::string ru, std::string en, glm::vec3 fallback)
{ return Make(std::move(name), ParamType::Vec3, std::move(ru), std::move(en), fallback, -INFINITY, INFINITY, false); }
ParamSpec String(std::string name, std::string ru, std::string en, std::string fallback, bool required)
{ return Make(std::move(name), ParamType::String, std::move(ru), std::move(en), std::move(fallback), -INFINITY, INFINITY, required); }
ParamSpec Event(std::string name, std::string ru, std::string en)
{ return Make(std::move(name), ParamType::Event, std::move(ru), std::move(en), std::string(), -INFINITY, INFINITY, true); }
ParamSpec Key(std::string name, ParamType type, std::string ru, std::string en, bool required)
{
    if (!IsKey(type)) throw std::invalid_argument("Ai::Param::Key(\"" + name + "\"): the type must be one of the Key types");
    return Make(std::move(name), type, std::move(ru), std::move(en), std::string(), -INFINITY, INFINITY, required);
}
ParamSpec Value(std::string name, std::string ru, std::string en)
{ return Make(std::move(name), ParamType::Value, std::move(ru), std::move(en), false, -INFINITY, INFINITY, false); }
} // namespace Param

const Blackboard::Value& Params::Get(std::string_view name) const
{
    const auto found = values_.find(name);
    if (found == values_.end()) throw std::invalid_argument("the block has no parameter \"" + std::string(name) + "\"");
    return found->second;
}

template<class T> static const T& Typed(const Params& params, std::string_view name, const char* type)
{
    const T* value = std::get_if<T>(&params.Get(name));
    if (!value) throw std::invalid_argument("parameter \"" + std::string(name) + "\" is not a " + type);
    return *value;
}

bool Params::Bool(std::string_view name) const { return Typed<bool>(*this, name, "bool"); }
int Params::Int(std::string_view name) const { return Typed<int>(*this, name, "int"); }
float Params::Float(std::string_view name) const
{
    const Blackboard::Value& value = Get(name);
    if (const int* whole = std::get_if<int>(&value)) return static_cast<float>(*whole);
    return Typed<float>(*this, name, "float");
}
glm::vec3 Params::Vec3(std::string_view name) const { return Typed<glm::vec3>(*this, name, "vec3"); }
const std::string& Params::String(std::string_view name) const { return Typed<std::string>(*this, name, "string"); }
VariableType Params::KeyType(std::string_view name) const
{
    const auto found = keyTypes_.find(name);
    if (found == keyTypes_.end()) throw std::invalid_argument("parameter \"" + std::string(name) + "\" is not a key");
    return found->second;
}

static BlockSpec Block(std::string type, BlockKind kind, std::string ru, std::string en, std::vector<ParamSpec> params, std::string hintRu, std::string hintEn)
{
    BlockSpec block;
    block.type = std::move(type);
    block.kind = kind;
    block.nameRu = std::move(ru);
    block.nameEn = std::move(en);
    block.hintRu = std::move(hintRu);
    block.hintEn = std::move(hintEn);
    block.params = std::move(params);
    return block;
}

BlockSpec TaskBlock(std::string type, std::string ru, std::string en, std::vector<ParamSpec> params, TaskFactory factory, std::string hintRu, std::string hintEn)
{
    BlockSpec block = Block(std::move(type), BlockKind::Task, std::move(ru), std::move(en), std::move(params), std::move(hintRu), std::move(hintEn));
    block.task = std::move(factory);
    return block;
}

BlockSpec ConditionBlock(std::string type, std::string ru, std::string en, std::vector<ParamSpec> params, ConditionFactory factory, std::string hintRu, std::string hintEn)
{
    BlockSpec block = Block(std::move(type), BlockKind::Condition, std::move(ru), std::move(en), std::move(params), std::move(hintRu), std::move(hintEn));
    block.condition = std::move(factory);
    return block;
}

BlockSpec ServiceBlock(std::string type, std::string ru, std::string en, std::vector<ParamSpec> params, ServiceFactory factory, std::string hintRu, std::string hintEn)
{
    BlockSpec block = Block(std::move(type), BlockKind::Service, std::move(ru), std::move(en), std::move(params), std::move(hintRu), std::move(hintEn));
    block.service = std::move(factory);
    return block;
}

void RegisterBlock(BlockSpec block)
{
    Check(block, false);
    Registry& blocks = Blocks_();
    std::string type = block.type;
    blocks[std::move(type)] = std::move(block);
}

const BlockSpec* FindBlock(std::string_view type)
{
    const Registry& blocks = Blocks_();
    const auto found = blocks.find(type);
    return found == blocks.end() ? nullptr : &found->second;
}

std::vector<const BlockSpec*> Blocks()
{
    std::vector<const BlockSpec*> list;
    for (const auto& entry : Blocks_()) list.push_back(&entry.second);
    return list;
}

std::string MachineCatalogJson()
{
    Json root;
    root["format"] = File::FormatName;
    root["schema"] = File::SchemaId;
    root["schemaVersion"] = File::SchemaVersion;
    root["nodes"] = Json::array({
        {{"type", File::StateNode}, {"nameRu", "Состояние"}, {"nameEn", "State"}, {"inputs", {"in"}}, {"outputs", {"out"}},
         {"properties", {"name", "parent", "tasks", "enterIf", "services", "linked"}}},
        {{"type", File::FinishNode}, {"nameRu", "Закончить"}, {"nameEn", "Finish"}, {"inputs", {"in"}}, {"outputs", Json::array()}},
        {{"type", File::NextNode}, {"nameRu", "Следующее"}, {"nameEn", "Next"}, {"inputs", {"in"}}, {"outputs", Json::array()}}});
    root["triggers"] = {"completed", "succeeded", "failed", "event", "tick"};
    root["variableTypes"] = {"bool", "int", "float", "vec3", "string", "object"};
    root["combinators"] = {"Ai.All", "Ai.Any"};
    Json blocks = Json::array();
    for (const BlockSpec* block : Blocks())
    {
        Json params = Json::array();
        for (const ParamSpec& param : block->params)
        {
            Json item{{"name", param.name}, {"type", ToString(param.type)}, {"labelRu", param.labelRu}, {"labelEn", param.labelEn},
                      {"required", param.required || param.type == ParamType::Event}};
            if (param.type != ParamType::Value && !IsKey(param.type) && param.type != ParamType::Event) item["default"] = ValueJson(param.fallback);
            if (std::isfinite(param.minimum)) item["min"] = param.minimum;
            if (std::isfinite(param.maximum)) item["max"] = param.maximum;
            params.push_back(std::move(item));
        }
        blocks.push_back({{"type", block->type}, {"kind", KindName(block->kind)}, {"nameRu", block->nameRu}, {"nameEn", block->nameEn},
                          {"hintRu", block->hintRu}, {"hintEn", block->hintEn}, {"params", std::move(params)},
                          {"engine", block->type.rfind("Ai.", 0) == 0}});
    }
    root["blocks"] = std::move(blocks);
    return root.dump(2);
}
} // namespace Ai
