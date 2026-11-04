#pragma once

#include "IAudioDecoder.h"

#include <xaudio2.h>

#include <atomic>
#include <memory>
#include <vector>

namespace audio
{
    class AudioStream final : public IXAudio2VoiceCallback
    {
    public:
        struct Options final
        {
            bool loop = false;
            float volume = 1.0f;
            uint32_t framesPerBuffer = 16384;
        };

        ~AudioStream();

        bool Start(
            IXAudio2* xaudio,
            std::unique_ptr<IAudioDecoder> decoder,
            const Options& options
        );

        void RequestStop();

        bool IsFinished() const noexcept
        {
            return ended_.load() || forcedStop_.load();
        }

    private:
        struct BufferContext final
        {
            AudioStream* owner = nullptr;
            int index = 0;
        };

        void HandleBufferEnd(int bufferIndex);
        void RefillAndSubmit(int bufferIndex);

        // IXAudio2VoiceCallback
        void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override
        {
        }

        void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override
        {
        }

        void STDMETHODCALLTYPE OnStreamEnd() override;

        void STDMETHODCALLTYPE OnBufferStart(void*) override
        {
        }

        void STDMETHODCALLTYPE OnBufferEnd(void* context) override;

        void STDMETHODCALLTYPE OnLoopEnd(void*) override
        {
        }

        void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override
        {
        }

        IXAudio2SourceVoice* voice_ = nullptr;

        std::unique_ptr<IAudioDecoder> decoder_;

        WaveFormat format_{};

        std::vector<AudioBytes> buffers_;
        std::vector<BufferContext> contexts_;

        uint32_t framesPerBuffer_ = 16384;
        bool loop_ = false;

        std::atomic<bool> stopping_{false};
        std::atomic<bool> forcedStop_{false};
        std::atomic<bool> ended_{false};
    };
}
