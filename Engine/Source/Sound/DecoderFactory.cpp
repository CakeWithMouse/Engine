#include "Engine/Sound/DecoderFactory.h"
#include "Engine/Sound/WavDecoder.h"
#include "Engine/Sound/MediaFoundationDecoder.h"

#include <algorithm>

namespace audio
{
    namespace
    {
        bool EndsWithInsensitive(
            const std::wstring& str,
            const std::wstring& suffix
        )
        {
            if (suffix.size() > str.size())
            {
                return false;
            }

            auto it1 = str.rbegin();
            auto it2 = suffix.rbegin();

            for (; it2 != suffix.rend(); ++it1, ++it2)
            {
                if (towlower(*it1) != towlower(*it2))
                {
                    return false;
                }
            }

            return true;
        }
    }

    std::unique_ptr<IAudioDecoder> DecoderFactory::Create(
        const std::wstring& filePath
    )
    {
        if (EndsWithInsensitive(filePath, L".wav"))
        {
            return std::make_unique<WavDecoder>();
        }

        return std::make_unique<MediaFoundationDecoder>();
    }
}
