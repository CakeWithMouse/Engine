#include "SdkModule.h"
#include <Engine/Project/ProjectModule.h>
static ComponentTypeCatalog ProjectComponentTypes() { return {}; }
#include <Engine/Project/SceneModule.inl>
extern "C" __declspec(dllexport) int ProbeSubsystem(int action)
{
    if(action==1) return subsystemTicks;
    if(action==2) failSubsystem=true;
    if(action==3) failSubsystem=false;
    return liveSubsystems;
}
