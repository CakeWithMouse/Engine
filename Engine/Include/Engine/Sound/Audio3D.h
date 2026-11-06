#pragma once

#include <x3daudio.h>

namespace audio
{
    struct Vec3 final
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    inline X3DAUDIO_VECTOR ToX3DVector(const Vec3& v)
    {
        X3DAUDIO_VECTOR out{};
        out.x = v.x;
        out.y = v.y;
        out.z = v.z;
        return out;
    }

    struct AudioListener3D final
    {
        Vec3 position{};
        Vec3 velocity{};
        Vec3 front{0.0f, 0.0f, 1.0f};
        Vec3 top{0.0f, 1.0f, 0.0f};

        X3DAUDIO_LISTENER native{};

        void Commit()
        {
            native.Position = ToX3DVector(position);
            native.Velocity = ToX3DVector(velocity);
            native.OrientFront = ToX3DVector(front);
            native.OrientTop = ToX3DVector(top);
            native.pCone = nullptr;
        }
    };

    struct AudioEmitter3D final
    {
        Vec3 position{};
        Vec3 velocity{};
        Vec3 front{0.0f, 0.0f, 1.0f};
        Vec3 top{0.0f, 1.0f, 0.0f};

        float radius = 0.25f;

        X3DAUDIO_EMITTER native{};

        void Commit(uint32_t channelCount = 1)
        {
            ZeroMemory(&native, sizeof(native));

            native.Position = ToX3DVector(position);
            native.Velocity = ToX3DVector(velocity);
            native.OrientFront = ToX3DVector(front);
            native.OrientTop = ToX3DVector(top);

            native.ChannelCount = channelCount;
            native.ChannelRadius = radius;

            native.CurveDistanceScaler = 1.0f;
            native.DopplerScaler = 1.0f;
        }
    };
}
