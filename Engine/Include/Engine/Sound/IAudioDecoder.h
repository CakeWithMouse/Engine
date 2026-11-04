#pragma once

#include "AudioTypes.h"
#include <string>

namespace audio
{
    class IAudioDecoder
    {
    public:
        virtual ~IAudioDecoder() = default;

        virtual bool Open(const std::wstring& filePath) = 0;

        [[nodiscard]] virtual const WaveFormat& GetFormat() const = 0;

        // Если длина неизвестна, можно вернуть 0.
        [[nodiscard]] virtual uint64_t GetLengthFrames() const = 0;

        // Читает запрошенное количество аудио-фреймов.
        // Возвращает реально прочитанное количество фреймов.
        virtual uint64_t ReadFrames(void* destination, uint64_t framesToRead) = 0;

        virtual bool SeekFrames(uint64_t frameIndex) = 0;
    };
}
