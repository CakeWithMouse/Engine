#include "Engine/Sound/WavDecoder.h"

#include <cstring>

namespace audio
{
    WavDecoder::~WavDecoder()
    {
        if (file_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(file_);
            file_ = INVALID_HANDLE_VALUE;
        }
    }

    bool WavDecoder::Open(const std::wstring& filePath)
    {
        file_ = CreateFileW(
            filePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (file_ == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        auto fail = [this]()
        {
            CloseHandle(file_);
            file_ = INVALID_HANDLE_VALUE;
            return false;
        };

        auto readExact = [this](void* buffer, DWORD bytes) -> bool
        {
            DWORD read = 0;
            return ReadFile(file_, buffer, bytes, &read, nullptr) && read == bytes;
        };

        auto skipBytes = [this](uint64_t bytes) -> bool
        {
            LARGE_INTEGER li{};
            li.QuadPart = static_cast<LONGLONG>(bytes);
            return SetFilePointerEx(file_, li, nullptr, FILE_CURRENT) != FALSE;
        };

        // RIFF-заголовок: "RIFF" <размер> "WAVE".
        char riffId[4]{};
        uint32_t riffSize = 0;
        char waveId[4]{};

        if (!readExact(riffId, 4) || !readExact(&riffSize, 4) ||
            !readExact(waveId, 4) ||
            memcmp(riffId, "RIFF", 4) != 0 || memcmp(waveId, "WAVE", 4) != 0)
        {
            return fail();
        }

        // Идём по чанкам, пока не найдём "fmt " и "data".
        bool haveFormat = false;
        bool haveData = false;

        while (!haveFormat || !haveData)
        {
            char chunkId[4]{};
            uint32_t chunkSize = 0;

            if (!readExact(chunkId, 4) || !readExact(&chunkSize, 4))
            {
                return fail();
            }

            // Чанки выравниваются на чётный размер.
            uint64_t paddedSize = chunkSize + (chunkSize & 1u);

            if (memcmp(chunkId, "fmt ", 4) == 0)
            {
                // Минимум WAVEFORMATEX (16 байт) + размер самой структуры.
                if (chunkSize < sizeof(WAVEFORMAT))
                {
                    return fail();
                }

                format_.formatBytes.resize(chunkSize);
                if (!readExact(format_.formatBytes.data(), chunkSize))
                {
                    return fail();
                }

                if (chunkSize & 1u && !skipBytes(1))
                {
                    return fail();
                }

                const WAVEFORMATEX* wf = format_.Get();
                format_.blockAlign = wf->nBlockAlign;
                format_.channels = wf->nChannels;
                format_.sampleRate = wf->nSamplesPerSec;

                if (format_.blockAlign == 0)
                {
                    return fail();
                }

                haveFormat = true;
            }
            else if (memcmp(chunkId, "data", 4) == 0)
            {
                LARGE_INTEGER pos{};
                if (!SetFilePointerEx(file_, {}, &pos, FILE_CURRENT))
                {
                    return fail();
                }

                dataStart_ = static_cast<uint64_t>(pos.QuadPart);
                dataLength_ = chunkSize;

                if (!skipBytes(paddedSize))
                {
                    return fail();
                }

                haveData = true;
            }
            else if (!skipBytes(paddedSize))
            {
                return fail();
            }
        }

        return SeekFrames(0);
    }

    uint64_t WavDecoder::ReadFrames(void* destination, uint64_t framesToRead)
    {
        if (file_ == INVALID_HANDLE_VALUE || format_.blockAlign == 0)
        {
            return 0;
        }

        uint64_t framesAvailable = GetLengthFrames();

        if (positionFrames_ >= framesAvailable)
        {
            return 0;
        }

        uint64_t framesLeft = framesAvailable - positionFrames_;
        uint64_t framesToActuallyRead = min(framesToRead, framesLeft);

        DWORD bytesToRead =
            static_cast<DWORD>(framesToActuallyRead * format_.blockAlign);

        DWORD bytesRead = 0;

        BOOL ok = ReadFile(
            file_,
            destination,
            bytesToRead,
            &bytesRead,
            nullptr
        );

        if (!ok || bytesRead == 0)
        {
            return 0;
        }

        uint64_t framesRead = bytesRead / format_.blockAlign;
        positionFrames_ += framesRead;

        return framesRead;
    }

    bool WavDecoder::SeekFrames(uint64_t frameIndex)
    {
        if (file_ == INVALID_HANDLE_VALUE || format_.blockAlign == 0)
        {
            return false;
        }

        uint64_t framesAvailable = GetLengthFrames();

        if (frameIndex > framesAvailable)
        {
            frameIndex = framesAvailable;
        }

        uint64_t bytePosition = dataStart_ + frameIndex * format_.blockAlign;

        LARGE_INTEGER li{};
        li.QuadPart = static_cast<LONGLONG>(bytePosition);

        if (!SetFilePointerEx(file_, li, nullptr, FILE_BEGIN))
        {
            return false;
        }

        positionFrames_ = frameIndex;
        return true;
    }
}
