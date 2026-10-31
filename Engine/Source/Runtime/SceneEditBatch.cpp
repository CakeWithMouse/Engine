#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Scene/SceneDocument.h>
#include <Engine/Scene/SceneAssets.h>
#include <Engine/Components/GameComponents.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace {
glm::mat4 Matrix(const SceneTransform& t) { return glm::translate(glm::mat4(1),t.position)*glm::mat4_cast(t.rotation)*glm::scale(glm::mat4(1),t.scale); }
glm::mat4 World(GameComponent& object) {
    const auto matrix=object.GetWorldMatrix(); glm::mat4 result(1);
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)result[r][c]=matrix.m[r][c];
    return result;
}
SceneTransform Transform(const glm::mat4& matrix) {
    SceneTransform t; glm::vec3 skew; glm::vec4 perspective;
    if(!glm::decompose(matrix,t.scale,t.rotation,t.position,skew,perspective))throw std::runtime_error("Cannot decompose group transform");
    t.rotation=glm::normalize(t.rotation); const auto check=Matrix(t);
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)
        if(!std::isfinite(check[r][c]) || std::abs(check[r][c]-matrix[r][c])>0.001f*(1+std::abs(matrix[r][c])))
            throw std::runtime_error("Group transform would introduce shear. Use uniform scale or unparent the objects.");
    return t;
}
}
EngineSceneBatchApi SceneEditSession::GetBatchApi() noexcept {
    return {1,this,
        [](void* p,uint32_t operation,const uint64_t* ids,uint32_t count,uint64_t active,const EngineSceneTransform* target,uint64_t* output,char* error,uint32_t capacity)->uint32_t {
            if(capacity && !error)return EngineSceneInvalidArgument;
            if(capacity)error[0]=0;
            if(!ids||!count||count>100000||!output||operation>EngineBatchDelete)return EngineSceneInvalidArgument;
            return Invoke(p,[&](SceneEditSession& s)->uint32_t {
                if(s.state_!=EngineEditing)return EngineSceneReadOnly;
                if(s.attached_&&(!s.width_||!s.height_))return EngineSceneBusy;
                try {
                    auto data=s.edit_->GetScene().GetSnapshot();
                    auto& scene=s.edit_->GetScene();
                    std::set<uint64_t> selected(ids,ids+count);
                    if(selected.size()!=count)return EngineSceneInvalidArgument;
                    for(auto id:selected)if(!scene.FindComponent(id))return EngineSceneNotFound;
                    std::vector<std::string> selectedNames;
                    for(uint32_t i=0;i<count;++i)selectedNames.push_back(std::find_if(data.components.begin(),data.components.end(),[&](auto& o){return o.id==ids[i];})->name);
                    auto remapData=[&](SceneComponentSnapshot& o,const std::map<ComponentId,ComponentId>& mapping) {
                        if(o.projectData.empty())return;
                        auto object=o.typeName==SceneAssets::ModelType ? SceneAssets::CreateModel(o.modelAsset,false) : s.catalog_.Find(o.typeName)->create();
                        if(!object)throw std::runtime_error("Cannot create object for reference remapping");
                        object->LoadProjectData(o.projectData);object->RemapProjectReferences(mapping);o.projectData=object->SaveProjectData();
                    };
                    if(operation==EngineBatchTransform) {
                        if(!target||!selected.count(active))return EngineSceneInvalidArgument;
                        auto* anchor=scene.FindComponent(active);
                        for(auto* parent=anchor->GetParent();parent;parent=parent->GetParent())
                            if(selected.count(parent->GetComponentId()))throw std::runtime_error("Use the selected ancestor as the group transform anchor");
                        const auto before=std::find_if(data.components.begin(),data.components.end(),[&](auto& o){return o.id==active;})->transform;
                        SceneTransform after; for(int i=0;i<3;++i){after.position[i]=target->position[i];after.scale[i]=target->scale[i];}
                        after.rotation={target->rotation[3],target->rotation[0],target->rotation[1],target->rotation[2]};
                        for(int i=0;i<3;++i)if(!std::isfinite(after.position[i])||!std::isfinite(after.scale[i])||std::abs(after.scale[i])<1e-6f)return EngineSceneInvalidArgument;
                        if(!std::isfinite(glm::dot(after.rotation,after.rotation))||glm::dot(after.rotation,after.rotation)<1e-12f)return EngineSceneInvalidArgument;
                        after.rotation=glm::normalize(after.rotation);
                        const auto anchorWorld=World(*anchor);
                        const auto parentWorld=anchorWorld*glm::inverse(Matrix(before));
                        const auto delta=parentWorld*Matrix(after)*glm::inverse(anchorWorld);
                        for(auto& o:data.components)if(selected.count(o.id)) {
                            auto parent=o.parentId;bool covered=false;
                            while(parent){if(selected.count(parent)){covered=true;break;}auto* c=scene.FindComponent(parent);parent=c&&c->GetParent()?c->GetParent()->GetComponentId():0;}
                            if(covered)continue;
                            const auto world=World(*scene.FindComponent(o.id));
                            const auto parentMatrix=world*glm::inverse(Matrix(o.transform));
                            o.transform=Transform(glm::inverse(parentMatrix)*delta*world);
                        }
                    } else {
                        // A selected parent owns its entire duplicated/deleted subtree.
                        bool changed=true;while(changed){changed=false;for(const auto& o:data.components)if(selected.count(o.parentId)&&selected.insert(o.id).second)changed=true;}
                        if(operation==EngineBatchDelete) {
                            std::map<ComponentId,ComponentId> mapping;for(auto id:selected)mapping[id]=0;
                            data.components.erase(std::remove_if(data.components.begin(),data.components.end(),[&](auto& o){return selected.count(o.id)!=0;}),data.components.end());
                            for(auto& o:data.components)remapData(o,mapping);
                        } else {
                            uint64_t next=0;std::set<std::string> names;for(auto& o:data.components){next=(std::max)(next,o.id);names.insert(o.name);}
                            if(next>UINT64_MAX-selected.size())throw std::runtime_error("Object ID limit reached");
                            std::map<ComponentId,ComponentId> mapping;for(auto id:selected)mapping[id]=++next;
                            std::map<uint64_t,std::string> newNames;std::vector<SceneComponentSnapshot> copies;
                            for(const auto& source:data.components)if(selected.count(source.id)) {
                                auto copy=source;copy.id=mapping.at(source.id);int suffix=2;
                                do{copy.name=source.name+" "+std::to_string(suffix++);}while(names.count(copy.name));
                                names.insert(copy.name);newNames[source.id]=copy.name;
                                if(mapping.count(copy.parentId))copy.parentId=mapping.at(copy.parentId);
                                else copy.transform.position.x+=1;
                                remapData(copy,mapping);copies.push_back(std::move(copy));
                            }
                            data.components.insert(data.components.end(),copies.begin(),copies.end());
                            for(uint32_t i=0;i<count;++i)selectedNames[i]=newNames.at(ids[i]);
                        }
                    }
                    if(operation==EngineBatchTransform) {
                        SceneDocument::Validate(data,s.catalog_);
                        // Validation precedes all mutations. The existing IDs and
                        // live material resources stay stable throughout a drag.
                        for(const auto& o:data.components)if(selected.count(o.id))scene.SetTransform(o.id,o.transform);
                    } else s.ReplaceEdit(data);
                    for(uint32_t i=0;i<count;++i){auto* object=s.edit_->GetScene().FindComponentByName(selectedNames[i]);output[i]=object?object->GetComponentId():0;}
                    return EngineSceneOk;
                } catch(const std::exception& e){if(capacity)std::snprintf(error,capacity,"%s",e.what());return EngineSceneFailure;}
            });
        },
        [](void* p,const uint64_t* ids,uint32_t count)->uint32_t {
            if((count&&!ids)||count>100000)return EngineSceneInvalidArgument;
            return Invoke(p,[&](SceneEditSession& s)->uint32_t {
                std::vector<uint64_t> selected;for(uint32_t i=0;i<count;++i){if(!s.edit_->GetScene().FindComponent(ids[i]))return EngineSceneNotFound;selected.push_back(ids[i]);}
                s.edit_->SetEditorSelections(std::move(selected));return EngineSceneOk;
            });
        }};
}
