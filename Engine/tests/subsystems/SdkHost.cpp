#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Engine/Runtime/EditSessionApi.h>
#include <iostream>
#include <filesystem>
#include <stdexcept>
void Check(bool ok,const char* text) { if(!ok) throw std::runtime_error(text); }
int wmain(int argc,wchar_t** argv)
{
    try {
        Check(argc==3,"DLL and content arguments");
        const auto modulePath=std::filesystem::path(argv[1]).make_preferred();
        const auto content=std::filesystem::path(argv[2]).u8string();
        for(int reload=0;reload<2;++reload) {
            HMODULE dll=LoadLibraryExW(modulePath.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH); Check(dll!=nullptr,"LoadLibrary");
            auto get=reinterpret_cast<EngineGetEditModuleApiFn>(GetProcAddress(dll,"EngineGetEditModuleApi"));
            auto probe=reinterpret_cast<int(*)(int)>(GetProcAddress(dll,"ProbeSubsystem"));
            Check(get&&probe,"exports");
            Check(probe(0)==0 && probe(1)==0,"fresh module state after reload");
            EngineEditModuleApi factory{};EngineEditSessionApi api{};
            Check(get(ENGINE_EDIT_API_VERSION,sizeof(factory),&factory)!=0,"factory ABI");
            Check(factory.create(content.c_str(),&api)!=0,"create edit");
            Check(probe(0)==0,"edit has no subsystem");
            probe(2);
            Check(api.control(api.context,EnginePlay)==EngineSceneFailure && probe(0)==0,"failed Play rollback in DLL");
            probe(3);
            Check(api.control(api.context,EnginePlay)==EngineSceneOk && probe(0)==1,"one DLL instance");
            Check(api.frame(api.context,.02)==EngineSceneOk && probe(1)==1,"DLL tick");
            api.control(api.context,EnginePause);api.frame(api.context,.02);
            Check(probe(0)==1 && probe(1)==1,"pause preserves instance");
            api.control(api.context,EngineStop);
            Check(probe(0)==0,"Stop destroys before unload");
            api.control(api.context,EnginePlay);
            Check(probe(0)==1,"replay");
            factory.destroy(api.context);
            Check(probe(0)==0,"destroy while Playing cleans before FreeLibrary");
            Check(FreeLibrary(dll)!=0,"FreeLibrary");
        }
        std::cout<<"External SDK DLL: lifecycle, rollback and unload/reload passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
