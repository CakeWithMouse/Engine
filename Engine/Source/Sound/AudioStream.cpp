#include "Engine/Sound/AudioStream.h"

namespace audio
{
    AudioStream::~AudioStream()
    {
        if (voice_)
        {
            voice_->Stop(0);
            voice_->FlushSourceBuffers();
            voice_->DestroyVoice();
            voice_ = nullptr;
        }
    }

    bool AudioStream::Start(
        IXAudio2* xaudio,
        std::unique_ptr<IAudioDecoder> decoder,
        const Options& options
    )
    {
        if (!xaudio || !decoder)
        {
            return false;
        }

        decoder_ = std::move(decoder);
        format_ = decoder_->GetFormat();

        loop_ = options.loop;
        framesPerBuffer_ = options.framesPerBuffer;

        if (framesPerBuffer_ == 0)
        {
            framesPerBuffer_ = 16384;
        }

        if (format_.blockAlign == 0)
        {
            return false;
        }

        HRESULT hr = xaudio->CreateSourceVoice(
            &voice_,
            format_.Get(),
            0,
            XAUDIO2_DEFAULT_FREQ_RATIO,
            this,
            nullptr,
            nullptr
        );

        if (FAILED(hr) || !voice_)
        {
            return false;
        }

        voice_->SetVolume(options.volume);

        constexpr int BufferCount = 3;

        buffers_.resize(BufferCount);
        contexts_.resize(BufferCount);

        for (int i = 0; i < BufferCount; ++i)
        {
            buffers_[i].resize(
                static_cast<std::size_t>(framesPerBuffer_) * format_.blockAlign
            );

            contexts_[i].owner = this;
            contexts_[i].index = i;

            RefillAndSubmit(i);
        }

        hr = voice_->Start(0);

        if (FAILED(hr))
        {
            voice_->DestroyVoice();
            voice_ = nullptr;
            return false;
        }

        return true;
    }

    void AudioStream::RequestStop()
    {
        forcedStop_.store(true);
        stopping_.store(true);

        if (voice_)
        {
            voice_->Stop(0);
            voice_->FlushSourceBuffers();
        }
    }

    void AudioStream::OnStreamEnd()
    {
        ended_.store(true);
    }

    void AudioStream::OnBufferEnd(void* context)
    {
        if (!context)
        {
            return;
        }

        auto* ctx = static_cast<BufferContext*>(context);

        if (ctx->owner)
        {
            ctx->owner->HandleBufferEnd(ctx->index);
        }
    }

    void AudioStream::HandleBufferEnd(int bufferIndex)
    {
        if (stopping_.load())
        {
            return;
        }

        RefillAndSubmit(bufferIndex);
    }

    void AudioStream::RefillAndSubmit(int bufferIndex)
    {
        if (!voice_ || !decoder_)
        {
            return;
        }

        if (stopping_.load())
        {
            return;
        }

        AudioBytes& buffer = buffers_[bufferIndex];

        uint64_t framesRead = decoder_->ReadFrames(
            buffer.data(),
            framesPerBuffer_
        );

        if (framesRead == 0 && loop_)
        {
            decoder_->SeekFrames(0);
            framesRead = decoder_->ReadFrames(
                buffer.data(),
                framesPerBuffer_
            );
        }

        if (framesRead == 0)
        {
            return;
        }

        XAUDIO2_BUFFER xaudioBuffer{};

        xaudioBuffer.pAudioData = buffer.data();
        xaudioBuffer.AudioBytes =
            static_cast<UINT32>(framesRead * format_.blockAlign);

        xaudioBuffer.pContext = &contexts_[bufferIndex];

        if (!loop_ && framesRead < framesPerBuffer_)
        {
            xaudioBuffer.Flags = XAUDIO2_END_OF_STREAM;
        }

        voice_->SubmitSourceBuffer(&xaudioBuffer);
    }
}
