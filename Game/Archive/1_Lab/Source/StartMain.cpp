// MySuper3DApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "Public/FirstSemestrLegasy.h"
#include "Public/Base/Base.h"
#include "Public/Base/Config/BaseResources.h"
#include "Public/Base/Config/BaseGameConfig.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

#define UseLegasyDebug 0


int main()
{
    BaseGameConfig Config;
    BaseResources Resource;
    if (!Resource.Initialize())
    {
        std::cerr << "Resource initialization failed." << std::endl;
        return 1;
    }
    const std::filesystem::path projectDirectory = Resource.FindProjectDirectory();
    if (projectDirectory.empty())
    {
        std::cerr << "Cannot find the project directory (Source/Shaders). "
                     "Run from 1_Lab or set ENGINE_PROJECT_DIR." << std::endl;
        return 1;
    }
    std::error_code error;
    std::filesystem::current_path(projectDirectory, error);
    if (error)
    {
        std::cerr << "Cannot enter the project directory: " << error.message() << std::endl;
        return 1;
    }

    const std::string ContentRoot = Resource.ResolveContentRoot();
    
#if UseLegasyDebug
    FirstSemestrLegasy LegasyCode(Resource, Config);
    switch (Config.CurrentGameType)
    {
    case FirstLabTriangles:
        return LegasyCode.FirstLabStart_a();
    case FirstLabCubes:
        return LegasyCode.FirstLabStart_b();
    case SecondLab:
        return LegasyCode.SecondLabStart();
    case ThirdLab:
        return LegasyCode.ThirdLabStart(ContentRoot);
    case ForthLab:
        return LegasyCode.ForthLabStart(ContentRoot);
    default:
        return 0;
    }
#else
    BaseEngine Engine;
    
    if (!Engine.Initialize(Resource, Config))
    {
        std::cerr << "Engine initialization failed." << std::endl;
        return 1;
    }
    if (!Engine.StartUp())
    {
        std::cerr << "Engine startup failed." << std::endl;
        return 1;
    }
    
    if (!Engine.Play())
    {
        std::cerr << "Engine play failed." << std::endl;
        return 1;
    }
    do {/*nothing or engine health check*/} while (Engine.IsPlaying());
    
    Engine.Stop();
    Resource.DeInitialize();
    
#endif
}
