#ifndef LOCAL_STB_IMAGE_COMPAT_H
#define LOCAL_STB_IMAGE_COMPAT_H

#include <cstdlib>

extern "C" {
unsigned char* stbi_load(char const* filename, int* x, int* y, int* comp, int req_comp);
void stbi_image_free(void* retval_from_stbi_load);
void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip);
}

#ifdef STB_IMAGE_IMPLEMENTATION

#include <algorithm>
#include <cstring>
#include <vector>
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "windowscodecs.lib")

namespace local_stbi
{
    inline bool& FlipOnLoad()
    {
        static bool flip = false;
        return flip;
    }

    inline IWICImagingFactory* GetFactory()
    {
        static Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        if (!factory)
        {
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
        }
        return factory.Get();
    }
}

extern "C" void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip)
{
    local_stbi::FlipOnLoad() = flag_true_if_should_flip != 0;
}

extern "C" void stbi_image_free(void* retval_from_stbi_load)
{
    std::free(retval_from_stbi_load);
}

extern "C" unsigned char* stbi_load(char const* filename, int* x, int* y, int* comp, int req_comp)
{
    if (!filename || !x || !y)
    {
        return nullptr;
    }

    IWICImagingFactory* factory = local_stbi::GetFactory();
    if (!factory)
    {
        return nullptr;
    }

    int wideLength = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
    UINT codePage = CP_UTF8;
    if (wideLength == 0)
    {
        codePage = CP_ACP;
        wideLength = MultiByteToWideChar(codePage, 0, filename, -1, nullptr, 0);
    }
    if (wideLength == 0)
    {
        return nullptr;
    }

    std::vector<wchar_t> widePath(static_cast<size_t>(wideLength));
    if (MultiByteToWideChar(codePage, 0, filename, -1, widePath.data(), wideLength) == 0)
    {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(
        widePath.data(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr))
    {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
    {
        return nullptr;
    }

    UINT width = 0;
    UINT height = 0;
    hr = frame->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0)
    {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr))
    {
        return nullptr;
    }

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
    {
        return nullptr;
    }

    int channels = req_comp > 0 ? req_comp : 4;
    if (channels < 1 || channels > 4)
    {
        return nullptr;
    }

    const size_t rgbaStride = static_cast<size_t>(width) * 4;
    const size_t rgbaSize = rgbaStride * static_cast<size_t>(height);
    std::vector<unsigned char> rgba(rgbaSize);
    hr = converter->CopyPixels(nullptr, static_cast<UINT>(rgbaStride), static_cast<UINT>(rgbaSize), rgba.data());
    if (FAILED(hr))
    {
        return nullptr;
    }

    const size_t outStride = static_cast<size_t>(width) * static_cast<size_t>(channels);
    unsigned char* output = static_cast<unsigned char*>(std::malloc(outStride * static_cast<size_t>(height)));
    if (!output)
    {
        return nullptr;
    }

    for (UINT row = 0; row < height; ++row)
    {
        UINT sourceRow = local_stbi::FlipOnLoad() ? height - 1 - row : row;
        const unsigned char* src = rgba.data() + static_cast<size_t>(sourceRow) * rgbaStride;
        unsigned char* dst = output + static_cast<size_t>(row) * outStride;
        for (UINT col = 0; col < width; ++col)
        {
            const unsigned char* pixel = src + static_cast<size_t>(col) * 4;
            unsigned char* outPixel = dst + static_cast<size_t>(col) * static_cast<size_t>(channels);
            if (channels == 1)
            {
                outPixel[0] = static_cast<unsigned char>((static_cast<unsigned int>(pixel[0]) + pixel[1] + pixel[2]) / 3);
            }
            else if (channels == 2)
            {
                outPixel[0] = static_cast<unsigned char>((static_cast<unsigned int>(pixel[0]) + pixel[1] + pixel[2]) / 3);
                outPixel[1] = pixel[3];
            }
            else
            {
                std::memcpy(outPixel, pixel, static_cast<size_t>(channels));
            }
        }
    }

    *x = static_cast<int>(width);
    *y = static_cast<int>(height);
    if (comp)
    {
        *comp = 4;
    }
    return output;
}

#endif

#endif
