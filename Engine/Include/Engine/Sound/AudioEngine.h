#pragma once

#include "AudioTypes.h"
#include "Audio3D.h"
#include "IAudioDecoder.h"
#include "VoicePool.h"
#include "AudioStream.h"

#include <xaudio2.h>
#include <x3daudio.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "x3daudio.lib")

namespace audio
{
    class AudioEngine final
    {
    public:
        struct SoundId final
        {
            uint32_t value = 0;

            bool IsValid() const noexcept
            {
                return value != 0;
            }
        };

        struct StreamHandle final
        {
            uint32_t value = 0;

            bool IsValid() const noexcept
            {
                return value != 0;
            }
        };

        struct PlayOptions final
        {
            float volume = 1.0f;
            float pitchSemitones = 0.0f;
            bool loop = false;
        };

        struct StreamOptions final
        {
            bool loop = false;
            float volume = 1.0f;
            uint32_t framesPerBuffer = 16384;
        };

        AudioEngine() = default;

        ~AudioEngine()
        {
            Shutdown();
        }

        bool Initialize();
        void Shutdown();
        void Update();

        SoundId LoadSound(const std::wstring& filePath);

        bool Play2D(SoundId id, const PlayOptions& options = {});

        bool Play3D(
            SoundId id,
            const AudioEmitter3D& emitter,
            const PlayOptions& options = {}
        );

        StreamHandle PlayStream(
            const std::wstring& filePath,
            const StreamOptions& options = {}
        );

        void StopStream(StreamHandle handle);

        void SetListener(const AudioListener3D& listener)
        {
            listener_ = listener;
        }

        void StopAll();

    private:
        struct SoundAsset final
        {
            AudioBytes audioData;
            WaveFormat format;
            XAUDIO2_BUFFER buffer{};
        };

        struct Active3DSound final
        {
            PooledVoice* voice = nullptr;
            AudioEmitter3D emitter;
            float basePitchRatio = 1.0f;
        };

        bool InitializeX3DAudio();

        void Update3D();
        void UpdateStreams();

        void Apply3D(Active3DSound& sound);

        static float PitchSemitonesToRatio(float semitones);

        Microsoft::WRL::ComPtr<IXAudio2> xaudio_;
        IXAudio2MasteringVoice* masterVoice_ = nullptr;

        VoicePool voicePool_;

        X3DAUDIO_HANDLE x3dInstance_{};
        uint32_t outputChannels_ = 2;

        AudioListener3D listener_;

        std::unordered_map<uint32_t, std::unique_ptr<SoundAsset>> sounds_;
        uint32_t nextSoundId_ = 1;

        std::unordered_map<uint32_t, std::unique_ptr<AudioStream>> streams_;
        uint32_t nextStreamId_ = 1;

        std::vector<Active3DSound> active3DSounds_;

        bool ownsCom_ = false;
    };
}
