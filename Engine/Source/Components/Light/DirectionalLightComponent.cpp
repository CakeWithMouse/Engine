#include <Engine/Components/Light/DirectionalLightComponent.h>
#include "../../../ThirdParty/nlohmann/json.hpp"
#include <cmath>
#include <stdexcept>

using Json = nlohmann::json;

DirectionalLightComponent::DirectionalLightComponent()
{
    SetRotation({-55, -30, 0});
}

std::string DirectionalLightComponent::SaveProjectData() const
{
    return Json{{"version", 1}, {"color", {color_.x, color_.y, color_.z}},
        {"intensity", intensity_}, {"enabled", enabled_}, {"shadows", shadows_},
        {"shadowDistance", shadowDistance_}}.dump();
}

void DirectionalLightComponent::LoadProjectData(const std::string& text)
{
    if (text.empty()) return;
    const auto data = Json::parse(text);
    if (!data.is_object() || data.size() != 6 || data.at("version") != 1 ||
        !data.at("color").is_array() || data.at("color").size() != 3 ||
        !data.at("enabled").is_boolean() || !data.at("shadows").is_boolean())
        throw std::invalid_argument("Invalid directional light settings");
    auto number = [](const Json& value, float low, float high) {
        if (!value.is_number()) throw std::invalid_argument("Expected light parameter number");
        const float result = value.get<float>();
        if (!std::isfinite(result) || result < low || result > high)
            throw std::invalid_argument("Light parameter outside allowed range");
        return result;
    };
    const glm::vec3 color(number(data["color"][0], 0, 1), number(data["color"][1], 0, 1), number(data["color"][2], 0, 1));
    const float intensity = number(data.at("intensity"), 0, 1000);
    const float distance = number(data.at("shadowDistance"), 100, 100000);
    color_ = color; intensity_ = intensity; shadowDistance_ = distance;
    enabled_ = data["enabled"].get<bool>(); shadows_ = data["shadows"].get<bool>();
}

std::string DirectionalLightComponent::DescribeEditorProperties() const
{
    return R"({"version":1,"fields":[
        {"path":"/enabled","type":"bool","label":"Включён","group":"Направленный свет"},
        {"path":"/color","type":"color","label":"Цвет","group":"Направленный свет"},
        {"path":"/intensity","type":"number","label":"Интенсивность","min":0,"max":1000,"step":0.1,"group":"Направленный свет"},
        {"path":"/shadows","type":"bool","label":"Тени","group":"Направленный свет"},
        {"path":"/shadowDistance","type":"number","label":"Дальность теней","min":100,"max":100000,"step":10,"group":"Направленный свет","description":"Расстояние от камеры. Меньшее значение даёт более чёткие тени вблизи."}
    ]})";
}
