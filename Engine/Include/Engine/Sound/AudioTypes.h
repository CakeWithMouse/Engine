#pragma once

#include <Windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <xaudio2.h>

#include <cstdint>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>

namespace audio
{
    template <typename T, std::size_t Alignment>
    class AlignedAllocator final
    {
    public:
        using value_type = T;

        template <typename U>
        struct rebind
        {
            using other = AlignedAllocator<U, Alignment>;
        };

        AlignedAllocator() noexcept = default;

        template <typename U>
        AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept
        {
        }

        [[nodiscard]] T* allocate(std::size_t n)
        {
            if (n == 0) return nullptr;
            void* p = _aligned_malloc(n * sizeof(T), Alignment);
            if (!p) throw std::bad_alloc();
            return static_cast<T*>(p);
        }

        void deallocate(T* p, std::size_t) noexcept
        {
            _aligned_free(p);
        }
    };

    using AudioBytes = std::vector<uint8_t, AlignedAllocator<uint8_t, 16>>;

    struct WaveFormat final
    {
        std::vector<uint8_t> formatBytes;

        UINT32 blockAlign = 0;
        UINT32 channels = 0;
        UINT32 sampleRate = 0;

        [[nodiscard]] const WAVEFORMATEX* Get() const noexcept
        {
            return reinterpret_cast<const WAVEFORMATEX*>(formatBytes.data());
        }
    };

    struct WaveFormatKey final
    {
        uint16_t formatTag = 0;
        uint16_t channels = 0;
        uint32_t sampleRate = 0;
        uint16_t bitsPerSample = 0;
        uint16_t blockAlign = 0;

        bool operator==(const WaveFormatKey& rhs) const noexcept
        {
            return
                formatTag == rhs.formatTag &&
                channels == rhs.channels &&
                sampleRate == rhs.sampleRate &&
                bitsPerSample == rhs.bitsPerSample &&
                blockAlign == rhs.blockAlign;
        }

        bool operator!=(const WaveFormatKey& rhs) const noexcept
        {
            return !(*this == rhs);
        }
    };

    inline WaveFormatKey MakeWaveFormatKey(const WAVEFORMATEX* fmt)
    {
        WaveFormatKey key{};
        key.formatTag = fmt->wFormatTag;
        key.channels = fmt->nChannels;
        key.sampleRate = fmt->nSamplesPerSec;
        key.bitsPerSample = fmt->wBitsPerSample;
        key.blockAlign = fmt->nBlockAlign;
        return key;
    }
}
