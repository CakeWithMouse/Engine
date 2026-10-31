#pragma once
#include "QtHost.h"
#include <iostream>
#include <stdexcept>

namespace OverlayHostTests {
struct Sink {
    EngineViewportOverlayApi api{};
    int iconCalls=0, lineCalls=0, billboardCalls=0;
    uint32_t cameraStatus=EngineSceneOk, iconStatus=EngineSceneOk;
    uint32_t width=800;
    uint8_t pixel=0;
    uint32_t lines=0, billboards=0;
    Sink() {
        api={1,this,
            [](void* p,EngineCameraInfo* c)->uint32_t {auto& s=*static_cast<Sink*>(p);c->width=s.width;c->height=600;return s.cameraStatus;},
            [](void* p,const EngineOverlayLine*,uint32_t n,float)->uint32_t {auto& s=*static_cast<Sink*>(p);++s.lineCalls;s.lines=n;return EngineSceneOk;},
            [](void* p,uint32_t,uint32_t,uint32_t,const uint8_t* rgba)->uint32_t {auto& s=*static_cast<Sink*>(p);++s.iconCalls;s.pixel=*rgba;return s.iconStatus;},
            [](void* p,const EngineOverlayBillboard*,uint32_t n)->uint32_t {auto& s=*static_cast<Sink*>(p);++s.billboardCalls;s.billboards=n;return EngineSceneOk;}};
    }
};
inline int Run() {
    int checks=0;auto check=[&](bool ok,const char* text){if(!ok)throw std::runtime_error(text);++checks;};
    Sink first, second;
    struct Service { const EngineViewportOverlayApi* current; } service{&first.api};
    EngineModuleHost callbacks{};callbacks.user=&service;
    callbacks.getService=[](void* p,uint32_t id,uint32_t)->const void* {
        return id==EngineViewportOverlayService?static_cast<Service*>(p)->current:nullptr;
    };
    EditorQtHost host(callbacks);
    EngineCameraInfo camera{};
    check(!host.GetCamera(camera),"camera unavailable before first update");
    uint8_t pixels[4]={42,0,0,255};
    host.SetOverlayIcon(1,1,1,pixels);pixels[0]=77;
    host.SetOverlayLines({EngineOverlayLine{}},2);
    host.SetOverlayBillboards({EngineOverlayBillboard{}});
    host.Update(0.016);
    check(host.GetCamera(camera) && camera.width==800,"camera success uses zero ABI status");
    check(first.iconCalls==1 && first.pixel==42,"icon submission copies caller bytes");
    check(first.lines==1 && first.billboards==1,"geometry reaches overlay API");
    host.Update(0.016);check(first.iconCalls==1,"unchanged icons are not uploaded each frame");
    pixels[0]=42;host.SetOverlayIcon(1,1,1,pixels);host.Update(0.016);
    check(first.iconCalls==1,"identical icon deduplicated");
    first.iconStatus=EngineSceneFailure;pixels[0]=90;host.SetOverlayIcon(1,1,1,pixels);host.Update(0.016);
    check(first.iconCalls==2,"changed icon uploaded");
    first.iconStatus=EngineSceneOk;host.Update(0.016);
    check(first.iconCalls==3 && first.pixel==90,"failed upload retried");
    first.cameraStatus=EngineSceneFailure;host.Update(0.016);
    check(!host.GetCamera(camera),"failed camera query invalidates stale camera");
    first.cameraStatus=EngineSceneOk;first.width=0;host.Update(0.016);
    check(!host.GetCamera(camera),"zero-size viewport has no usable camera");
    first.width=800;host.Update(0.016);
    check(host.GetCamera(camera),"camera recovers after resize");
    service.current=&second.api;host.Update(0.016);
    check(second.iconCalls==1 && second.pixel==90,"new DLL session receives cached icons");
    check(second.lines==1 && second.billboards==1,"new DLL session receives current geometry");
    check(first.iconCalls==3,"old DLL callback not called after service replacement");
    host.SetOverlayLines({});host.SetOverlayBillboards({});host.Update(0.016);
    check(second.lines==0 && second.billboards==0,"empty submissions clear overlay geometry");
    service.current=nullptr;host.Update(0.016);
    check(!host.GetCamera(camera),"removed service invalidates camera");
    service.current=&first.api;host.Update(0.016);
    check(first.iconCalls==4,"service reappearance restores icons");
    check(first.lines==0 && first.billboards==0,"service reappearance preserves cleared overlays");
    std::cout<<"Overlay host checks: "<<checks<<std::endl;return checks;
}
}
