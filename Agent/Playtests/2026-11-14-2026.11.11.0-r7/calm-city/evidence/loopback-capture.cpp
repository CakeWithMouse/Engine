#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"propsys.lib")
#pragma comment(lib,"uuid.lib")

using Microsoft::WRL::ComPtr;
namespace {
struct WavHeader { char riff[4]={'R','I','F','F'}; uint32_t size=0; char wave[4]={'W','A','V','E'}; char fmt[4]={'f','m','t',' '}; uint32_t fmtSize=16; uint16_t format=1,channels=1; uint32_t rate=0,byteRate=0; uint16_t align=2,bits=16; char data[4]={'d','a','t','a'}; uint32_t dataSize=0; };
std::wstring Utf8ToWide(const std::string& s) { if(s.empty()) return {}; int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0); std::wstring w(n? n-1:0,L'\0'); if(n>1) MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,w.data(),n); return w; }
std::string WideToUtf8(const wchar_t* s) { if(!s) return {}; int n=WideCharToMultiByte(CP_UTF8,0,s,-1,nullptr,0,nullptr,nullptr); std::string v(n? n-1:0,'\0'); if(n>1) WideCharToMultiByte(CP_UTF8,0,s,-1,v.data(),n,nullptr,nullptr); return v; }
std::string JsonEscape(const std::string& s) { std::string out; for(unsigned char c:s){if(c=='\\'||c=='"'){out+='\\';out+=static_cast<char>(c);}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else if(c>=0x20)out+=static_cast<char>(c);} return out; }
bool Arg(int argc,wchar_t** argv,const wchar_t* name,std::wstring& out) { for(int i=1;i+1<argc;++i) if(_wcsicmp(argv[i],name)==0){out=argv[i+1];return true;} return false; }
float Sample(const BYTE* p,const WAVEFORMATEX* f) {
    WORD tag=f->wFormatTag;
    if(tag==WAVE_FORMAT_EXTENSIBLE){auto* e=reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(f);tag=IsEqualGUID(e->SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)?WAVE_FORMAT_IEEE_FLOAT:IsEqualGUID(e->SubFormat,KSDATAFORMAT_SUBTYPE_PCM)?WAVE_FORMAT_PCM:0;}
    if(tag==WAVE_FORMAT_IEEE_FLOAT && f->wBitsPerSample==32){float v;std::memcpy(&v,p,4);return std::isfinite(v)?std::clamp(v,-1.0f,1.0f):0.0f;}
    if(tag!=WAVE_FORMAT_PCM)return 0.0f;
    if(f->wBitsPerSample==8)return (static_cast<int>(*p)-128)/128.0f;
    if(f->wBitsPerSample==16){int16_t v;std::memcpy(&v,p,2);return v/32768.0f;}
    if(f->wBitsPerSample==24){int32_t v=static_cast<int32_t>(p[0])|(static_cast<int32_t>(p[1])<<8)|(static_cast<int32_t>(p[2])<<16);if(v&0x800000)v|=~0xffffff;return v/8388608.0f;}
    if(f->wBitsPerSample==32){int32_t v;std::memcpy(&v,p,4);return v/2147483648.0f;}
    return 0.0f;
}
std::wstring FriendlyName(IMMDevice* d) { ComPtr<IPropertyStore> ps; if(FAILED(d->OpenPropertyStore(STGM_READ,&ps)))return L""; PROPVARIANT v; PropVariantInit(&v); std::wstring out; if(SUCCEEDED(ps->GetValue(PKEY_Device_FriendlyName,&v))&&v.vt==VT_LPWSTR)out=v.pwszVal; PropVariantClear(&v); return out; }
void Check(HRESULT hr,const char* what){if(FAILED(hr)){std::cerr<<what<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<"\n";throw hr;}}
}
int wmain(int argc,wchar_t** argv) {
    std::wstring secondsText,outPath,summaryPath,blockText;
    if(!Arg(argc,argv,L"--seconds",secondsText)||!Arg(argc,argv,L"--out",outPath)||!Arg(argc,argv,L"--summary",summaryPath)){std::wcerr<<L"Usage: LoopbackCapture.exe --seconds N --out capture.wav --summary summary.json [--block-ms 100]\n";return 2;}
    const double seconds=std::wcstod(secondsText.c_str(),nullptr); Arg(argc,argv,L"--block-ms",blockText); const int blockMs=blockText.empty()?100:_wtoi(blockText.c_str());
    if(seconds<0.1||seconds>120||blockMs<20||blockMs>1000){std::wcerr<<L"seconds must be 0.1..120 and block-ms 20..1000\n";return 2;}
    const HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED); if(FAILED(co)){Check(co,"CoInitializeEx");}
    try {
        ComPtr<IMMDeviceEnumerator> en; Check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&en)),"MMDeviceEnumerator");
        ComPtr<IMMDevice> dev; Check(en->GetDefaultAudioEndpoint(eRender,eConsole,&dev),"GetDefaultAudioEndpoint");
        LPWSTR idRaw=nullptr; Check(dev->GetId(&idRaw),"GetDeviceId"); std::wstring deviceId=idRaw?idRaw:L""; CoTaskMemFree(idRaw);
        const std::wstring deviceName=FriendlyName(dev.Get());
        ComPtr<IAudioClient> client; Check(dev->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client.GetAddressOf())),"Activate IAudioClient");
        WAVEFORMATEX* mix=nullptr; Check(client->GetMixFormat(&mix),"GetMixFormat");
        const auto freeMix=[&]{CoTaskMemFree(mix);};
        if(mix->nChannels<1||mix->nBlockAlign==0){freeMix();throw std::runtime_error("Unsupported endpoint format");}
        const uint32_t sampleRate=mix->nSamplesPerSec; const WORD endpointChannels=mix->nChannels;
        const WORD sampleBits=mix->wBitsPerSample; const WORD sampleTag=mix->wFormatTag;
        WORD effectiveTag=sampleTag;
        if(sampleTag==WAVE_FORMAT_EXTENSIBLE){auto* e=reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(mix);effectiveTag=IsEqualGUID(e->SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)?WAVE_FORMAT_IEEE_FLOAT:IsEqualGUID(e->SubFormat,KSDATAFORMAT_SUBTYPE_PCM)?WAVE_FORMAT_PCM:0;}
        if(!sampleRate||!((effectiveTag==WAVE_FORMAT_IEEE_FLOAT&&sampleBits==32)||(effectiveTag==WAVE_FORMAT_PCM&&(sampleBits==8||sampleBits==16||sampleBits==24||sampleBits==32)))){freeMix();throw std::runtime_error("Unsupported endpoint sample format");}
        HRESULT init=client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,static_cast<REFERENCE_TIME>(seconds*10000000.0)+2000000,0,mix,nullptr);
        if(FAILED(init)){freeMix();Check(init,"Initialize shared loopback");}
        ComPtr<IAudioCaptureClient> capture; Check(client->GetService(IID_PPV_ARGS(&capture)),"GetService IAudioCaptureClient");
        std::vector<int16_t> mono; LARGE_INTEGER qpcStart{},qpcFrequency{}; QueryPerformanceFrequency(&qpcFrequency); QueryPerformanceCounter(&qpcStart);
        const uint64_t qpcStart100ns=static_cast<uint64_t>((static_cast<long double>(qpcStart.QuadPart)*10000000.0L)/qpcFrequency.QuadPart);
        const auto start=std::chrono::steady_clock::now(); const auto stop=start+std::chrono::duration<double>(seconds);
        Check(client->Start(),"Start loopback");
        while(std::chrono::steady_clock::now()<stop){
            UINT32 packet=0; Check(capture->GetNextPacketSize(&packet),"GetNextPacketSize");
            if(!packet){std::this_thread::sleep_for(std::chrono::milliseconds(5));continue;}
            BYTE* data=nullptr; UINT32 frames=0; DWORD flags=0; UINT64 qpcPosition=0;
            Check(capture->GetBuffer(&data,&frames,&flags,nullptr,&qpcPosition),"GetBuffer");
            const unsigned bytesPerSample=mix->wBitsPerSample/8;
            size_t first=mono.size();
            if(qpcPosition>=qpcStart100ns){first=static_cast<size_t>((static_cast<long double>(qpcPosition-qpcStart100ns)*sampleRate)/10000000.0L);}
            const size_t end=first+frames;if(mono.size()<end)mono.resize(end,0);
            for(UINT32 i=0;i<frames;++i){float sum=0; if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT))for(WORD c=0;c<mix->nChannels;++c)sum+=Sample(data+(static_cast<size_t>(i)*mix->nChannels+c)*bytesPerSample,mix); const float v=std::clamp(sum/static_cast<float>(mix->nChannels),-1.0f,1.0f);const size_t at=first+i;if(at<mono.size())mono[at]=static_cast<int16_t>(std::lrint(v*32767.0f));}
            Check(capture->ReleaseBuffer(frames),"ReleaseBuffer");
        }
        client->Stop(); freeMix();
        // WASAPI may deliver no packets while idle or stop delivery after the last voice.
        // QPC placement keeps active packets at their capture times; pad silence to requested duration.
        const size_t requestedSamples=static_cast<size_t>(std::llround(seconds*sampleRate));
        if(mono.size()<requestedSamples)mono.resize(requestedSamples,0);else if(mono.size()>requestedSamples)mono.resize(requestedSamples);
        WavHeader h; h.rate=sampleRate;
        h.channels=1; h.byteRate=h.rate*2; h.align=2;
        // This helper intentionally stores capture samples in a mono 16-bit WAV at endpoint rate.
        std::ofstream wav(outPath,std::ios::binary); if(!wav)throw std::runtime_error("Cannot create output WAV"); h.dataSize=static_cast<uint32_t>(mono.size()*sizeof(int16_t)); h.size=36+h.dataSize; wav.write(reinterpret_cast<const char*>(&h),sizeof(h)); wav.write(reinterpret_cast<const char*>(mono.data()),h.dataSize); wav.close();
        const size_t window=std::max<size_t>(1,static_cast<size_t>(h.rate*blockMs/1000));
        std::ofstream json(summaryPath); if(!json)throw std::runtime_error("Cannot create summary JSON"); json<<"{\n  \"deviceId\":\""<<JsonEscape(WideToUtf8(deviceId.c_str()))<<"\",\n  \"deviceName\":\""<<JsonEscape(WideToUtf8(deviceName.c_str()))<<"\",\n  \"captureStartQpc100ns\":"<<qpcStart100ns<<",\n  \"sampleRate\":"<<h.rate<<",\n  \"endpointChannels\":"<<endpointChannels<<",\n  \"outputChannels\":1,\n  \"endpointFormatTag\":"<<sampleTag<<",\n  \"endpointBitsPerSample\":"<<sampleBits<<",\n  \"capturedSamples\":"<<mono.size()<<",\n  \"durationSeconds\":"<<static_cast<double>(mono.size())/h.rate<<",\n  \"windows\":[\n";
        for(size_t begin=0,index=0;begin<mono.size();begin+=window,++index){const size_t end=std::min(mono.size(),begin+window);long double squares=0;int peak=0;for(size_t i=begin;i<end;++i){const int x=mono[i];squares+=static_cast<long double>(x)*x;peak=std::max(peak,std::abs(x));}const double rms=end>begin?std::sqrt(static_cast<double>(squares/(end-begin)))/32768.0:0;json<<"    {\"index\":"<<index<<",\"startSeconds\":"<<static_cast<double>(begin)/h.rate<<",\"endSeconds\":"<<static_cast<double>(end)/h.rate<<",\"rms\":"<<rms<<",\"peak\":"<<peak/32768.0<<"}"<<(end<mono.size()?",":"")<<"\n";}
        json<<"  ]\n}\n"; json.close(); std::cout<<"capture OK samples="<<mono.size()<<" rate="<<h.rate<<" device="<<WideToUtf8(deviceName.c_str())<<"\n"; CoUninitialize(); return 0;
    } catch(HRESULT){CoUninitialize();return 1;} catch(const std::exception& e){std::cerr<<e.what()<<"\n";CoUninitialize();return 1;}
}
