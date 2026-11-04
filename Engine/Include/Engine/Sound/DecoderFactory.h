#pragma once

#include "IAudioDecoder.h"
#include <memory>
#include <string>

namespace audio
{
    class DecoderFactory final
    {
    public:
        static std::unique_ptr<IAudioDecoder> Create(const std::wstring& filePath);
    };
}
