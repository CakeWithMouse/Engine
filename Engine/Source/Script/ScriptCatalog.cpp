#include "ScriptProgram.h"

#include <Engine/InputDevice/Keys.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <unordered_map>

namespace Script
{
namespace
{
ValueType ParseType(const std::string& text)
{
    static const std::unordered_map<std::string, ValueType> types{
        {"exec", ValueType::Exec}, {"bool", ValueType::Bool}, {"float", ValueType::Float}, {"vec3", ValueType::Vec3},
        {"string", ValueType::String}, {"object", ValueType::Object}, {"any", ValueType::Any}, {"variable", ValueType::Variable}};
    return types.at(text);
}

// "name:type" pins; inputs before the bar, outputs after it.
std::vector<PinInfo> Pins(std::initializer_list<const char*> inputs, std::initializer_list<const char*> outputs)
{
    std::vector<PinInfo> pins;
    auto add = [&](const char* spec, bool input) {
        const std::string text = spec;
        const size_t colon = text.find(':');
        pins.push_back({text.substr(0, colon), input, ParseType(text.substr(colon + 1))});
    };
    for (const char* pin : inputs) add(pin, true);
    for (const char* pin : outputs) add(pin, false);
    return pins;
}

NodeSpec Spec(Op op, const char* type, const char* group, const char* nameRu, const char* nameEn, const char* hintRu, const char* hintEn,
              std::vector<PinInfo> pins, std::vector<PropertyInfo> properties = {}, bool event = false)
{
    return {{type, group, nameRu, nameEn, hintRu, hintEn, std::move(pins), std::move(properties), event}, op};
}

std::vector<NodeSpec> BuildSpecs()
{
    const PropertyInfo key{"key", "key"}, variable{"variable", "variable"}, eventName{"event", "string"}, memory{"memory", "string"};
    return {
        Spec(Op::BeginPlay, "Event.BeginPlay", "Events", "Начало игры", "Begin play", "Один раз при запуске игры или Play.",
             "Once when the game or Play starts.", Pins({}, {"then:exec"}), {}, true),
        Spec(Op::Tick, "Event.Tick", "Events", "Каждый кадр", "Every frame", "Каждый кадр после логики объекта; DeltaTime — секунды кадра.",
             "Every frame after the object's own logic; DeltaTime is the frame time in seconds.", Pins({}, {"then:exec", "DeltaTime:float"}), {}, true),
        Spec(Op::KeyPressed, "Event.KeyPressed", "Events", "Клавиша нажата", "Key pressed", "Когда клавиша нажата.", "When the key goes down.",
             Pins({}, {"then:exec"}), {key}, true),
        Spec(Op::KeyReleased, "Event.KeyReleased", "Events", "Клавиша отпущена", "Key released", "Когда клавиша отпущена.", "When the key goes up.",
             Pins({}, {"then:exec"}), {key}, true),

        Spec(Op::Every, "Event.Every", "Events", "Каждые N секунд", "Every N seconds", "Срабатывает раз в интервал (секунды > 0).",
             "Fires once per interval (seconds > 0).", Pins({}, {"then:exec"}), {{"seconds", "float"}}, true),
        Spec(Op::CustomEvent, "Event.Custom", "Events", "Своё событие", "Custom event",
             "Срабатывает, когда этот или другой объект вызывает событие с таким именем («Вызвать событие»).",
             "Fires when this or another object calls the event by this name (Call event).", Pins({}, {"then:exec"}), {eventName}, true),

        Spec(Op::Branch, "Flow.Branch", "Flow", "Ветвление", "Branch", "True, если условие выполнено, иначе False.",
             "True when the condition holds, False otherwise.", Pins({"exec:exec", "Condition:bool"}, {"True:exec", "False:exec"})),
        Spec(Op::Sequence, "Flow.Sequence", "Flow", "Последовательность", "Sequence", "Выходы по очереди: then0, then1…",
             "Runs the outputs in order: then0, then1…", Pins({"exec:exec"}, {}), {{"outputs", "outputs"}}),
        Spec(Op::Delay, "Flow.Delay", "Flow", "Задержка", "Delay",
             "Продолжает через Seconds секунд; пока ждёт, повторный вход не учитывается.",
             "Continues after Seconds; while waiting, entering again is ignored.", Pins({"exec:exec", "Seconds:float"}, {"then:exec"})),
        Spec(Op::DoOnce, "Flow.DoOnce", "Flow", "Один раз", "Do once", "Пропускает поток только в первый раз за Play.",
             "Lets the flow through only the first time in Play.", Pins({"exec:exec"}, {"then:exec"})),
        Spec(Op::FlipFlop, "Flow.FlipFlop", "Flow", "Попеременно", "Flip flop", "По очереди: A, B, A, B…; IsA — прошёл ли поток в A.",
             "In turn: A, B, A, B…; IsA tells whether the flow went to A.", Pins({"exec:exec"}, {"A:exec", "B:exec", "IsA:bool"})),
        Spec(Op::For, "Flow.For", "Flow", "Цикл", "For loop", "Body для Index от First до Last включительно, затем Completed.",
             "Body for Index from First to Last inclusive, then Completed.",
             Pins({"exec:exec", "First:float", "Last:float"}, {"Body:exec", "Completed:exec", "Index:float"})),

        Spec(Op::Print, "Action.Print", "Actions", "Вывести в консоль", "Print", "Пишет значение в консоль редактора и журнал игры.",
             "Writes the value to the editor console and the game log.", Pins({"exec:exec", "Text:any"}, {"then:exec"})),
        Spec(Op::SetPosition, "Action.SetPosition", "Actions", "Задать позицию", "Set position", "Позиция объекта относительно родителя.",
             "Position of the object relative to its parent.", Pins({"exec:exec", "Target:object", "Position:vec3"}, {"then:exec"})),
        Spec(Op::SetRotation, "Action.SetRotation", "Actions", "Задать поворот", "Set rotation", "Поворот в градусах по осям X, Y, Z.",
             "Rotation in degrees around X, Y, Z.", Pins({"exec:exec", "Target:object", "Rotation:vec3"}, {"then:exec"})),
        Spec(Op::SetScale, "Action.SetScale", "Actions", "Задать масштаб", "Set scale", "Масштаб по осям X, Y, Z.", "Scale along X, Y, Z.",
             Pins({"exec:exec", "Target:object", "Scale:vec3"}, {"then:exec"})),
        Spec(Op::Translate, "Action.Translate", "Actions", "Сдвинуть", "Translate", "Прибавляет смещение к позиции.", "Adds the offset to the position.",
             Pins({"exec:exec", "Target:object", "Offset:vec3"}, {"then:exec"})),
        Spec(Op::Rotate, "Action.Rotate", "Actions", "Повернуть", "Rotate", "Прибавляет градусы к повороту.", "Adds the degrees to the rotation.",
             Pins({"exec:exec", "Target:object", "Degrees:vec3"}, {"then:exec"})),
        Spec(Op::CallEvent, "Action.CallEvent", "Actions", "Вызвать событие", "Call event",
             "Запускает «Своё событие» с этим именем в скрипте объекта Target (пустой — сам объект).",
             "Runs the Custom event of this name in the script of Target (empty — this object).",
             Pins({"exec:exec", "Target:object"}, {"then:exec"}), {eventName}),
        Spec(Op::Spawn, "Action.Spawn", "Actions", "Создать копию", "Spawn copy",
             "Копия объекта Template (модель, материал, скрипт, настройки) в точке Position; Object — новый объект.",
             "A copy of Template (model, material, script, settings) at Position; Object is the new object.",
             Pins({"exec:exec", "Template:object", "Position:vec3"}, {"then:exec", "Object:object"})),
        Spec(Op::Destroy, "Action.Destroy", "Actions", "Удалить объект", "Destroy", "Убирает объект Target из сцены в конце кадра.",
             "Removes Target from the scene at the end of the frame.", Pins({"exec:exec", "Target:object"}, {"then:exec"})),
        // Автоматы ИИ: Target — ИИ-персонаж (пустой — сам объект).
        Spec(Op::SendAiEvent, "Ai.SendEvent", "AI", "Отправить событие ИИ", "Send AI event",
             "Событие автомату ИИ-персонажа Target: сработают его переходы «по событию» (в следующем решении).",
             "An event to the state machine of the AI character Target: its event transitions fire (at the next think).",
             Pins({"exec:exec", "Target:object"}, {"then:exec"}), {eventName}),
        Spec(Op::SetAiMemory, "Ai.SetMemory", "AI", "Записать в память ИИ", "Set AI memory",
             "Значение в память ИИ-персонажа Target по ключу (число, логическое, вектор, строка или объект).",
             "A value into the memory of the AI character Target under the key (number, bool, vector, string or object).",
             Pins({"exec:exec", "Target:object", "Value:any"}, {"then:exec"}), {memory}),
        Spec(Op::AiMemoryNumber, "Ai.GetNumber", "AI", "Память ИИ: число", "AI memory: number",
             "Число из памяти ИИ-персонажа (целое тоже); Has — есть ли такое значение.", "A number from the AI character's memory (an int too); Has — whether it is there.",
             Pins({"Target:object"}, {"Value:float", "Has:bool"}), {memory}),
        Spec(Op::AiMemoryBool, "Ai.GetBool", "AI", "Память ИИ: логическое", "AI memory: bool", "Логическое из памяти ИИ-персонажа.",
             "A bool from the AI character's memory.", Pins({"Target:object"}, {"Value:bool", "Has:bool"}), {memory}),
        Spec(Op::AiMemoryVector, "Ai.GetVector", "AI", "Память ИИ: вектор", "AI memory: vector", "Вектор (точка) из памяти ИИ-персонажа.",
             "A vector (a point) from the AI character's memory.", Pins({"Target:object"}, {"Value:vec3", "Has:bool"}), {memory}),
        Spec(Op::AiMemoryObject, "Ai.GetObject", "AI", "Память ИИ: объект", "AI memory: object",
             "Объект из памяти ИИ-персонажа (например, кого он видит).", "An object from the AI character's memory (for example, whom it sees).",
             Pins({"Target:object"}, {"Value:object", "Has:bool"}), {memory}),
        Spec(Op::AiState, "Ai.State", "AI", "Состояние ИИ", "AI state", "Активный путь автомата («Patrol/GoToA»); Running — работает ли автомат.",
             "The machine's active path (\"Patrol/GoToA\"); Running — whether the machine runs.", Pins({"Target:object"}, {"State:string", "Running:bool"})),
        Spec(Op::AiInState, "Ai.InState", "AI", "ИИ в состоянии", "AI in state",
             "Истина, если состояние State активно (само или как родитель активного).", "True when State is active (itself or as a parent of the active one).",
             Pins({"Target:object", "State:string"}, {"Value:bool"})),
        Spec(Op::SetVariable, "Variable.Set", "Variables", "Задать переменную", "Set variable", "Новое значение переменной скрипта.",
             "A new value of a script variable.", Pins({"exec:exec", "Value:variable"}, {"then:exec"}), {variable}),

        Spec(Op::Self, "Object.Self", "Objects", "Сам объект", "Self", "Объект, к которому привязан скрипт.", "The object the script is attached to.",
             Pins({}, {"Object:object"})),
        Spec(Op::FindObject, "Object.Find", "Objects", "Найти объект", "Find object", "Объект сцены по имени; нет такого — пусто.",
             "A scene object by name; none if there is no such object.", Pins({"Name:string"}, {"Object:object"})),
        Spec(Op::GetPosition, "Object.GetPosition", "Objects", "Позиция", "Get position", "Позиция объекта относительно родителя.",
             "Position of the object relative to its parent.", Pins({"Target:object"}, {"Position:vec3"})),
        Spec(Op::GetRotation, "Object.GetRotation", "Objects", "Поворот", "Get rotation", "Поворот объекта в градусах.", "Rotation of the object in degrees.",
             Pins({"Target:object"}, {"Rotation:vec3"})),
        Spec(Op::GetScale, "Object.GetScale", "Objects", "Масштаб", "Get scale", "Масштаб объекта.", "Scale of the object.",
             Pins({"Target:object"}, {"Scale:vec3"})),
        Spec(Op::GetVariable, "Variable.Get", "Variables", "Переменная", "Get variable", "Текущее значение переменной скрипта.",
             "The current value of a script variable.", Pins({}, {"Value:variable"}), {variable}),

        Spec(Op::ConstFloat, "Const.Float", "Constants", "Число", "Number", "Число.", "A number.", Pins({}, {"Value:float"}), {{"value", "float"}}),
        Spec(Op::ConstBool, "Const.Bool", "Constants", "Логическое", "Boolean", "Да или нет.", "True or false.", Pins({}, {"Value:bool"}), {{"value", "bool"}}),
        Spec(Op::ConstVec3, "Const.Vec3", "Constants", "Вектор", "Vector", "Три числа X, Y, Z.", "Three numbers X, Y, Z.", Pins({}, {"Value:vec3"}),
             {{"value", "vec3"}}),
        Spec(Op::ConstString, "Const.String", "Constants", "Строка", "Text", "Текст.", "Text.", Pins({}, {"Value:string"}), {{"value", "string"}}),

        Spec(Op::MakeVec3, "Vec3.Make", "Vectors", "Собрать вектор", "Make vector", "Вектор из X, Y, Z.", "A vector from X, Y, Z.",
             Pins({"X:float", "Y:float", "Z:float"}, {"Vector:vec3"})),
        Spec(Op::Vec3Length, "Vec3.Length", "Vectors", "Длина вектора", "Vector length", "Длина вектора.", "Length of the vector.",
             Pins({"Vector:vec3"}, {"Length:float"})),
        Spec(Op::Vec3Normalize, "Vec3.Normalize", "Vectors", "Направление", "Normalize", "Вектор длины 1 в ту же сторону; нулевой остаётся нулевым.",
             "A vector of length 1 in the same direction; zero stays zero.", Pins({"Vector:vec3"}, {"Out:vec3"})),
        Spec(Op::Vec3Lerp, "Vec3.Lerp", "Vectors", "Смешивание векторов", "Lerp vectors", "A при Alpha = 0, B при Alpha = 1.", "A at Alpha 0, B at Alpha 1.",
             Pins({"A:vec3", "B:vec3", "Alpha:float"}, {"Out:vec3"})),
        Spec(Op::Distance, "Vec3.Distance", "Vectors", "Расстояние", "Distance", "Расстояние между точками A и B.", "Distance between points A and B.",
             Pins({"A:vec3", "B:vec3"}, {"Distance:float"})),
        Spec(Op::BreakVec3, "Vec3.Break", "Vectors", "Разобрать вектор", "Break vector", "X, Y, Z вектора.", "X, Y, Z of a vector.",
             Pins({"Vector:vec3"}, {"X:float", "Y:float", "Z:float"})),
        Spec(Op::Vec3Add, "Vec3.Add", "Vectors", "Сложение векторов", "Add vectors", "A + B.", "A + B.", Pins({"A:vec3", "B:vec3"}, {"Out:vec3"})),
        Spec(Op::Vec3Subtract, "Vec3.Subtract", "Vectors", "Вычитание векторов", "Subtract vectors", "A − B.", "A - B.", Pins({"A:vec3", "B:vec3"}, {"Out:vec3"})),
        Spec(Op::Vec3Scale, "Vec3.Scale", "Vectors", "Вектор × число", "Scale vector", "Каждая компонента вектора × Factor.",
             "Every component times Factor.", Pins({"Vector:vec3", "Factor:float"}, {"Out:vec3"})),

        Spec(Op::Add, "Math.Add", "Math", "Сложение", "Add", "A + B.", "A + B.", Pins({"A:float", "B:float"}, {"Out:float"})),
        Spec(Op::Subtract, "Math.Subtract", "Math", "Вычитание", "Subtract", "A − B.", "A - B.", Pins({"A:float", "B:float"}, {"Out:float"})),
        Spec(Op::Multiply, "Math.Multiply", "Math", "Умножение", "Multiply", "A × B.", "A * B.", Pins({"A:float", "B:float"}, {"Out:float"})),
        Spec(Op::Divide, "Math.Divide", "Math", "Деление", "Divide", "A / B; деление на 0 — ошибка.", "A / B; division by 0 is an error.",
             Pins({"A:float", "B:float"}, {"Out:float"})),
        Spec(Op::Sin, "Math.Sin", "Math", "Синус", "Sine", "sin(In), In в радианах.", "sin(In), In in radians.", Pins({"In:float"}, {"Out:float"})),
        Spec(Op::Cos, "Math.Cos", "Math", "Косинус", "Cosine", "cos(In), In в радианах.", "cos(In), In in radians.", Pins({"In:float"}, {"Out:float"})),
        Spec(Op::Abs, "Math.Abs", "Math", "Модуль", "Absolute", "|In|.", "|In|.", Pins({"In:float"}, {"Out:float"})),
        Spec(Op::Min, "Math.Min", "Math", "Минимум", "Min", "Меньшее из A и B.", "The smaller of A and B.", Pins({"A:float", "B:float"}, {"Out:float"})),
        Spec(Op::Max, "Math.Max", "Math", "Максимум", "Max", "Большее из A и B.", "The larger of A and B.", Pins({"A:float", "B:float"}, {"Out:float"})),
        Spec(Op::Lerp, "Math.Lerp", "Math", "Смешивание (lerp)", "Lerp", "A при Alpha = 0, B при Alpha = 1.", "A at Alpha 0, B at Alpha 1.",
             Pins({"A:float", "B:float", "Alpha:float"}, {"Out:float"})),
        Spec(Op::Clamp, "Math.Clamp", "Math", "Ограничение", "Clamp", "Value между Min и Max.", "Value between Min and Max.",
             Pins({"Value:float", "Min:float", "Max:float"}, {"Out:float"})),
        Spec(Op::Random, "Math.Random", "Math", "Случайное число", "Random", "Случайное число от Min до Max.", "A random number from Min to Max.",
             Pins({"Min:float", "Max:float"}, {"Out:float"})),
        Spec(Op::Time, "Math.Time", "Math", "Время", "Time", "Секунды с начала игры этого объекта.", "Seconds since this object's play started.",
             Pins({}, {"Seconds:float"})),

        Spec(Op::Less, "Logic.Less", "Logic", "Меньше", "Less", "A < B.", "A < B.", Pins({"A:float", "B:float"}, {"Out:bool"})),
        Spec(Op::Greater, "Logic.Greater", "Logic", "Больше", "Greater", "A > B.", "A > B.", Pins({"A:float", "B:float"}, {"Out:bool"})),
        Spec(Op::Equal, "Logic.Equal", "Logic", "Равно", "Equal", "A = B с точностью 0.000001.", "A = B within 0.000001.",
             Pins({"A:float", "B:float"}, {"Out:bool"})),
        Spec(Op::And, "Logic.And", "Logic", "И", "And", "A и B.", "A and B.", Pins({"A:bool", "B:bool"}, {"Out:bool"})),
        Spec(Op::Or, "Logic.Or", "Logic", "Или", "Or", "A или B.", "A or B.", Pins({"A:bool", "B:bool"}, {"Out:bool"})),
        Spec(Op::Not, "Logic.Not", "Logic", "Не", "Not", "Не In.", "Not In.", Pins({"In:bool"}, {"Out:bool"})),

        Spec(Op::KeyDown, "Input.KeyDown", "Input", "Клавиша зажата", "Key down", "Клавиша сейчас нажата.", "The key is down now.",
             Pins({}, {"Down:bool"}), {key}),
        Spec(Op::MousePosition, "Input.MousePosition", "Input", "Позиция мыши", "Mouse position",
             "Курсор в пикселях окна игры от левого верхнего угла; Valid — курсор над окном. Кнопки мыши — клавиши LeftButton, RightButton.",
             "The cursor in game window pixels from the top-left corner; Valid — the cursor is over the window. Mouse buttons are the LeftButton, RightButton keys.",
             Pins({}, {"X:float", "Y:float", "Valid:bool"})),
    };
}

std::vector<std::pair<std::string, int>> BuildKeys()
{
    std::vector<std::pair<std::string, int>> keys;
    for (char c = 'A'; c <= 'Z'; ++c) keys.push_back({std::string(1, c), int(Keys::A) + (c - 'A')});
    for (int d = 0; d <= 9; ++d) keys.push_back({"D" + std::to_string(d), int(Keys::D0) + d});
    for (int f = 1; f <= 12; ++f) keys.push_back({"F" + std::to_string(f), int(Keys::F1) + f - 1});
    for (int n = 0; n <= 9; ++n) keys.push_back({"NumPad" + std::to_string(n), int(Keys::NumPad0) + n});
    const std::pair<const char*, Keys> named[] = {
        {"Space", Keys::Space}, {"Enter", Keys::Enter}, {"Escape", Keys::Escape}, {"Tab", Keys::Tab}, {"Back", Keys::Back},
        {"Left", Keys::Left}, {"Right", Keys::Right}, {"Up", Keys::Up}, {"Down", Keys::Down},
        {"Shift", Keys::Shift}, {"Control", Keys::Control}, {"Alt", Keys::Alt},
        {"LeftShift", Keys::LeftShift}, {"RightShift", Keys::RightShift}, {"LeftControl", Keys::LeftControl}, {"RightControl", Keys::RightControl},
        {"Insert", Keys::Insert}, {"Delete", Keys::Delete}, {"Home", Keys::Home}, {"End", Keys::End}, {"PageUp", Keys::PageUp}, {"PageDown", Keys::PageDown},
        {"LeftButton", Keys::LeftButton}, {"RightButton", Keys::RightButton}, {"MiddleButton", Keys::MiddleButton}};
    for (const auto& [name, key] : named) keys.push_back({name, int(key)});
    return keys;
}

const std::vector<std::pair<std::string, int>>& KeyTable()
{
    static const std::vector<std::pair<std::string, int>> keys = BuildKeys();
    return keys;
}
} // namespace

const char* TypeName(ValueType type)
{
    switch (type)
    {
    case ValueType::Exec: return "exec";
    case ValueType::Bool: return "bool";
    case ValueType::Float: return "float";
    case ValueType::Vec3: return "vec3";
    case ValueType::String: return "string";
    case ValueType::Object: return "object";
    case ValueType::Any: return "any";
    case ValueType::Variable: return "variable";
    }
    return "?";
}

std::string ToText(const Value& value)
{
    char buffer[96];
    if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? "true" : "false";
    if (std::holds_alternative<float>(value))
    {
        std::snprintf(buffer, sizeof(buffer), "%g", std::get<float>(value));
        return buffer;
    }
    if (std::holds_alternative<glm::vec3>(value))
    {
        const glm::vec3 v = std::get<glm::vec3>(value);
        std::snprintf(buffer, sizeof(buffer), "(%g, %g, %g)", v.x, v.y, v.z);
        return buffer;
    }
    if (std::holds_alternative<std::string>(value)) return std::get<std::string>(value);
    if (std::holds_alternative<ObjectRef>(value))
    {
        const uint64_t id = std::get<ObjectRef>(value).id;
        return id ? "object #" + std::to_string(id) : "none";
    }
    return {};
}

Value DefaultValue(ValueType type)
{
    switch (type)
    {
    case ValueType::Bool: return false;
    case ValueType::Float: return 0.0f;
    case ValueType::Vec3: return glm::vec3(0.0f);
    case ValueType::String: return std::string();
    case ValueType::Object: return ObjectRef{};
    default: return {};
    }
}

const std::vector<NodeSpec>& Specs()
{
    static const std::vector<NodeSpec> specs = BuildSpecs();
    return specs;
}

const NodeSpec* FindSpec(const std::string& type)
{
    static const std::unordered_map<std::string, const NodeSpec*> index = [] {
        std::unordered_map<std::string, const NodeSpec*> map;
        for (const auto& spec : Specs()) map.emplace(spec.info.type, &spec);
        return map;
    }();
    const auto found = index.find(type);
    return found == index.end() ? nullptr : found->second;
}

const std::vector<NodeInfo>& Catalog()
{
    static const std::vector<NodeInfo> catalog = [] {
        std::vector<NodeInfo> nodes;
        for (const auto& spec : Specs()) nodes.push_back(spec.info);
        return nodes;
    }();
    return catalog;
}

const NodeInfo* FindNode(const std::string& type)
{
    const NodeSpec* spec = FindSpec(type);
    return spec ? &spec->info : nullptr;
}

std::vector<PinInfo> NodePins(const NodeInfo& node, int sequenceOutputs)
{
    std::vector<PinInfo> pins = node.pins;
    if (node.type == "Flow.Sequence")
        for (int i = 0; i < sequenceOutputs; ++i) pins.push_back({"then" + std::to_string(i), false, ValueType::Exec});
    return pins;
}

const std::vector<std::string>& KeyNames()
{
    static const std::vector<std::string> names = [] {
        std::vector<std::string> list;
        for (const auto& key : KeyTable()) list.push_back(key.first);
        return list;
    }();
    return names;
}

int KeyCode(const std::string& name)
{
    for (const auto& key : KeyTable())
        if (key.first == name) return key.second;
    return 0;
}

std::string CatalogJson()
{
    using Json = nlohmann::ordered_json;
    Json nodes = Json::array();
    for (const auto& node : Catalog())
    {
        Json pins = Json::array(), properties = Json::array();
        for (const auto& pin : node.pins) pins.push_back({{"name", pin.name}, {"input", pin.input}, {"type", TypeName(pin.type)}});
        for (const auto& property : node.properties) properties.push_back({{"name", property.name}, {"kind", property.kind}});
        nodes.push_back({{"type", node.type}, {"group", node.group}, {"name", {{"ru", node.nameRu}, {"en", node.nameEn}}},
                         {"hint", {{"ru", node.hintRu}, {"en", node.hintEn}}}, {"event", node.event}, {"pins", pins}, {"properties", properties}});
    }
    Json root = {{"format", FormatName}, {"formatVersion", FormatVersion}, {"schema", SchemaId}, {"schemaVersion", SchemaVersion},
                 {"maxSequenceOutputs", MaxSequenceOutputs}, {"keys", KeyNames()}, {"nodes", nodes}};
    return root.dump(2);
}
} // namespace Script
