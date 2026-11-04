#include "Engine/Sound/VoicePool.h"

namespace audio
{
    void VoiceCallback::OnStreamEnd()
    {
        if (owner)
        {
            owner->ended.store(true);
        }
    }

    void VoicePool::Initialize(
        IXAudio2* xaudio,
        IXAudio2MasteringVoice* masterVoice
    )
    {
        xaudio_ = xaudio;
        masterVoice_ = masterVoice;
    }

    void VoicePool::Shutdown()
    {
        ReleaseAllActive();

        for (auto& pooled : allVoices_)
        {
            if (pooled->voice)
            {
                pooled->voice->Stop(0);
                pooled->voice->FlushSourceBuffers();
                pooled->voice->DestroyVoice();
                pooled->voice = nullptr;
            }
        }

        allVoices_.clear();
        activeVoices_.clear();
        freeVoices_.clear();
    }

    void VoicePool::Update()
    {
        MoveEndedVoicesToFreeList();
    }

    PooledVoice* VoicePool::Acquire(const WAVEFORMATEX* format)
    {
        if (!xaudio_)
        {
            return nullptr;
        }

        std::lock_guard lock(mutex_);

        WaveFormatKey key = MakeWaveFormatKey(format);

        for (auto it = freeVoices_.begin(); it != freeVoices_.end(); ++it)
        {
            PooledVoice* candidate = *it;

            if (candidate->key == key)
            {
                freeVoices_.erase(it);

                candidate->ended.store(false);
                candidate->active.store(true);

                activeVoices_.push_back(candidate);

                return candidate;
            }
        }

        auto pooled = std::make_unique<PooledVoice>();
        auto callback = std::make_unique<VoiceCallback>();

        callback->owner = pooled.get();

        IXAudio2SourceVoice* voice = nullptr;

        HRESULT hr = xaudio_->CreateSourceVoice(
            &voice,
            format,
            0,
            4.0f,
            callback.get(),
            nullptr,
            nullptr
        );

        if (FAILED(hr) || !voice)
        {
            return nullptr;
        }

        pooled->voice = voice;
        pooled->callback = std::move(callback);
        pooled->key = key;
        pooled->ended.store(false);
        pooled->active.store(true);

        PooledVoice* raw = pooled.get();

        allVoices_.push_back(std::move(pooled));
        activeVoices_.push_back(raw);

        return raw;
    }

    void VoicePool::ReleaseAllActive()
    {
        std::lock_guard lock(mutex_);

        for (PooledVoice* voice : activeVoices_)
        {
            if (!voice) continue;

            if (voice->voice)
            {
                voice->voice->Stop(0);
                voice->voice->FlushSourceBuffers();
            }

            voice->ended.store(false);
            voice->active.store(false);

            freeVoices_.push_back(voice);
        }

        activeVoices_.clear();
    }

    void VoicePool::MoveEndedVoicesToFreeList()
    {
        std::lock_guard lock(mutex_);

        for (std::size_t i = 0; i < activeVoices_.size();)
        {
            PooledVoice* voice = activeVoices_[i];

            if (voice->ended.load())
            {
                voice->voice->Stop(0);
                voice->voice->FlushSourceBuffers();

                voice->ended.store(false);
                voice->active.store(false);

                activeVoices_.erase(activeVoices_.begin() + i);
                freeVoices_.push_back(voice);
            }
            else
            {
                ++i;
            }
        }
    }
}
