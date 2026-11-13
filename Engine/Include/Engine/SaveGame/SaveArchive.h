#pragma once
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map>
#include <cmath>
#include <limits>
#include <stdexcept>

// Module-local state visitor. Overrides call their immediate base, then add a named section.
// Serialize methods must only visit data: no spawning, IO, subscriptions or external side effects.
class SaveArchive
{
public:
    using Json = nlohmann::json;
    enum class Mode { Write, Validate, Read };
    SaveArchive(Json& data, Mode mode, const std::map<uint64_t, uint64_t>* ids = nullptr)
        : data_(data), mode_(mode), ids_(ids) {
        if (mode == Mode::Write) data = Json::object();
        if (!data.is_object()) throw std::runtime_error("Save state must be an object");
    }
    bool IsReading() const { return mode_ == Mode::Read; }
    bool IsWriting() const { return mode_ == Mode::Write; }
    // Data of this archive level. For deferred restores: copy it now, read it later without remapping.
    const Json& Raw() const { return data_; }
    template<class T> void Field(const char* name, T& value) {
        if (IsWriting()) { data_[name] = value; Check(data_[name]); return; }
        if (!data_.contains(name)) return; // New fields keep constructor defaults.
        const auto& input = data_.at(name);
        Check(input);
        if constexpr (std::is_same_v<T, bool>) {
            if (!input.is_boolean()) throw std::runtime_error(std::string(name) + ": expected boolean");
        } else if constexpr (std::is_arithmetic_v<T>) {
            if (!input.is_number() || (std::is_integral_v<T> && !input.is_number_integer()))
                throw std::runtime_error(std::string(name) + ": expected number");
            const long double number = input.get<long double>();
            if (number < (std::numeric_limits<T>::lowest)() || number > (std::numeric_limits<T>::max)())
                throw std::runtime_error(std::string(name) + ": outside numeric range");
        }
        T decoded = input.get<T>();
        Check(Json(decoded)); // Conversion to narrower container elements must remain finite.
        if (IsReading()) value = std::move(decoded);
    }
    template<glm::length_t N, class T, glm::qualifier Q> void Field(const char* name, glm::vec<N,T,Q>& value) {
        std::vector<T> array; for (int i=0;i<N;++i) array.push_back(value[i]);
        if (mode_==Mode::Validate) { SaveArchive reader(data_,Mode::Read,ids_); reader.Field(name,array); }
        else Field(name, array);
        if (array.size()!=N) throw std::runtime_error(std::string(name)+": invalid vector size");
        for(const auto component:array) if(!std::isfinite(component)) throw std::runtime_error(std::string(name)+": non-finite vector");
        if (IsReading()) for (int i=0;i<N;++i) value[i]=array[i];
    }
    template<class F> void Section(const char* name, F&& visit) {
        if (!IsWriting() && !data_.contains(name)) return;
        SaveArchive child(data_[name], mode_, ids_); visit(child);
    }
    // Save component IDs, remap after ALL components have been constructed. Zero is null.
    void Reference(const char* name, uint64_t& value) {
        auto id=value; Field(name,id);
        if (!IsWriting() && id && ids_) {
            const auto found=ids_->find(id);
            if(found==ids_->end()) throw std::runtime_error(std::string(name)+": missing saved object reference");
            id=found->second;
        }
        if(IsReading()) value=id;
    }
    void Version(unsigned current) {
        unsigned version=current; Field("version",version);
        if (!IsWriting() && data_.contains("version") && data_.at("version").get<unsigned>()>current)
            throw std::runtime_error("Unsupported save state version");
    }
    static void Check(const Json& data, unsigned depth=0) {
        if(depth>64) throw std::runtime_error("Save state nesting limit exceeded");
        if(data.is_number_float() && !std::isfinite(data.get<double>())) throw std::runtime_error("Non-finite save number");
        if(data.is_string() && data.get_ref<const std::string&>().size()>16*1024*1024) throw std::runtime_error("Save string too large");
        if(data.is_structured()) for(const auto& value:data) Check(value,depth+1);
    }
private:
    Json& data_; Mode mode_; const std::map<uint64_t,uint64_t>* ids_;
};
