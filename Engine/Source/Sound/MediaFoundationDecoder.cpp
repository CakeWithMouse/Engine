#include "Engine/Sound/MediaFoundationDecoder.h"
#include <new>

namespace
{
    bool BuildWaveFormatFromMediaType(IMFMediaType* mediaType, audio::WaveFormat& out)
    {
        if (!mediaType)
        {
            return false;
        }

        GUID subtype{};

        UINT32 channels = 0;
        UINT32 sampleRate = 0;
        UINT32 bitsPerSample = 0;
        UINT32 blockAlign = 0;
        UINT32 avgBytesPerSec = 0;

        HRESULT hr = mediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (FAILED(hr))
        {
            return false;
        }

        hr = mediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
        if (FAILED(hr) || channels == 0)
        {
            return false;
        }

        hr = mediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
        if (FAILED(hr) || sampleRate == 0)
        {
            return false;
        }

        hr = mediaType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlign);

        if (FAILED(hr) || blockAlign == 0)
        {
            hr = mediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);

            if (FAILED(hr) || bitsPerSample == 0)
            {
                return false;
            }

            blockAlign = channels * bitsPerSample / 8;
        }
        else
        {
            hr = mediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);

            if (FAILED(hr) || bitsPerSample == 0)
            {
                bitsPerSample = blockAlign * 8 / channels;
            }
        }

        hr = mediaType->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &avgBytesPerSec);

        if (FAILED(hr) || avgBytesPerSec == 0)
        {
            avgBytesPerSec = blockAlign * sampleRate;
        }

        bool isPcm = IsEqualGUID(subtype, MFAudioFormat_PCM);
        bool isFloat = IsEqualGUID(subtype, MFAudioFormat_Float);

        if (!isPcm && !isFloat)
        {
            return false;
        }

        if (channels == 0 || sampleRate == 0 || blockAlign == 0 || bitsPerSample == 0)
        {
            return false;
        }

        WAVEFORMATEX wfx{};

        wfx.wFormatTag = isFloat ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
        wfx.nChannels = static_cast<WORD>(channels);
        wfx.nSamplesPerSec = sampleRate;
        wfx.nAvgBytesPerSec = avgBytesPerSec;
        wfx.nBlockAlign = static_cast<WORD>(blockAlign);
        wfx.wBitsPerSample = static_cast<WORD>(bitsPerSample);
        wfx.cbSize = 0;

        out.formatBytes.assign(
            reinterpret_cast<uint8_t*>(&wfx),
            reinterpret_cast<uint8_t*>(&wfx) + sizeof(wfx)
        );

        out.blockAlign = blockAlign;
        out.channels = channels;
        out.sampleRate = sampleRate;

        return true;
    }
}

namespace audio
{
    namespace
    {
        bool EnsureMediaFoundationStarted()
        {
            static bool started = false;
            static bool ok = false;

            if (!started)
            {
                HRESULT hr = MFStartup(MF_VERSION);
                ok = SUCCEEDED(hr);
                started = true;
            }

            return ok;
        }
    }

    MediaFoundationDecoder::~MediaFoundationDecoder()
    {
        reader_.Reset();
    }

    bool MediaFoundationDecoder::Open(const std::wstring& filePath)
    {
        if (!EnsureMediaFoundationStarted())
        {
            return false;
        }

        HRESULT hr = MFCreateSourceReaderFromURL(
            filePath.c_str(),
            nullptr,
            reader_.GetAddressOf()
        );

        if (FAILED(hr))
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IMFMediaType> partialType;

        hr = MFCreateMediaType(&partialType);
        if (FAILED(hr))
        {
            return false;
        }

        partialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        partialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);

        hr = reader_->SetCurrentMediaType(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
            nullptr,
            partialType.Get()
        );

        if (FAILED(hr))
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IMFMediaType> currentType;

        hr = reader_->GetCurrentMediaType(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
            currentType.GetAddressOf()
        );

