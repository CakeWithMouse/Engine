#pragma once
#include <Engine/Runtime/EditSessionApi.h>

// Frozen Windows x64 ABI shipped with point lights, before materials. Never change these layouts.
namespace EngineLegacyV5
{
struct Object
{
    uint64_t id; const char* name; const char* typeName; uint64_t parentId;
    uint32_t inheritParentScale; EngineSceneTransform transform;
    const char* modelAsset; const char* textureAsset;
    uint32_t hasPointLight; EngineScenePointLight pointLight;
};
using Visitor = void (ENGINE_MODULE_CALL *)(void*, const Object*);
struct SceneApi
{
    uint32_t version; void* context;
    uint32_t (ENGINE_MODULE_CALL *listTypes)(void*, EngineSceneTypeVisitor, void*);
    uint32_t (ENGINE_MODULE_CALL *readScene)(void*, EngineSceneInfo*, Visitor, void*);
    uint32_t (ENGINE_MODULE_CALL *createObject)(void*, const char*, const char*, const EngineSceneTransform*, uint64_t*);
    uint32_t (ENGINE_MODULE_CALL *removeObject)(void*, uint64_t);
    uint32_t (ENGINE_MODULE_CALL *renameObject)(void*, uint64_t, const char*);
    uint32_t (ENGINE_MODULE_CALL *setTransform)(void*, uint64_t, const EngineSceneTransform*);
    uint32_t (ENGINE_MODULE_CALL *setPointLight)(void*, uint64_t, const EngineScenePointLight*);
};
struct EditApi
{
    uint32_t version; void* context; SceneApi scene;
    uint32_t (ENGINE_MODULE_CALL *control)(void*, uint32_t);
    uint32_t (ENGINE_MODULE_CALL *getState)(void*, uint32_t*);
    uint32_t (ENGINE_MODULE_CALL *readActiveScene)(void*, EngineSceneInfo*, Visitor, void*);
    uint32_t (ENGINE_MODULE_CALL *setParent)(void*, uint64_t, uint64_t, uint32_t);
    uint32_t (ENGINE_MODULE_CALL *attachViewport)(void*, void*, uint32_t, uint32_t);
    uint32_t (ENGINE_MODULE_CALL *resizeViewport)(void*, uint32_t, uint32_t);
    uint32_t (ENGINE_MODULE_CALL *camera)(void*, float, float, float, float, float, float);
    uint32_t (ENGINE_MODULE_CALL *frame)(void*, double);
    uint32_t (ENGINE_MODULE_CALL *saveScene)(void*, const char*, char*, uint32_t);
    uint32_t (ENGINE_MODULE_CALL *loadScene)(void*, const char*, uint64_t*, char*, uint32_t);
    uint32_t (ENGINE_MODULE_CALL *pickObject)(void*, float, float, uint64_t*);
    uint32_t (ENGINE_MODULE_CALL *focusObject)(void*, uint64_t);
    uint32_t (ENGINE_MODULE_CALL *setContentRoot)(void*, const char*, char*, uint32_t);
    uint32_t (ENGINE_MODULE_CALL *importAsset)(void*, const char*, uint32_t, char*, uint32_t, char*, uint32_t);
    uint32_t (ENGINE_MODULE_CALL *createModel)(void*, const char*, const char*, const EngineSceneTransform*, uint64_t*, char*, uint32_t);
    uint32_t (ENGINE_MODULE_CALL *setTexture)(void*, uint64_t, const char*, char*, uint32_t);
};
struct Factory
{
    uint32_t version;
    int (ENGINE_MODULE_CALL *create)(const char*, EditApi*);
    void (ENGINE_MODULE_CALL *destroy)(void*);
};
using GetFactory = int (ENGINE_MODULE_CALL *)(uint32_t, uint32_t, Factory*);
using GetScene = int (ENGINE_MODULE_CALL *)(uint32_t, uint32_t, void*, SceneApi*);
static_assert(sizeof(Object) == 128 && sizeof(SceneApi) == 72 && sizeof(EditApi) == 216 && sizeof(Factory) == 24,
    "Frozen v5 x64 ABI changed");

struct VisitBridge
{
    EngineSceneObjectVisitor visitor; void* user;
    static void ENGINE_MODULE_CALL Visit(void* p, const Object* o)
    {
        auto& b = *static_cast<VisitBridge*>(p);
        const EngineSceneObjectInfo value{o->id,o->name,o->typeName,o->parentId,o->inheritParentScale,
            o->transform,o->modelAsset,o->textureAsset,o->hasPointLight,o->pointLight,nullptr};
        b.visitor(b.user, &value);
    }
};
inline bool Complete(const SceneApi& a)
{
    return a.version == 3 && a.context && a.listTypes && a.readScene && a.createObject && a.removeObject && a.renameObject && a.setTransform;
}
inline EngineSceneApi Adapt(SceneApi& legacy)
{
    return {ENGINE_SCENE_API_VERSION, &legacy,
        [](void* p, EngineSceneTypeVisitor v, void* u) { auto& a = *static_cast<SceneApi*>(p); return a.listTypes(a.context,v,u); },
        [](void* p, EngineSceneInfo* i, EngineSceneObjectVisitor v, void* u) -> uint32_t {
            if (!v) return EngineSceneInvalidArgument;
            auto& a = *static_cast<SceneApi*>(p); VisitBridge b{v,u}; return a.readScene(a.context,i,VisitBridge::Visit,&b); },
        [](void* p, const char* t, const char* n, const EngineSceneTransform* x, uint64_t* id) { auto& a = *static_cast<SceneApi*>(p); return a.createObject(a.context,t,n,x,id); },
        [](void* p, uint64_t id) { auto& a = *static_cast<SceneApi*>(p); return a.removeObject(a.context,id); },
        [](void* p, uint64_t id, const char* n) { auto& a = *static_cast<SceneApi*>(p); return a.renameObject(a.context,id,n); },
        [](void* p, uint64_t id, const EngineSceneTransform* t) { auto& a = *static_cast<SceneApi*>(p); return a.setTransform(a.context,id,t); },
        [](void* p, uint64_t id, const EngineScenePointLight* l) -> uint32_t { auto& a = *static_cast<SceneApi*>(p); return a.setPointLight ? a.setPointLight(a.context,id,l) : EngineSceneReadOnly; }};
}
inline bool Complete(const EditApi& a)
{
    return a.version == 5 && a.context && Complete(a.scene) && a.control && a.getState && a.readActiveScene &&
        a.setParent && a.attachViewport && a.resizeViewport && a.camera && a.frame && a.saveScene && a.loadScene &&
        a.pickObject && a.focusObject && a.setContentRoot && a.importAsset && a.createModel && a.setTexture;
}
inline EngineEditSessionApi Adapt(EditApi& legacy)
{
    return {ENGINE_EDIT_API_VERSION, &legacy, Adapt(legacy.scene),
        [](void* p, uint32_t c) { auto& a = *static_cast<EditApi*>(p); return a.control(a.context,c); },
        [](void* p, uint32_t* s) { auto& a = *static_cast<EditApi*>(p); return a.getState(a.context,s); },
        [](void* p, EngineSceneInfo* i, EngineSceneObjectVisitor v, void* u) -> uint32_t {
            if (!v) return EngineSceneInvalidArgument;
            auto& a = *static_cast<EditApi*>(p); VisitBridge b{v,u}; return a.readActiveScene(a.context,i,VisitBridge::Visit,&b); },
        [](void* p, uint64_t c, uint64_t parent, uint32_t i) { auto& a = *static_cast<EditApi*>(p); return a.setParent(a.context,c,parent,i); },
        [](void* p, void* w, uint32_t x, uint32_t y) { auto& a = *static_cast<EditApi*>(p); return a.attachViewport(a.context,w,x,y); },
        [](void* p, uint32_t x, uint32_t y) { auto& a = *static_cast<EditApi*>(p); return a.resizeViewport(a.context,x,y); },
        [](void* p, float r, float u, float f, float y, float pitch, float z) { auto& a = *static_cast<EditApi*>(p); return a.camera(a.context,r,u,f,y,pitch,z); },
        [](void* p, double dt) { auto& a = *static_cast<EditApi*>(p); return a.frame(a.context,dt); },
        [](void* p, const char* f, char* e, uint32_t n) { auto& a = *static_cast<EditApi*>(p); return a.saveScene(a.context,f,e,n); },
        [](void* p, const char* f, uint64_t* id, char* e, uint32_t n) { auto& a = *static_cast<EditApi*>(p); return a.loadScene(a.context,f,id,e,n); },
        [](void* p, float x, float y, uint64_t* id) { auto& a = *static_cast<EditApi*>(p); return a.pickObject(a.context,x,y,id); },
        [](void* p, uint64_t id) { auto& a = *static_cast<EditApi*>(p); return a.focusObject(a.context,id); },
        [](void* p, const char* f, char* e, uint32_t n) { auto& a = *static_cast<EditApi*>(p); return a.setContentRoot(a.context,f,e,n); },
        [](void* p, const char* f, uint32_t k, char* out, uint32_t cap, char* e, uint32_t n) { auto& a = *static_cast<EditApi*>(p); return a.importAsset(a.context,f,k,out,cap,e,n); },
        [](void* p, const char* f, const char* name, const EngineSceneTransform* t, uint64_t* id, char* e, uint32_t n) { auto& a = *static_cast<EditApi*>(p); return a.createModel(a.context,f,name,t,id,e,n); },
        [](void* p, uint64_t id, const char* f, char* e, uint32_t n) { auto& a = *static_cast<EditApi*>(p); return a.setTexture(a.context,id,f,e,n); }};
}
}
