#include "Engine/Sound/WavDecoder.h"

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

        // Здесь должен быть парсинг RIFF/WAVE:
        // 1. Найти "fmt ".
        // 2. Сохранить WAVEFORMATEX в format_.formatBytes.
        // 3. Найти "data".
        // 4. Запомнить смещение начала данных и размер.
        //
        // Например:
        //
        // format_.formatBytes = ...;
        // format_.blockAlign = wf->nBlockAlign;
        // format_.channels = wf->nChannels;
        // format_.sampleRate = wf->nSamplesPerSec;
        //
        // dataStart_ = позиция начала data-чанка;
        // dataLength_ = размер data-чанка;
        //
        // После этого файл не закрываем.

        // ВАЖНО:
        // Если реализация парсера уже была в предыдущем классе,
        // его нужно адаптировать под режим "файл остаётся открытым".

        return true;
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
