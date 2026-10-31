#pragma once
#include "SceneCommandHandler.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <map>

// In-memory scene with real checkpoint files: tests exercise command routing,
// selection and history independently of the renderer and game DLL.
namespace HostRegressionTests {
struct Scene {
    std::map<uint64_t, EngineSceneTransform> objects{{1, {{0,0,0},{0,0,0,1},{1,1,1}}}, {2, {{4,0,0},{0,0,0,1},{1,1,1}}}};
    std::map<uint64_t, uint64_t> parents;
    int saves = 0, batchCalls = 0, propertyCalls = 0;
    bool failSave = false, failLoad = false, failMutation = false;
    uint32_t operation = 99;
    std::vector<uint64_t> highlighted;
    EngineEditSessionApi api{};
    EngineSceneBatchApi batch{};
    EngineAuthoringApi authoring{};
    Scene() {
        api.context = api.scene.context = this;
        api.saveScene = [](void* p, const char* path, char*, uint32_t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p); ++s.saves;
            if (s.failSave) return EngineSceneIoError;
            QJsonArray rows;
            for (const auto& [id,t] : s.objects) rows.append(QJsonObject{{"id",qint64(id)},{"x",t.position[0]}});
            QFile f(QString::fromUtf8(path));
            return f.open(QIODevice::WriteOnly) && f.write(QJsonDocument(rows).toJson()) > 0 ? EngineSceneOk : EngineSceneIoError;
        };
        api.loadScene = [](void* p, const char* path, uint64_t* id, char*, uint32_t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p); if(s.failLoad) return EngineSceneInvalidDocument;
            QFile f(QString::fromUtf8(path)); if(!f.open(QIODevice::ReadOnly)) return EngineSceneIoError;
            auto rows = QJsonDocument::fromJson(f.readAll()).array(); s.objects.clear();
            for(const auto& row: rows) { auto o=row.toObject(); s.objects[o["id"].toInteger()]={{float(o["x"].toDouble()),0,0},{0,0,0,1},{1,1,1}}; }
            *id=1; return EngineSceneOk;
        };
        api.readActiveScene = [](void* p, EngineSceneInfo*, EngineSceneObjectVisitor visitor, void* user) -> uint32_t {
            auto& s=*static_cast<Scene*>(p);
            for(const auto& [id,t]:s.objects) {
                EngineSceneObjectInfo o{}; o.id=id; o.name=id==1?"First":"Second"; o.typeName="Primitive.Cube"; o.transform=t; o.parentId=s.parents[id];
                visitor(user,&o);
            }
            return EngineSceneOk;
        };
        api.scene.setTransform = [](void* p,uint64_t id,const EngineSceneTransform* t) -> uint32_t {
            auto& s=*static_cast<Scene*>(p); if(s.failMutation) return EngineSceneFailure;
            if(!s.objects.count(id)) return EngineSceneNotFound; s.objects[id]=*t; return EngineSceneOk;
        };
        api.scene.renameObject = [](void*,uint64_t,const char*) -> uint32_t { return EngineSceneOk; };
        api.scene.createObject = [](void* p,const char*,const char*,const EngineSceneTransform* t,uint64_t* id) -> uint32_t {
            auto& s=*static_cast<Scene*>(p); *id=s.objects.rbegin()->first+1; s.objects[*id]=*t; return EngineSceneOk;
        };
        api.setParent = [](void* p,uint64_t child,uint64_t parent,uint32_t) -> uint32_t {
            static_cast<Scene*>(p)->parents[child]=parent; return EngineSceneOk;
        };
        batch.context=this; batch.version=1;
        batch.select = [](void* p,const uint64_t* ids,uint32_t count) -> uint32_t {
            auto& s=*static_cast<Scene*>(p); s.highlighted.clear(); if(count) s.highlighted.assign(ids,ids+count); return EngineSceneOk;
        };
        batch.apply = [](void* p,uint32_t op,const uint64_t* ids,uint32_t count,uint64_t active,const EngineSceneTransform* target,uint64_t* output,char*,uint32_t) -> uint32_t {
            auto& s=*static_cast<Scene*>(p); ++s.batchCalls; s.operation=op;
            if(s.failMutation) return EngineSceneFailure;
            const float delta=target->position[0]-s.objects.at(active).position[0];
            for(uint32_t i=0;i<count;++i) {
                output[i]=ids[i];
                if(op==EngineBatchTransform) s.objects[ids[i]].position[0]+=delta;
                else if(op==EngineBatchDelete) {s.objects.erase(ids[i]);output[i]=0;}
                else {output[i]=s.objects.rbegin()->first+1;s.objects[output[i]]=s.objects.at(ids[i]);}
            }
            return EngineSceneOk;
        };
        authoring.version=1; authoring.context=this;
        authoring.describe = [](void*,uint64_t,EngineProjectDataVisitor visitor,void* p) -> uint32_t {visitor(p,"{\"fields\":[]}");return EngineSceneOk;};
        authoring.setProperty = [](void* p,uint64_t,const char*,const char*,char*,uint32_t) -> uint32_t {
            auto& s=*static_cast<Scene*>(p);++s.propertyCalls;return s.failMutation?EngineSceneInvalidArgument:EngineSceneOk;
        };
    }
};
inline int Run() {
    int checks=0;
    auto check=[&](bool ok,const char* message){if(!ok) throw std::runtime_error(message);++checks;};
    Scene s; SceneCommandHandler handler; EditorHistory history;
    check(history.Initialize(),"history initialization");
    SceneCommandHandler::Context ctx{s.api,{}, {}, {}, &s.batch, &s.authoring};
    handler.PublishScene(ctx,IEditorHost::Edit);
    auto run=[&](EditorCommand c,IEditorHost::EditorState state=IEditorHost::Edit) {
        uint32_t result=999;QString error;handler.Handle(c,ctx,history,state,result,error);if(result!=EngineSceneOk) std::cerr << "Command " << c.type << " status " << result << ": " << error.toStdString() << std::endl;return result;
    };
    EditorCommand select;select.type=EditorCommand::SelectObject;select.objectId=1;select.objectIds={1,2};
    check(run(select)==EngineSceneOk,"group selection succeeds");
    check(handler.ReadSnapshot().selectedIds.size()==2,"group selection retained");
    check(s.highlighted.size()==2,"group highlight forwarded");
    check(handler.ReadSnapshot().authoring,"authoring capability published");
    check(!handler.ReadSnapshot().objects.front().propertySchema.empty(),"selected schema published");
    EditorCommand move;move.type=EditorCommand::UpdateObjectTransform;move.objectId=1;move.path="nudge";move.right=2;
    check(run(move)==EngineSceneOk,"group nudge succeeds");
    check(s.batchCalls==1 && s.operation==EngineBatchTransform,"group uses batch API");
    check(s.objects[1].position[0]==2 && s.objects[2].position[0]==6,"relative group positions preserved");
    check(run(move)==EngineSceneOk && s.saves==1,"consecutive nudges share checkpoint");
    EditorCommand property;property.type=EditorCommand::SetProperty;property.objectId=1;property.path="/speed";property.name="5";
    check(run(property)==EngineSceneOk && s.propertyCalls==1,"authoring setter routed");
    check(run(move)==EngineSceneOk && s.saves==3,"property edit breaks nudge coalescing");
    EditorCommand undo;undo.type=EditorCommand::Undo;
    check(run(undo)==EngineSceneOk && s.objects[1].position[0]==4,"undo restores pre-nudge scene");
    check(history.CanRedo(),"undo enables redo");
    EditorCommand redo;redo.type=EditorCommand::Redo;
    check(run(redo)==EngineSceneOk && s.objects[1].position[0]==6,"redo restores group movement");
    check(run(select)==EngineSceneOk,"reselect after reload");
    const int before=s.saves;
    check(run(move,IEditorHost::Play)==EngineSceneReadOnly,"Play rejects transform");
    check(run(property,IEditorHost::Pause)==EngineSceneReadOnly,"Pause rejects property edit");
    check(s.saves==before,"read-only commands do not checkpoint");
    s.failMutation=true;
    check(run(property)==EngineSceneInvalidArgument,"property validation failure propagated");
    check(run(move)==EngineSceneFailure,"batch failure propagated");
    check(s.objects[1].position[0]==6,"failed mutation preserves scene");
    s.failMutation=false;
    ctx.authoring=nullptr;
    check(run(property)==EngineSceneReadOnly,"legacy DLL without authoring remains read-only");
    ctx.batch=nullptr;run(select);
    check(handler.ReadSnapshot().selectedIds.size()==1 && !handler.ReadSnapshot().batchEditing,"legacy DLL falls back to one selection");
    ctx.batch=&s.batch;ctx.authoring=&s.authoring;run(select);
    handler.PickSelection(2,true);handler.PublishScene(ctx,IEditorHost::Edit);
    check(handler.ReadSnapshot().selectedIds==std::vector<uint64_t>{1},"toggle removes selected object");
    handler.PickSelection(2,true);handler.PublishScene(ctx,IEditorHost::Edit);
    check(handler.ReadSnapshot().selectedIds.size()==2,"toggle adds object");
    handler.PickSelection(0,false);handler.PublishScene(ctx,IEditorHost::Edit);
    check(!handler.HasSelection() && s.highlighted.empty(),"empty pick clears selection");
    EditorCommand iconSelect;iconSelect.type=EditorCommand::SelectObject;iconSelect.objectId=1;iconSelect.name="toggle";
    check(run(iconSelect)==EngineSceneOk && handler.ReadSnapshot().selectedIds==std::vector<uint64_t>{1},"billboard toggle starts selection");
    iconSelect.objectId=2;run(iconSelect);
    check(handler.ReadSnapshot().selectedIds.size()==2,"billboard toggle extends selection");
    run(iconSelect);check(handler.ReadSnapshot().selectedIds==std::vector<uint64_t>{1},"billboard toggle removes selection");
    s.parents[2]=1;select.objectId=2;run(select);
    check(handler.ReadSnapshot().selected==1,"selected ancestor anchors group transform");
    handler.PublishScene(ctx,IEditorHost::Play);
    check(s.highlighted.empty(),"Play clears edit selection highlight");
    EditorCommand create;create.type=EditorCommand::CreateObject;create.path="Primitive.Cube";create.name="Child";create.parentId=1;
    check(run(create)==EngineSceneOk,"create child succeeds");
    check(s.parents[handler.SelectedId()]==1,"create child preserves requested parent");
    history.Reset();handler.SetSelectedId(1);
    s.failSave=true;
    check(run(move)==EngineSceneFailure,"checkpoint failure blocks mutation");
    check(!history.CanUndo(),"failed checkpoint adds no history");s.failSave=false;
    run(move);s.failLoad=true;
    check(run(undo)==EngineSceneInvalidDocument && history.CanUndo(),"failed undo retains history");
    s.failLoad=false;check(run(undo)==EngineSceneOk,"undo retries after load failure");
    check(history.CanRedo(),"successful retry enables redo");
    history.Reset();check(!history.CanUndo() && !history.CanRedo(),"reset clears both history stacks");
    run(select);EditorCommand duplicate;duplicate.type=EditorCommand::DuplicateObject;duplicate.objectId=1;
    check(run(duplicate)==EngineSceneOk && s.operation==EngineBatchDuplicate,"group duplicate routed");
    check(handler.ReadSnapshot().selectedIds.size()==2,"duplicates become selected group");
    EditorCommand remove;remove.type=EditorCommand::RemoveObject;remove.objectId=handler.SelectedId();
    check(run(remove)==EngineSceneOk && s.operation==EngineBatchDelete,"group deletion routed");
    check(!handler.HasSelection(),"deleting group clears selection");
    history.BeginGizmoDrag(1);move.path="gizmo";
    auto now=std::chrono::steady_clock::now();history.OnTransformApplied(move,now);history.ClearHistory();
    check(!history.ShouldCoalesce(move,now),"clearing history invalidates active gesture checkpoint");
    std::cout<<"Host regression checks: "<<checks<<std::endl;return checks;
}
}
