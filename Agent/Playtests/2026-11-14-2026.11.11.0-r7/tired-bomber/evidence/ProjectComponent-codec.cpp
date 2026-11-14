#include "ProjectComponent.h"
#include <stdexcept>
void ProjectComponent::BeginPlay() { GameComponent::BeginPlay(); }
void ProjectComponent::Tick(float deltaTime) { GameComponent::Tick(deltaTime); }

std::string BomberReferenceProbe::DescribeEditorProperties() const
{
    return R"json({"version":1,"fields":[
        {"path":"/score","type":"number","label":"Score","min":0,"max":100,"step":0.25,"decimals":2},
        {"path":"/target","type":"number","label":"Target component ID","min":0,"max":1000000,"step":1,"decimals":0}
    ]})json";
}

std::string BomberReferenceProbe::SaveProjectData() const
{
    return nlohmann::json{{"version", codecVersion}, {"score", score}, {"target", targetId}}.dump();
}

void BomberReferenceProbe::LoadProjectData(const std::string& data)
{
    if (data.empty()) return;
    const auto json = nlohmann::json::parse(data);
    const auto version = json.value("version", 0u);
    if (version != 2) throw std::runtime_error("BomberReferenceProbe project-data version mismatch");
    codecVersion = version;
    score = json.at("score").get<float>();
    targetId = json.at("target").get<ComponentId>();
}

void BomberReferenceProbe::RemapProjectReferences(const std::map<ComponentId, ComponentId>& remap)
{
    if (!targetId) return;
    const auto found = remap.find(targetId);
    if (found == remap.end()) throw std::runtime_error("BomberReferenceProbe target reference is missing");
    targetId = found->second;
}
