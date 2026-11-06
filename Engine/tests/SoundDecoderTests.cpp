#include <Engine/Sound/WavDecoder.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
struct TemporaryWave
{
    std::filesystem::path path;
    TemporaryWave()
    {
        wchar_t directory[MAX_PATH]{}, name[MAX_PATH]{};
        Require(GetTempPathW(MAX_PATH, directory) != 0 && GetTempFileNameW(directory, L"wav", 0, name) != 0,
            "Cannot create WAV fixture");
        path = name;
    }
    ~TemporaryWave() { std::error_code error; std::filesystem::remove(path, error); }
};
}

void SoundDecoderTests()
{
    TemporaryWave fixture;
    // Mono PCM16, with an odd-sized unknown chunk to exercise RIFF padding.
    const unsigned char wave[] = {
        'R','I','F','F', 52,0,0,0, 'W','A','V','E',
        'J','U','N','K', 1,0,0,0, 42,0,
        'f','m','t',' ', 16,0,0,0, 1,0, 1,0, 0x40,0x1f,0,0,
        0x80,0x3e,0,0, 2,0, 16,0,
        'd','a','t','a', 6,0,0,0, 1,0, 0xfe,0xff, 3,0
    };
    { std::ofstream out(fixture.path, std::ios::binary); out.write(reinterpret_cast<const char*>(wave), sizeof(wave)); }
    {
        audio::WavDecoder decoder;
        Require(decoder.Open(fixture.path.wstring()), "WAV open failed");
        const auto& format = decoder.GetFormat();
        Require(format.channels == 1 && format.sampleRate == 8000 && format.blockAlign == 2,
            "WAV format mismatch");
        Require(decoder.GetLengthFrames() == 3, "WAV length mismatch");
        std::array<int16_t, 4> samples{};
        Require(decoder.ReadFrames(samples.data(), 4) == 3 && samples[0] == 1 && samples[1] == -2 && samples[2] == 3,
            "WAV samples mismatch");
        Require(decoder.ReadFrames(samples.data(), 1) == 0, "WAV must stop at data end");
        Require(decoder.SeekFrames(1) && decoder.ReadFrames(samples.data(), 1) == 1 && samples[0] == -2,
            "WAV seek mismatch");
        Require(decoder.SeekFrames(100) && decoder.ReadFrames(samples.data(), 1) == 0, "WAV seek must clamp to end");
    }
    { std::ofstream out(fixture.path, std::ios::binary | std::ios::trunc); out << "not a wave"; }
    { audio::WavDecoder decoder; Require(!decoder.Open(fixture.path.wstring()), "Invalid RIFF accepted"); }

    SceneEditSession session({});
    const auto api = session.GetAudioPreviewApi();
    Require(api.version == 1 && api.context && api.play && api.stop, "Audio preview table incomplete");
    char error[128]{};
    Require(api.play(nullptr, "/Game/test.wav", error, sizeof(error)) == EngineSceneInvalidArgument,
        "Audio preview accepted null context");
    for (const char* path : {"", "C:/test.wav", "/Engine/test.wav", "/Game/../test.wav", "/Game/test.txt"})
        Require(api.play(api.context, path, error, sizeof(error)) == EngineSceneInvalidArgument,
            "Audio preview accepted invalid path");
    Require(api.play(api.context, "/Game/test.wav", nullptr, 1) == EngineSceneInvalidArgument,
        "Audio preview accepted null error buffer");
    Require(api.stop(api.context) == EngineSceneOk, "Stopping idle preview failed");
    uint32_t status = EngineSceneOk;
    std::thread worker([&] { status = api.stop(api.context); });
    worker.join();
    Require(status == EngineSceneWrongThread, "Audio preview accepted wrong thread");
    const auto edit = session.GetApi();
    Require(edit.control(edit.context, EnginePlay) == EngineSceneOk, "Cannot start headless Play");
    Require(api.play(api.context, "/Game/test.wav", error, sizeof(error)) == EngineSceneReadOnly,
        "Audio preview allowed during Play");
    Require(edit.control(edit.context, EngineStop) == EngineSceneOk, "Cannot stop headless Play");
}
