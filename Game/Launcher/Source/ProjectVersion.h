#pragma once
#include "../../../Engine/Runtime/Project.h"
#include <vector>
#include <cstdint>

namespace ProjectVersion {
// Release numbers are dotted integers. Do not infer upgrade direction for
// custom labels, unpinned projects, or a newer project's engine version.
inline bool IsNewer(const std::string& current, const std::string& previous) {
    auto parse=[](const std::string& text) {
        std::vector<uint32_t> parts; uint64_t value=0; bool digit=false;
        for(char c:text) {
            if(c=='.' && digit){parts.push_back(static_cast<uint32_t>(value));value=0;digit=false;}
            else if(c>='0' && c<='9') {value=value*10+c-'0';digit=true;if(value>UINT32_MAX)return std::vector<uint32_t>{};}
            else return std::vector<uint32_t>{};
        }
        if(!digit)return std::vector<uint32_t>{};
        parts.push_back(static_cast<uint32_t>(value));return parts;
    };
    auto a=parse(current),b=parse(previous);if(a.empty()||b.empty())return false;
    const auto size=a.size()>b.size()?a.size():b.size();a.resize(size);b.resize(size);
    return a>b;
}
inline std::filesystem::path CopyDestination(const std::filesystem::path& project,const std::string& version) {
    const auto root=project.parent_path();
    const auto base=root.parent_path()/(root.filename().wstring()+L"-"+std::wstring(version.begin(),version.end()));
    auto destination=base;
    for(unsigned i=2;std::filesystem::exists(destination);++i)destination=base.wstring()+L"-"+std::to_wstring(i);
    return destination;
}
}
