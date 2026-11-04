#include "Engine/Sound/AudioEngine.h"
#include "Engine/Sound/DecoderFactory.h"

#include <cmath>

namespace audio
{
    float AudioEngine::PitchSemitonesToRatio(float semitones)
    {
        if (!std::isfinite(semitones))
        {
            return 1.0f;
        }

        float ratio = std::pow(2.0f, semitones / 12.0f);

        if (!std::isfinite(ratio))
        {
            return 1.0f;
        }

        if (ratio < 0.25f) ratio = 0.25f;
        if (ratio > 4.00f) ratio = 4.00f;

        return ratio;
    }

    bool AudioEngine::Initialize()
    {
        if (xaudio_)
        {
            return true;
        }

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        {
            return false;
        }

        ownsCom_ = SUCCEEDED(hr);

        hr = XAudio2Create(
            xaudio_.GetAddressOf(),
            0,
            XAUDIO2_DEFAULT_PROCESSOR
        );

        if (FAILED(hr))
        {
            if (ownsCom_)
            {
                CoUninitialize();
                ownsCom_ = false;
            }

            return false;
        }

        hr = xaudio_->CreateMasteringVoice(&masterVoice_);

        if (FAILED(hr))
        {
            xaudio_.Reset();

            if (ownsCom_)
            {
                CoUninitialize();
                ownsCom_ = false;
            }

            return false;
        }

        XAUDIO2_VOICE_DETAILS details{};
        masterVoice_->GetVoiceDetails(&details);
        outputChannels_ = details.InputChannels;

        if (!InitializeX3DAudio())
        {
            Shutdown();
            return false;
        }

        voicePool_.Initialize(xaudio_.Get(), masterVoice_);

        return true;
    }

    bool AudioEngine::InitializeX3DAudio()
    {
        DWORD channelMask = SPEAKER_STEREO;

        // Если вдруг в твоем SDK нет этого метода,
        // можно временно использовать SPEAKER_STEREO.
        masterVoice_->GetChannelMask(&channelMask);

        HRESULT hr = X3DAudioInitialize(
            channelMask,
            343.5f,
            x3dInstance_
        );

        return SUCCEEDED(hr);
    }

    void AudioEngine::Shutdown()
    {
        StopAll();

        voicePool_.Shutdown();

        sounds_.clear();
        streams_.clear();
        active3DSounds_.clear();

        if (masterVoice_)
        {
            masterVoice_->DestroyVoice();
            masterVoice_ = nullptr;
        }

        xaudio_.Reset();

        if (ownsCom_)
        {
            CoUninitialize();
            ownsCom_ = false;
        }
    }

    void AudioEngine::Update()
    {
        Update3D();
        voicePool_.Update();
        UpdateStreams();
    }

    AudioEngine::SoundId AudioEngine::LoadSound(const std::wstring& filePath)
    {
        auto decoder = DecoderFactory::Create(filePath);

        if (!decoder)
        {
            return {};
        }

        if (!decoder->Open(filePath))
        {
            return {};
        }

        const WaveFormat format = decoder->GetFormat();

        if (format.blockAlign == 0)
        {
            return {};
        }

        AudioBytes data;

        uint64_t lengthFrames = decoder->GetLengthFrames();

        if (lengthFrames > 0)
        {
            data.resize(lengthFrames * format.blockAlign);

            uint64_t readFrames = decoder->ReadFrames(
                data.data(),
                lengthFrames
            );

            data.resize(readFrames * format.blockAlign);
        }
        else
        {
            AudioBytes temp;
            temp.resize(static_cast<std::size_t>(16384) * format.blockAlign);

            while (true)
            {
                uint64_t readFrames = decoder->ReadFrames(temp.data(), 16384);

                if (readFrames == 0)
                {
                    break;
                }

                std::size_t oldSize = data.size();
                std::size_t addBytes =
                    readFrames * format.blockAlign;

                data.resize(oldSize + addBytes);

                memcpy(
                    data.data() + oldSize,
                    temp.data(),
                    addBytes
                );
            }
        }

        if (data.empty())
        {
            return {};
        }

        auto asset = std::make_unique<SoundAsset>();

        asset->audioData = std::move(data);
        asset->format = format;

        asset->buffer = {};
        asset->buffer.pAudioData = asset->audioData.data();
        asset->buffer.AudioBytes =
            static_cast<UINT32>(asset->audioData.size());
        asset->buffer.Flags = XAUDIO2_END_OF_STREAM;

        uint32_t id = nextSoundId_++;

        sounds_.emplace(id, std::move(asset));

        return SoundId{id};
    }

    bool AudioEngine::Play2D(SoundId id, const PlayOptions& options)
    {
        auto it = sounds_.find(id.value);

        if (it == sounds_.end())
        {
            return false;
        }

        SoundAsset* asset = it->second.get();

        PooledVoice* voice = voicePool_.Acquire(asset->format.Get());

        if (!voice)
        {
            return false;
        }

        XAUDIO2_BUFFER buffer = asset->buffer;

        if (options.loop)
        {
            buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
            buffer.LoopBegin = 0;

            if (asset->format.blockAlign != 0)
            {
                buffer.LoopLength = static_cast<UINT32>(
                    asset->audioData.size() / asset->format.blockAlign
                );
            }
        }
        else
        {
            buffer.LoopCount = 0;
            buffer.LoopBegin = 0;
            buffer.LoopLength = 0;
        }

        HRESULT hr = voice->voice->SubmitSourceBuffer(&buffer);

        if (FAILED(hr))
        {
            return false;
        }

        voice->voice->SetVolume(options.volume);

        float ratio = PitchSemitonesToRatio(options.pitchSemitones);
        voice->voice->SetFrequencyRatio(ratio);

        hr = voice->voice->Start(0);

        return SUCCEEDED(hr);
    }

