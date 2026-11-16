#pragma once
#include "AiMachineEditorWindow.h"
#include "FsmGraphWidget.h"
#include "FsmBlockListEditor.h"
#include "ViewportWidget.h"
#include <QAbstractButton>
#include <QCheckBox>
#include <QStatusBar>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QSpinBox>
#include <QScrollArea>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

// Parameterized native Qt authoring + the same runtime extension used by Play.
namespace AiEditorDiagnostics {
using PointShadowDiagnostics::Check;
inline QJsonObject Messages(const QHash<int,QString>& values) {
    QJsonObject out; for(auto it=values.begin();it!=values.end();++it)out[QString::number(it.key())]=it.value(); return out;
}
inline QJsonArray Live(const EngineAiMachineApi& api) {
    QJsonArray units;
    api.listUnits(api.context,"",[](void* p,const EngineAiUnit* u){
        static_cast<QJsonArray*>(p)->append(QJsonObject{{"id",double(u->objectId)},{"name",u->objectName},
            {"machine",u->machine},{"activePath",u->activePath},{"running",bool(u->running)}});
    },&units);
    for(int i=0;i<units.size();++i) {
        auto unit=units[i].toObject();const auto id=uint64_t(unit["id"].toDouble());
        QJsonArray states,memory,history;
        api.readStates(api.context,id,[](void* p,const EngineAiState* s){static_cast<QJsonArray*>(p)->append(
            QJsonObject{{"node",s->node},{"name",s->name},{"path",s->path}});},&states);
        api.readMemory(api.context,id,[](void* p,const EngineAiMemoryValue* m){static_cast<QJsonArray*>(p)->append(
            QJsonObject{{"key",m->key},{"type",m->type},{"text",m->text}});},&memory);
        api.readHistory(api.context,id,[](void* p,const char* text){static_cast<QJsonArray*>(p)->append(QString::fromUtf8(text));},&history);
        unit["states"]=states;unit["memory"]=memory;unit["history"]=history;units[i]=unit;
    }
    return units;
}
inline void Field(QWidget& root,const QString& name,const QJsonValue& value) {
    auto* field=root.findChild<QWidget*>(name);Check(field,"named field not found");
    Check(field->isEnabled(),"field is disabled");field->setFocus();
    if(auto* line=qobject_cast<QLineEdit*>(field)) {
        line->setText(value.toString());QMetaObject::invokeMethod(line,"editingFinished",Qt::DirectConnection);
    } else if(auto* spin=qobject_cast<QDoubleSpinBox*>(field)) {
        spin->setValue(value.toDouble());QMetaObject::invokeMethod(spin,"editingFinished",Qt::DirectConnection);
    } else if(auto* spin=qobject_cast<QSpinBox*>(field)) {
        spin->setValue(value.toInt());QMetaObject::invokeMethod(spin,"editingFinished",Qt::DirectConnection);
    }
    else if(auto* box=qobject_cast<QCheckBox*>(field))box->setChecked(value.toBool());
    else if(auto* combo=qobject_cast<QComboBox*>(field)) {
        int index=combo->findData(value.toVariant());if(index<0)index=combo->findText(value.toString());
        Check(index>=0,"combo value unavailable");combo->setCurrentIndex(index);
    } else throw std::runtime_error("unsupported named field type");
}

inline QJsonObject HudDialog(EditorMainWindow& main,const QJsonObject& step) {
    QJsonObject result;QString failure;bool handled=false;QElapsedTimer elapsed;elapsed.start();
    QTimer driver;
    QObject::connect(&driver,&QTimer::timeout,[&] {
        auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if(!dialog||dialog->objectName()!="HudSettingsDialog")return;
        if(handled){if(elapsed.elapsed()>2500){failure="HUD dialog did not accept input";dialog->reject();}return;}
        handled=true;
        try {
            auto* choice=dialog->findChild<QComboBox*>("ProjectHudChoice");Check(choice,"HUD choice absent");
            result["beforeChoice"]=choice->currentData().toString();
            if(step.contains("choice")) {
                const int index=choice->findData(step["choice"].toString());Check(index>=0,"HUD choice unavailable");
                choice->setCurrentIndex(index);
            }
            result["selectedChoice"]=choice->currentData().toString();
            const auto image=step["dialogImage"].toString();
            if(!image.isEmpty()) {
                Check(QFileInfo(image).isAbsolute()&&!QFileInfo::exists(image),"HUD image must be new absolute path");
                Check(dialog->grab().save(image),"HUD dialog image save failed");
            }
            auto* buttons=dialog->findChild<QDialogButtonBox*>();Check(buttons,"HUD dialog buttons absent");
            buttons->button(step["cancel"].toBool()?QDialogButtonBox::Cancel:QDialogButtonBox::Ok)->click();
        } catch(const std::exception& e){failure=QString::fromUtf8(e.what());dialog->reject();}
    });
    driver.start(10);main.ShowHudSettings();
    Check(handled,"HUD dialog was not opened");Check(failure.isEmpty(),failure.toUtf8().constData());
    result["cancelled"]=step["cancel"].toBool();return result;
}

// Drive the production modal editors through their actual double-click event.
// A bounded timer rejects invalid input instead of leaving CI in a modal loop.
inline void EditDialog(FsmGraphWidget& graph,const QJsonObject& step) {
    const bool state=step["action"]=="dialog-state";
    QPointF point;
    if(state) {
        const auto* node=graph.Document().FindNode(step["node"].toInt());
        Check(node,"dialog node not found");point=node->pos+QPointF(70,14);
        graph.FocusNode(node->id);
    } else {
        EditorCore::GraphConnectionData link;
        link.fromNode=step["from"].toInt();link.toNode=step["to"].toInt();link.fromPin="out";link.toPin="in";
        graph.SelectLink(link);
        bool found=false;
        for(auto* item:graph.View()->scene()->selectedItems())
            if(auto* path=dynamic_cast<QGraphicsPathItem*>(item)) {
                point=path->mapToScene(path->path().pointAtPercent(.5));found=true;break;
            }
        Check(found,"dialog link not found");graph.View()->centerOn(point);
    }
    QString failure;bool handled=false;QElapsedTimer elapsed;elapsed.start();
    QTimer driver;
    QObject::connect(&driver,&QTimer::timeout,[&] {
        auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if(!dialog||dialog->objectName()!=(state?"FsmStateDialog":"FsmLinkDialog"))return;
        if(handled) {if(elapsed.elapsed()>2500){failure="dialog did not accept input";dialog->reject();}return;}
        handled=true;
        try {
            for(const auto value:step["edits"].toArray()) {
                const auto edit=value.toObject();
                if(edit.contains("blockList")) {
                    auto* list=dynamic_cast<FsmBlockListEditor*>(dialog->findChild<QWidget*>(edit["blockList"].toString()));
                    Check(list,"block list not found");
                    if(edit.contains("add"))Check(list->AddBlock(edit["add"].toString()),"block type unavailable");
                    if(edit.contains("row"))list->SelectBlock(edit["row"].toInt());
                } else Field(*dialog,edit["name"].toString(),edit["value"]);
            }
            const auto image=step["dialogImage"].toString();
            if(!image.isEmpty()) {
                Check(QFileInfo(image).isAbsolute()&&!QFileInfo::exists(image),"dialog image must be new absolute path");
                Check(dialog->grab().save(image),"dialog image save failed");
            }
            auto* buttons=dialog->findChild<QDialogButtonBox*>();Check(buttons,"dialog buttons absent");
            buttons->button(step["cancel"].toBool()?QDialogButtonBox::Cancel:QDialogButtonBox::Ok)->click();
        } catch(const std::exception& e){failure=QString::fromUtf8(e.what());dialog->reject();}
    });
    driver.start(10);
    auto* viewport=graph.View()->viewport();const QPoint pixel=graph.View()->mapFromScene(point);
    QMouseEvent click(QEvent::MouseButtonDblClick,QPointF(pixel),QPointF(viewport->mapToGlobal(pixel)),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(viewport,&click);
    while(!handled&&elapsed.elapsed()<3000){QApplication::processEvents();QThread::msleep(2);}
    Check(handled,"double click did not open expected dialog");
    Check(failure.isEmpty(),failure.toUtf8().constData());
}
template<class Capture,class Objects>
QJsonObject Run(const QJsonObject& in,EditorQtHost& host,GameEditModule& module,Capture& capture,Objects objects) {
    Check(module.AiMachineApi().version==1,"AI machine extension unavailable");
    EditorMainWindow main(&host);main.setAttribute(Qt::WA_DontShowOnScreen);main.resize(1100,760);main.show();
    capture.window=&main;
    struct CaptureGuard {Capture& c;~CaptureGuard(){c.window=nullptr;}} captureGuard{capture};
    QStringList dialogs;
    QTimer dismiss;
    QObject::connect(&dismiss,&QTimer::timeout,[&]{for(auto* w:QApplication::topLevelWidgets())
        if(auto* box=qobject_cast<QMessageBox*>(w);box&&box->isVisible()){dialogs<<box->text();box->accept();}});
    dismiss.start(10);
    auto pump=[&](int milliseconds=80) {
        QElapsedTimer timer;timer.start();
        do {host.Update(0);QApplication::processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QThread::msleep(2);}
        while(timer.elapsed()<milliseconds);
    };
    auto resolve=[&](const QString& name)->uint64_t {
        uint64_t id=0;for(const auto value:objects()){auto o=value.toObject();if(o["name"]==name){Check(!id,"ambiguous scene name");id=uint64_t(o["id"].toDouble());}}
        Check(id!=0,"scene target not found");return id;
    };
    QString machine=in["machine"].toString();
    QPointer<AiMachineEditorWindow> editor;
    QPointer<UIEditorWindow> uiEditor;
    auto open=[&] {
        Check(machine.startsWith("/Game/"),"machine must be a /Game path");
        main.OpenAiMachineEditor(machine);pump();editor=main.AiMachineEditor(machine);
        QElapsedTimer timer;timer.start();
        while(editor&&!editor->IsReady()&&!editor->IsReadOnly()&&timer.elapsed()<10000)pump();
        Check(editor,"AI window not created");
    };
    pump();if(!machine.isEmpty())open();
    auto snapshot=[&] {
        QJsonObject result{{"objects",objects()},{"live",Live(module.AiMachineApi())}};
        const auto scene=host.ReadSnapshot();QJsonArray selected;
        for(auto id:scene.selectedIds)selected.append(double(id));
        result["selected"]=double(scene.selected);result["selectedIds"]=selected;
        result["sceneCanUndo"]=scene.canUndo;result["sceneCanRedo"]=scene.canRedo;
        EngineCameraInfo camera{};
        if(host.GetCamera(camera)) {
            QJsonArray view,projection;for(float v:camera.viewMatrix)view.append(v);for(float v:camera.projMatrix)projection.append(v);
            result["camera"]=QJsonObject{{"position",QJsonArray{camera.position[0],camera.position[1],camera.position[2]}},
                {"view",view},{"projection",projection},{"width",int(camera.width)},{"height",int(camera.height)}};
        }
        if(auto* viewport=main.findChild<ViewportWidget*>()) {
            result["viewportSize"]=QJsonArray{viewport->width(),viewport->height()};
            result["coordinateSpaceLocal"]=viewport->IsCoordinateSpaceLocal();
        }
        result["playState"]=int(host.PlayState());result["machine"]=machine;
        QFile hud(QDir(QString::fromUtf8(host.host().contentRoot)).filePath("ProjectHud.txt"));
        result["hudDescriptorExists"]=hud.exists();
        if(hud.open(QIODevice::ReadOnly))result["hudChoice"]=QString::fromUtf8(hud.readAll()).trimmed();
        if(uiEditor) {
            result["uiPath"]=uiEditor->FilePath();
            if(uiEditor->Root())result["uiMarkup"]=QString::fromStdString(UIMarkup::Serialize(*uiEditor->Root()));
            if(uiEditor->Selected())result["uiSelected"]=QString::fromStdString(uiEditor->Selected()->Name);
            QFile file(uiEditor->FilePath());
            if(file.open(QIODevice::ReadOnly))result["uiSavedMarkup"]=QString::fromUtf8(file.readAll());
        }
        if(editor) {
            result["ready"]=editor->IsReady();result["readOnly"]=editor->IsReadOnly();
            result["checkCount"]=editor->CheckCount();result["problems"]=Messages(editor->Problems());
            result["warnings"]=Messages(editor->Warnings());result["status"]=editor->statusBar()->currentMessage();
            result["detail"]=editor->Status();
            if(editor->IsReady()) {
                auto* graph=editor->Graph();result["graph"]=QJsonDocument::fromJson(graph->Document().ExportToJson().toUtf8()).object();
                result["canUndo"]=graph->CanUndo();result["canRedo"]=graph->CanRedo();
            }
        }
        if(machine.startsWith("/Game/")) {
            QFile file(QDir(QString::fromUtf8(host.host().contentRoot)).filePath(machine.mid(6)));
            if(file.open(QIODevice::ReadOnly)){auto bytes=file.readAll();result["file"]=QJsonDocument::fromJson(bytes).object();
                result["fileSha256"]=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
        }
        return result;
    };
    const auto before=snapshot();QJsonArray results;
    const auto steps=in["steps"].toArray();Check(steps.size()<=120,"too many AI editor operations");
    for(const auto entry:steps) {
        const auto step=entry.toObject();const auto action=step["action"].toString();QJsonObject state;
        capture.status=EngineSceneOk;capture.error.clear();
        const auto* raw=editor.data();auto* graph=raw&&raw->IsReady()?editor->Graph():nullptr;
        HANDLE lock=INVALID_HANDLE_VALUE;
        if(step.contains("lockedFile")) {
            const auto path=step["lockedFile"].toString();Check(QFileInfo(path).isFile(),"lock target must exist");
            lock=CreateFileW(reinterpret_cast<const wchar_t*>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            Check(lock!=INVALID_HANDLE_VALUE,"file lock failed");
        }
        struct LockGuard {HANDLE h;~LockGuard(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}} lockGuard{lock};
        if(action=="wait")pump(PointShadowDiagnostics::Integer(step,"milliseconds",1000,1,5000));
        else if(action=="open"||action=="reopen") {
            if(editor){editor->close();pump();editor=nullptr;}
            if(step.contains("machine"))machine=step["machine"].toString();open();
        } else if(action=="close") {if(editor){editor->close();pump();editor=nullptr;}}
        else if(action=="create-file") {machine=main.CreateAiMachine(step["directory"].toString(),step["name"].toString());Check(!machine.isEmpty(),"AI file creation failed");open();}
        else if(action=="select"||action=="property"||action=="clone"||action=="save-scene"||action=="load-scene"||action=="scene-undo"||action=="scene-redo") {
            EditorCommand cmd;cmd.path=step["path"].toString();
            if(step.contains("target"))cmd.objectId=resolve(step["target"].toString());
            if(action=="select")cmd.type=EditorCommand::SelectObject;
            if(action=="clone")cmd.type=EditorCommand::DuplicateObject;
            if(action=="save-scene")cmd.type=EditorCommand::SaveScene;
            if(action=="load-scene")cmd.type=EditorCommand::LoadScene;
            if(action=="scene-undo")cmd.type=EditorCommand::Undo;
            if(action=="scene-redo")cmd.type=EditorCommand::Redo;
            if(action=="property"){cmd.type=EditorCommand::SetProperty;auto bytes=QJsonDocument(QJsonArray{step["value"]}).toJson(QJsonDocument::Compact);cmd.name=QString::fromUtf8(bytes.mid(1,bytes.size()-2));}
            host.PostCommand(cmd);pump(180);
        } else if(action=="button") {
            auto* button=main.findChild<QAbstractButton*>(step["name"].toString());
            Check(button&&button->isEnabled(),"button unavailable");button->click();pump(200);
            if(step.contains("machine")){machine=step["machine"].toString();editor=main.AiMachineEditor(machine);}
        } else if(action=="resize-window") {
            main.resize(PointShadowDiagnostics::Integer(step,"width",1100,640,1920),
                PointShadowDiagnostics::Integer(step,"height",760,480,1080));
            pump(250); // Let the viewport's real resize debounce update its swapchain.
        } else if(action=="viewport-drag") {
            Check(in["nativeViewport"].toBool(),"viewport drag requires editor.scene native mode");
            auto* viewport=main.findChild<ViewportWidget*>();Check(viewport,"viewport absent");
            auto point=[&](const char* xKey,const char* yKey) {
                const double x=PointShadowDiagnostics::Number(step,xKey,.5f),y=PointShadowDiagnostics::Number(step,yKey,.5f);
                Check(x>=0&&x<=1&&y>=0&&y<=1,"viewport drag uses normalized coordinates");
                return QPointF(x*viewport->width(),y*viewport->height());
            };
            const auto from=point("fromX","fromY"),to=point("toX","toY");
            const int moves=PointShadowDiagnostics::Integer(step,"moves",8,1,32);
            const auto buttonName=step["button"].toString("left");
            Check(buttonName=="left"||buttonName=="right"||buttonName=="middle","unknown viewport mouse button");
            const Qt::MouseButton dragButton=buttonName=="right"?Qt::RightButton:buttonName=="middle"?Qt::MiddleButton:Qt::LeftButton;
            const auto modifiers=step["control"].toBool()?Qt::ControlModifier:Qt::NoModifier;
            auto mouse=[&](QEvent::Type type,const QPointF& p,Qt::MouseButton button,Qt::MouseButtons buttons) {
                QMouseEvent event(type,p,QPointF(viewport->mapToGlobal(p.toPoint())),button,buttons,modifiers);
                QApplication::sendEvent(viewport,&event);pump(5);
            };
            mouse(QEvent::MouseButtonPress,from,dragButton,dragButton);
            for(int i=1;i<=moves;++i)mouse(QEvent::MouseMove,from+(to-from)*(double(i)/moves),Qt::NoButton,dragButton);
            mouse(QEvent::MouseButtonRelease,to,dragButton,Qt::NoButton);
        } else if(action=="hierarchy-click"||action=="viewport-click") {
            QWidget* target=nullptr;QPoint point;
            if(action=="hierarchy-click") {
                auto* tree=main.findChild<QTreeWidget*>("SceneHierarchy");Check(tree,"scene tree absent");
                const auto id=resolve(step["target"].toString());QTreeWidgetItem* found=nullptr;
                for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toULongLong()==id){found=*it;break;}
                Check(found,"scene tree item absent");tree->scrollToItem(found);pump();
                point=tree->visualItemRect(found).center();target=tree->viewport();
            } else {
                auto* viewport=main.findChild<ViewportWidget*>();Check(viewport,"viewport absent");
                Check(in["nativeViewport"].toBool(),"viewport clicks require editor.scene native mode");
                target=viewport;const double x=step["x"].toDouble(),y=step["y"].toDouble();
                Check(x>=0&&x<=1&&y>=0&&y<=1,"viewport click uses normalized coordinates");
                point=QPoint(int(x*viewport->width()),int(y*viewport->height()));
            }
            const auto modifiers=step["control"].toBool()?Qt::ControlModifier:Qt::NoModifier;
            QMouseEvent press(QEvent::MouseButtonPress,QPointF(point),QPointF(target->mapToGlobal(point)),Qt::LeftButton,Qt::LeftButton,modifiers);
            QMouseEvent release(QEvent::MouseButtonRelease,QPointF(point),QPointF(target->mapToGlobal(point)),Qt::LeftButton,Qt::NoButton,modifiers);
            QApplication::sendEvent(target,&press);QApplication::sendEvent(target,&release);
        } else if(action=="hud-settings") {
            state["hudDialog"]=HudDialog(main,step);
        } else if(action=="ui-open"||action=="ui-create") {
            const auto path=QFileInfo(step["path"].toString()).absoluteFilePath();
            const auto relative=QDir(QString::fromUtf8(host.host().contentRoot)).relativeFilePath(path);
            Check(QFileInfo(step["path"].toString()).isAbsolute()&&!relative.startsWith("../")&&!QDir::isAbsolutePath(relative),"UI path must be inside project Content");
            if(action=="ui-create")Check(main.CreateUI(path)==path,"UI creation failed");else main.OpenUIEditor(path);
            pump();uiEditor=nullptr;
            for(auto* widget:QApplication::topLevelWidgets())if(auto* candidate=dynamic_cast<UIEditorWindow*>(widget))
                if(QFileInfo(candidate->FilePath()).absoluteFilePath()==path)uiEditor=candidate;
            Check(uiEditor&&uiEditor->Root(),"UI editor unavailable");uiEditor->setAttribute(Qt::WA_DontShowOnScreen);
        } else if(action=="ui-close") {
            Check(uiEditor,"UI editor absent");uiEditor->close();pump();Check(!uiEditor,"UI editor close cancelled");
        } else if(action=="ui-select") {
            Check(uiEditor&&uiEditor->Root(),"UI editor absent");
            auto* element=uiEditor->Root()->Find(step["target"].toString().toStdString());Check(element,"UI element not found");
            auto* tree=uiEditor->findChild<QTreeWidget*>("UIHierarchy");Check(tree,"UI tree absent");QTreeWidgetItem* found=nullptr;
            for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).value<quintptr>()==reinterpret_cast<quintptr>(element)){found=*it;break;}
            Check(found,"UI hierarchy item absent");tree->setCurrentItem(found);
        } else if(action=="ui-add"||action=="ui-action") {
            Check(uiEditor,"UI editor absent");const auto name=action=="ui-add"?"UIAdd"+step["type"].toString():step["name"].toString();
            auto* command=uiEditor->findChild<QAction*>(name);Check(command&&command->isEnabled(),"UI action unavailable");command->trigger();
        } else if(action=="ui-field") {
            Check(uiEditor,"UI editor absent");
            if(step.contains("index")) {
                auto* combo=uiEditor->findChild<QComboBox*>(step["name"].toString());Check(combo&&combo->isEnabled(),"UI combo unavailable");
                combo->setCurrentIndex(PointShadowDiagnostics::Integer(step,"index",0,0,combo->count()-1));
            } else Field(*uiEditor,step["name"].toString(),step["value"]);
        } else if(action=="ui-command") {
            state["accepted"]=CommandRegistry::Instance().Execute(step["id"].toString());
        } else if(action=="scroll") {
            auto* field=main.findChild<QWidget*>(step["name"].toString());Check(field,"scroll target absent");
            auto* parent=field->parentWidget();
            while(parent&&!qobject_cast<QScrollArea*>(parent))parent=parent->parentWidget();
            auto* area=qobject_cast<QScrollArea*>(parent);Check(area,"target has no scroll area");
            area->ensureWidgetVisible(field,8,8);
        } else if(action=="field") {
            QWidget* scope=step["scope"].toString()=="main"?static_cast<QWidget*>(&main):editor.data();
            Check(scope,"field scope unavailable");Field(*scope,step["name"].toString(),step["value"]);
        } else if(action=="play"||action=="stop"||action=="pause"||action=="resume") {
            const auto command=action=="play"?EnginePlay:action=="stop"?EngineStop:action=="pause"?EnginePause:EngineResume;
            state["controlStatus"]=int(module.Api().control(module.Api().context,command));
            if(action=="play"&&module.ViewportInputApi().focus)
                module.ViewportInputApi().focus(module.ViewportInputApi().context,1);
            EditorCommand refresh;refresh.type=EditorCommand::FetchScene;host.PostCommand(refresh);
        } else if(action=="frames") {
            const int count=PointShadowDiagnostics::Integer(step,"count",1,1,2000);
            const float dt=PointShadowDiagnostics::Number(step,"dt",.016f);Check(dt>0&&dt<=.1f,"invalid frame delta");
            for(int i=0;i<count;++i){host.Update(0);const auto status=module.Api().frame(module.Api().context,dt);Check(status==EngineSceneOk,"AI frame failed");QApplication::processEvents();}
        } else if(action=="key") {
            auto api=module.ViewportInputApi();Check(api.version,"input unavailable");
            EditorCommand key;key.type=EditorCommand::InputKey;key.w=uint32_t(PointShadowDiagnostics::Integer(step,"key",69,0,255));key.h=step["down"].toBool(true)?1:0;host.PostCommand(key);
        } else {
            Check(graph,"ready graph required");
            if(action=="dialog-state"||action=="dialog-link")EditDialog(*graph,step);
            else if(action=="add")state["node"]=graph->AddNode(step["type"].toString("Fsm.State"),QPointF(step["x"].toDouble(),step["y"].toDouble()),step["properties"].toObject().toVariantMap());
            else if(action=="state")state["accepted"]=graph->SetNodeProperties(step["node"].toInt(),step["properties"].toObject().toVariantMap());
            else if(action=="connect"||action=="link") {
                EditorCore::GraphConnectionData link;link.fromNode=step["from"].toInt();link.toNode=step["to"].toInt();link.fromPin="out";link.toPin="in";
                bool ok=true;if(action=="connect")ok=graph->Connect(link.fromNode,link.fromPin,link.toNode,link.toPin);
                if(ok&&step.contains("properties"))ok=graph->SetLinkProperties(link,step["properties"].toObject().toVariantMap());state["accepted"]=ok;
            } else if(action=="remove") {QList<int> ids;for(auto id:step["nodes"].toArray())ids<<id.toInt();graph->RemoveNodes(ids);}
            else if(action=="variables") {QHash<QString,QString> renames;const auto map=step["renames"].toObject();for(auto i=map.begin();i!=map.end();++i)renames[i.key()]=i.value().toString();graph->SetVariables(step["values"].toArray(),renames);}
            else if(action=="settings")graph->SetSettings(step["value"].toObject());
            else if(action=="undo")graph->Undo();
            else if(action=="redo")graph->Redo();
            else if(action=="arrange")graph->ArrangeNodes();
            else if(action=="save"||action=="check")editor->SaveNow();
            else throw std::runtime_error("unknown AI editor action");
        }
        pump(lock==INVALID_HANDLE_VALUE?100:1200);
        if(in["nativeViewport"].toBool()&&host.PlayState()==EditorQtHost::Edit) {
            Check(module.Api().frame(module.Api().context,0)==EngineSceneOk,"native editor frame failed");pump();
        }
        if(step.contains("image")) {
            const auto path=step["image"].toString();Check(QFileInfo(path).isAbsolute()&&!QFileInfo::exists(path),"image must be new absolute path");
            QWidget* target=step["scope"].toString()=="main"?static_cast<QWidget*>(&main):step["scope"].toString()=="ui"?static_cast<QWidget*>(uiEditor.data()):editor.data();Check(target,"image target missing");
            if(editor&&target==editor.data())editor->Graph()->FrameNodes();pump();Check(target->grab().save(path),"AI Qt image save failed");
        }
        state["action"]=action;state["status"]=int(capture.status);state["error"]=capture.error;state["snapshot"]=snapshot();results.append(state);
    }
    module.Api().control(module.Api().context,EngineStop);pump();
    return {{"before",before},{"steps",results},{"dialogs",QJsonArray::fromStringList(dialogs)},
        {"observation","Actual themed Qt graph/card and runtime AI extension; widget image excludes D3D viewport."}};
}
}
