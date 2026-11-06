#pragma once

#include "IAudioDecoder.h"

namespace audio
{
    class WavDecoder final : public IAudioDecoder
    {
    public:
        WavDecoder() = default;
        ~WavDecoder() override;

        bool Open(const std::wstring& filePath) override;

        const WaveFormat& GetFormat() const override
        {
            return format_;
        }

        uint64_t GetLengthFrames() const override
        {
            if (format_.blockAlign == 0) return 0;
            return dataLength_ / format_.blockAlign;
        }

        uint64_t ReadFrames(void* destination, uint64_t framesToRead) override;
        bool SeekFrames(uint64_t frameIndex) override;

    private:
        HANDLE file_ = INVALID_HANDLE_VALUE;

        WaveFormat format_{};

        uint64_t dataStart_ = 0;
        uint64_t dataLength_ = 0;
        uint64_t positionFrames_ = 0;
    };
}