    bool AudioEngine::Play3D(
        SoundId id,
        const AudioEmitter3D& emitter,
        const PlayOptions& options
    )
    {
        auto it = sounds_.find(id.value);

        if (it == sounds_.end())
        {
            return false;
        }

        SoundAsset* asset = it->second.get();

        PooledVoice* voice = voicePool_.Acquire(asset->format.Get());

        if (!voice)
        {
            return false;
        }

        XAUDIO2_BUFFER buffer = asset->buffer;

        if (options.loop)
        {
            buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
            buffer.LoopBegin = 0;

            if (asset->format.blockAlign != 0)
            {
                buffer.LoopLength = static_cast<UINT32>(
                    asset->audioData.size() / asset->format.blockAlign
                );
            }
        }
        else
        {
            buffer.LoopCount = 0;
            buffer.LoopBegin = 0;
            buffer.LoopLength = 0;
        }

        HRESULT hr = voice->voice->SubmitSourceBuffer(&buffer);

        if (FAILED(hr))
        {
            return false;
        }

        voice->voice->SetVolume(options.volume);

        float basePitch = PitchSemitonesToRatio(options.pitchSemitones);

        voice->voice->SetFrequencyRatio(basePitch);

        Active3DSound active{};
        active.voice = voice;
        active.emitter = emitter;
        active.basePitchRatio = basePitch;

        active3DSounds_.push_back(active);

        hr = voice->voice->Start(0);

        return SUCCEEDED(hr);
    }

    AudioEngine::StreamHandle AudioEngine::PlayStream(
        const std::wstring& filePath,
        const StreamOptions& options
    )
    {
        auto decoder = DecoderFactory::Create(filePath);

        if (!decoder)
        {
            return {};
        }

        if (!decoder->Open(filePath))
        {
            return {};
        }

        auto stream = std::make_unique<AudioStream>();

        AudioStream::Options streamOptions{};
        streamOptions.loop = options.loop;
        streamOptions.volume = options.volume;
        streamOptions.framesPerBuffer = options.framesPerBuffer;

        if (!stream->Start(xaudio_.Get(), std::move(decoder), streamOptions))
        {
            return {};
        }

        uint32_t id = nextStreamId_++;

        streams_.emplace(id, std::move(stream));

        return StreamHandle{id};
    }

    void AudioEngine::StopStream(StreamHandle handle)
    {
        auto it = streams_.find(handle.value);

        if (it == streams_.end())
        {
            return;
        }

        it->second->RequestStop();
    }

    void AudioEngine::StopAll()
    {
        for (auto& pair : streams_)
        {
            pair.second->RequestStop();
        }

        streams_.clear();
        active3DSounds_.clear();

        voicePool_.ReleaseAllActive();
    }

    void AudioEngine::UpdateStreams()
    {
        for (auto it = streams_.begin(); it != streams_.end();)
        {
            if (it->second->IsFinished())
            {
                it = streams_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void AudioEngine::Update3D()
    {
        listener_.Commit();

        for (auto it = active3DSounds_.begin(); it != active3DSounds_.end();)
        {
            Active3DSound& sound = *it;

            if (!sound.voice || sound.voice->ended.load())
            {
                it = active3DSounds_.erase(it);
                continue;
            }

            Apply3D(sound);

            ++it;
        }
    }

    void AudioEngine::Apply3D(Active3DSound& sound)
    {
        if (!sound.voice || !sound.voice->voice)
        {
            return;
        }

        uint32_t srcChannels = sound.voice->key.channels;
        uint32_t dstChannels = outputChannels_;

        if (srcChannels == 0 || dstChannels == 0)
        {
            return;
        }

        std::size_t levelsSize =
            static_cast<std::size_t>(srcChannels) *
            static_cast<std::size_t>(dstChannels);

        if (sound.voice->outputLevels.size() != levelsSize)
        {
            sound.voice->outputLevels.resize(levelsSize);
        }

        sound.emitter.Commit(srcChannels);

        X3DAUDIO_DSP_SETTINGS dsp{};

        dsp.SrcChannelCount = srcChannels;
        dsp.DstChannelCount = dstChannels;
        dsp.pMatrixCoefficients = sound.voice->outputLevels.data();

        X3DAudioCalculate(
            x3dInstance_,
            &listener_.native,
            &sound.emitter.native,
            X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER,
            &dsp
        );

        sound.voice->voice->SetOutputMatrix(
            masterVoice_,
            srcChannels,
            dstChannels,
            sound.voice->outputLevels.data(),
            XAUDIO2_COMMIT_NOW
        );

        float ratio = sound.basePitchRatio * dsp.DopplerFactor;

        if (!std::isfinite(ratio))
        {
            ratio = 1.0f;
        }

        if (ratio < 0.25f) ratio = 0.25f;
        if (ratio > 4.00f) ratio = 4.00f;

        sound.voice->voice->SetFrequencyRatio(
            ratio,
            XAUDIO2_COMMIT_NOW
        );
    }
}