        if (FAILED(hr))
        {
            return false;
        }

        if (!BuildWaveFormatFromMediaType(currentType.Get(), format_))
        {
            return false;
        }

        // Получаем длительность, если источник ее отдает.
        PROPVARIANT prop{};

        hr = reader_->GetPresentationAttribute(
            MF_SOURCE_READER_MEDIASOURCE,
            MF_PD_DURATION,
            &prop
        );

        if (SUCCEEDED(hr) && prop.vt == VT_I8)
        {
            LONGLONG duration100ns = prop.hVal.QuadPart;

            double seconds =
                static_cast<double>(duration100ns) / 10'000'000.0;

            lengthFrames_ = static_cast<uint64_t>(
                seconds * static_cast<double>(format_.sampleRate)
            );
        }

        PropVariantClear(&prop);

        return true;
    }

    uint64_t MediaFoundationDecoder::ReadFrames(
        void* destination,
        uint64_t framesToRead
    )
    {
        if (!reader_ || format_.blockAlign == 0)
        {
            return 0;
        }

        auto out = static_cast<uint8_t*>(destination);
        uint64_t framesWritten = 0;

        while (framesWritten < framesToRead)
        {
            if (cachedOffset_ < cachedBytes_.size())
            {
                std::size_t bytesLeft = cachedBytes_.size() - cachedOffset_;
                uint64_t framesLeft = bytesLeft / format_.blockAlign;

                uint64_t framesToCopy =
                    min(framesLeft, framesToRead - framesWritten);

                std::size_t bytesToCopy =
                    framesToCopy * format_.blockAlign;

                memcpy(
                    out + framesWritten * format_.blockAlign,
                    cachedBytes_.data() + cachedOffset_,
                    bytesToCopy
                );

                cachedOffset_ += bytesToCopy;
                framesWritten += framesToCopy;

                if (cachedOffset_ == cachedBytes_.size())
                {
                    cachedBytes_.clear();
                    cachedOffset_ = 0;
                }

                continue;
            }

            if (!ReadNextSampleToCache())
            {
                break;
            }
        }

        return framesWritten;
    }

    bool MediaFoundationDecoder::ReadNextSampleToCache()
    {
        Microsoft::WRL::ComPtr<IMFSample> sample;
        DWORD streamFlags = 0;

        HRESULT hr = reader_->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
            0,
            nullptr,
            &streamFlags,
            nullptr,
            sample.GetAddressOf()
        );

        if (FAILED(hr))
        {
            return false;
        }

        if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM)
        {
            return false;
        }

        if (!sample)
        {
            return true;
        }

        Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;

        hr = sample->ConvertToContiguousBuffer(mediaBuffer.GetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        BYTE* data = nullptr;
        DWORD maxLength = 0;
        DWORD currentLength = 0;

        hr = mediaBuffer->Lock(&data, &maxLength, &currentLength);
        if (FAILED(hr))
        {
            return false;
        }

        cachedBytes_.assign(data, data + currentLength);
        cachedOffset_ = 0;

        mediaBuffer->Unlock();

        return true;
    }

    bool MediaFoundationDecoder::SeekFrames(uint64_t frameIndex)
    {
        if (!reader_ || format_.sampleRate == 0)
        {
            return false;
        }

        double seconds =
            static_cast<double>(frameIndex) /
            static_cast<double>(format_.sampleRate);

        LONGLONG time100ns = static_cast<LONGLONG>(seconds * 10'000'000.0);

        PROPVARIANT position{};
        position.vt = VT_I8;
        position.hVal.QuadPart = time100ns;

        HRESULT hr = reader_->SetCurrentPosition(GUID_NULL, position);

        PropVariantClear(&position);

        if (FAILED(hr))
        {
            return false;
        }

        cachedBytes_.clear();
        cachedOffset_ = 0;

        return true;
    }
}
