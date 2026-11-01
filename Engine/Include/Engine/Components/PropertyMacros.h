#pragma once
#include <Engine/Components/GameComponents.h>
#include <Engine/Components/EditorProperties.h> // compatibility with existing project headers
#include <nlohmann/json.hpp>
#include <cmath>
#include <limits>
#include <type_traits>
#include <stdexcept>

// A field list is visited for schema, save, validation and load. JSON stays internal.
namespace EngineProperties {
using Json = nlohmann::json;
struct Visitor {
    enum Mode { Schema, Save, Validate, Load } mode;
    Json& data;
    template<class T> void Enum(const char* name,T& value,const char* label,
        std::initializer_list<std::pair<std::remove_const_t<T>,const char*>> choices) {
        if(mode==Schema) {
            Json options=Json::array();for(const auto& choice:choices)options.push_back({{"value",choice.first},{"label",choice.second}});
            data["fields"].push_back({{"path",std::string("/")+name},{"label",label},{"type","enum"},{"options",options}});return;
        }
        if(mode!=Save && data.contains(name)) {
            bool found=false;for(const auto& choice:choices)if(data.at(name)==Json(choice.first))found=true;
            if(!found)throw std::invalid_argument(std::string(name)+": unknown choice");
        }
        Field(name,value,label);
    }
    template<class T> void Field(const char* name, T& value, const char* label,
        double minimum=-1e12, double maximum=1e12) {
        using Value = std::remove_const_t<T>;
        static_assert(std::is_arithmetic_v<Value> || std::is_same_v<Value,std::string>, "Property supports bool, numbers and std::string");
        const char* type = std::is_same_v<Value,bool> ? "bool" : std::is_integral_v<Value> ? "integer" : std::is_floating_point_v<Value> ? "number" : "string";
        if(mode==Schema) { data["fields"].push_back({{"path",std::string("/")+name},{"label",label},{"type",type},{"min",minimum},{"max",maximum}}); return; }
        if(mode==Save) { data[name]=value; return; }
        if(!data.contains(name)) return; // newly added fields retain constructor defaults
        const auto& input=data.at(name);
        if constexpr(std::is_same_v<Value,bool>) { if(!input.is_boolean()) throw std::invalid_argument(std::string(name)+": expected boolean"); }
        else if constexpr(std::is_arithmetic_v<Value>) {
            if(!input.is_number() || (std::is_integral_v<Value> && !input.is_number_integer())) throw std::invalid_argument(std::string(name)+": expected number");
            const double n=input.get<double>();
            if(!std::isfinite(n)||n<minimum||n>maximum||n<std::numeric_limits<Value>::lowest()||n>(std::numeric_limits<Value>::max)()) throw std::invalid_argument(std::string(name)+": outside range");
        } else if(!input.is_string()||input.get_ref<const std::string&>().size()>4096) throw std::invalid_argument(std::string(name)+": expected string");
        if constexpr(!std::is_const_v<T>) if(mode==Load) value=input.get<Value>();
    }
};
template<class T> auto Notify(T& object,int)->decltype(object.OnPropertiesChanged(),void()) {object.OnPropertiesChanged();}
template<class T> void Notify(T&,long) {}
template<class Base> Json Schema(const Base* base) {
    if constexpr(std::is_base_of_v<GameComponent,Base> || std::is_base_of_v<EditorPropertyProvider,Base>) {
        auto text=base->Base::DescribeEditorProperties();
        if(!text.empty())return Json::parse(text);
    }
    return Json{{"version",1},{"fields",Json::array()}};
}
inline Json Data(const std::string& text) { return text.empty()?Json{{"version",1}}:Json::parse(text); }
}
// GameComponent supplies the editor schema contract. No extra base is needed.
#define ENGINE_PROPERTY_FIELD(Member, ...) visitor.Field(#Member, this->Member, __VA_ARGS__);
#define ENGINE_ENUM_PROPERTY_FIELD(Member, Label, ...) visitor.Enum(#Member, this->Member, Label, {__VA_ARGS__});
#define ENGINE_PROPERTIES(Base, ...) \
    std::string DescribeEditorProperties() const override { \
        auto data=EngineProperties::Schema<Base>(this); \
        EngineProperties::Visitor visitor{EngineProperties::Visitor::Schema,data}; __VA_ARGS__ return data.dump(); } \
    std::string SaveProjectData() const override { \
        auto data=EngineProperties::Data(Base::SaveProjectData()); \
        EngineProperties::Visitor visitor{EngineProperties::Visitor::Save,data}; __VA_ARGS__ return data.dump(); } \
    void LoadProjectData(const std::string& text) override { \
        if(text.empty())return; auto data=EngineProperties::Data(text); \
        if(!data.is_object()||data.value("version",0)!=1)throw std::invalid_argument("Unsupported property data version"); \
        EngineProperties::Visitor visitor{EngineProperties::Visitor::Validate,data}; __VA_ARGS__ \
        const auto before=SaveProjectData(); const bool baseData=!Base::SaveProjectData().empty(); \
        try { if(baseData)Base::LoadProjectData(text); visitor.mode=EngineProperties::Visitor::Load; __VA_ARGS__ EngineProperties::Notify(*this,0); } \
        catch(...) { if(baseData)Base::LoadProjectData(before);data=EngineProperties::Data(before);visitor.mode=EngineProperties::Visitor::Load; __VA_ARGS__ EngineProperties::Notify(*this,0);throw; } }

// Markers consumed by ComponentGenerator; place immediately above a single field.
#define ENGINE_PROPERTY(...)
#define ENGINE_ENUM_PROPERTY(...)
// Once per class. Generated member definitions can access private fields.
#define ENGINE_GENERATED_PROPERTIES(Base) \
    using EnginePropertyBase = Base; \
    std::string DescribeEditorProperties() const override; \
    std::string SaveProjectData() const override; \
    void LoadProjectData(const std::string& text) override;
