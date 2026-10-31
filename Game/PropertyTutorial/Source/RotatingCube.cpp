#include "RotatingCube.h"
#include <cmath>
#include <nlohmann/json.hpp>

RotatingCube::RotatingCube()
{
    InitCube();
    SetColor({1.0f, 0.65f, 0.2f, 1.0f});
}

void RotatingCube::Tick(float deltaTime)
{
    CubeComponent::Tick(deltaTime);
    // Скорость задаётся в градусах в секунду. Tick работает только в Play.
    if (!enabled_) return;
    auto rotation = GetRotation();
    rotation.y = std::fmod(rotation.y + speed_ * deltaTime, 360.0f);
    SetRotation(rotation);
}

// 1. Данные объекта: попадают в сцену, Undo и копию для Play.
std::string RotatingCube::SaveProjectData() const
{
    return nlohmann::json{{"version",1},{"speed",speed_},{"enabled",enabled_},{"brightness",brightness_}}.dump();
}

// 2. Проверяем весь документ до изменения полей.
void RotatingCube::LoadProjectData(const std::string& text)
{
    if (text.empty()) return;
    const auto data = nlohmann::json::parse(text);
    const auto speed = data.at("speed").get<float>();
    const auto enabled = data.at("enabled").get<bool>();
    const auto brightness = data.value("brightness",1.0f);
    if (data.at("version") != 1 || !std::isfinite(speed) || speed < -720 || speed > 720 ||
        !std::isfinite(brightness) || brightness < 0.1f || brightness > 1.0f)
        throw std::runtime_error("Invalid tutorial cube settings");
    speed_ = speed; enabled_ = enabled; brightness_ = brightness;
    // Это свойство сразу меняет вид объекта и в Edit, и в Play.
    SetColor({brightness_, 0.65f * brightness_, 0.2f * brightness_, 1.0f});
}

// 3. Описание интерфейса. path указывает на ключ сохранённых данных.
// group задаёт вложенные разделы, которые можно сворачивать.
std::string RotatingCube::DescribeEditorProperties() const
{
    return R"json({"version":1,"fields":[
      {"path":"/speed","type":"number","label":"Скорость, °/с","group":"Свойства туториала/Вращение","min":-720,"max":720,"step":15,"decimals":1,"description":"Измените в Edit, затем нажмите Play. Отрицательная скорость меняет направление."},
      {"path":"/enabled","type":"bool","label":"Вращение включено","group":"Свойства туториала/Вращение","description":"Отключите и запустите Play: куб останется неподвижным."},
      {"path":"/brightness","type":"number","label":"Яркость куба","group":"Свойства туториала/Внешний вид","min":0.1,"max":1,"step":0.1,"decimals":2,"description":"Результат виден сразу в редакторе. Это пользовательское поле класса RotatingCube."}
    ]})json";
}
