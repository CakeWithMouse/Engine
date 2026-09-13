#pragma once

#include <string>

struct aiScene;

namespace Assimp
{
    class Importer
    {
    public:
        const aiScene* ReadFile(const std::string&, unsigned int)
        {
            errorString = "Assimp is not bundled with this project";
            return nullptr;
        }

        const char* GetErrorString() const
        {
            return errorString.c_str();
        }

    private:
        std::string errorString;
    };
}
