#pragma once

#include "IAudioDecoder.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <wrl/client.h>
#include <vector>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace audio
{
    class MediaFoundationDecoder final : public IAudioDecoder
    {
    public:
        ~MediaFoundationDecoder() override;

        bool Open(const std::wstring& filePath) override;

        const WaveFormat& GetFormat() const override
        {
            return format_;
        }

        uint64_t GetLengthFrames() const override
        {
            return lengthFrames_;
        }

        uint64_t ReadFrames(void* destination, uint64_t framesToRead) override;
        bool SeekFrames(uint64_t frameIndex) override;

    private:
        bool ReadNextSampleToCache();

        Microsoft::WRL::ComPtr<IMFSourceReader> reader_;

        WaveFormat format_{};
        uint64_t lengthFrames_ = 0;

        std::vector<uint8_t> cachedBytes_;
        std::size_t cachedOffset_ = 0;
    };
}
