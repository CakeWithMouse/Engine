#pragma once

#include "AudioTypes.h"

#include <xaudio2.h>
#include <wrl/client.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace audio {

    struct PooledVoice;

    class VoiceCallback final : public IXAudio2VoiceCallback {
    public:
        PooledVoice* owner = nullptr;

        void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
        void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}

        void STDMETHODCALLTYPE OnStreamEnd() override;

        void STDMETHODCALLTYPE OnBufferStart(void*) override {}
        void STDMETHODCALLTYPE OnBufferEnd(void*) override {}
        void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
        void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override {}
    };

    struct PooledVoice final {
        IXAudio2SourceVoice* voice = nullptr;
        std::unique_ptr<VoiceCallback> callback;

        WaveFormatKey key{};

        std::atomic<bool> ended{false};
        std::atomic<bool> active{false};

        std::vector<float> outputLevels;
    };

    class VoicePool final {
    public:
        void Initialize(
            IXAudio2* xaudio,
            IXAudio2MasteringVoice* masterVoice
        );

        void Shutdown();

        void Update();

        PooledVoice* Acquire(const WAVEFORMATEX* format);
        void ReleaseAllActive();

    private:
        void MoveEndedVoicesToFreeList();

        IXAudio2* xaudio_ = nullptr;
        IXAudio2MasteringVoice* masterVoice_ = nullptr;

        std::vector<std::unique_ptr<PooledVoice>> allVoices_;
        std::vector<PooledVoice*> activeVoices_;
        std::vector<PooledVoice*> freeVoices_;

        std::mutex mutex_;
    };

}