#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/Components/ProjectObject.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

ENGINE_OBJECT("City Rules", "CuriousCity.CityRules")
class CityRulesObject
{
public:
    int balance = 500;
    int buildingLimit = 12;
};

template<> struct ProjectObjectAdapter<CityRulesObject>
{
    static std::unique_ptr<CityRulesObject> Create() { return std::make_unique<CityRulesObject>(); }
    static void Tick(CityRulesObject&, float) {}
    static std::string Save(const CityRulesObject& value)
    {
        return nlohmann::json{{"version", 1}, {"balance", value.balance}, {"buildingLimit", value.buildingLimit}}.dump();
    }
    static void Load(CityRulesObject& value, const std::string& text)
    {
        if (text.empty()) return;
        const auto data = nlohmann::json::parse(text);
        if (data.at("version") != 1) throw std::runtime_error("Unsupported CityRulesObject schema");
        value.balance = data.at("balance").get<int>();
        value.buildingLimit = data.at("buildingLimit").get<int>();
    }
};
