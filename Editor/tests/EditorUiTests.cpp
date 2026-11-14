#define NOMINMAX
#include <Windows.h>
#include "QtHost.h"
#include "MainWindow.h"
#include "MaterialEditorWindow.h"
#include "MaterialTextureTools.h"
#include "MaterialGraphWidget.h"
#include "HlslGraphCompiler.h"
#include "UIEditorWindow.h"
#include <Engine/UI/UIMarkup.h>
#include <QDialogButtonBox>
#include "CodeClassDialog.h"
#include "ObjectInspectorPanel.h"
#include "PropertySection.h"
#include <Engine/Runtime/LegacyEditApi.h>
#include <Engine/Runtime/EditExtensions.h>
#include <Engine/Runtime/ProjectDataApi.h>
#include <Engine/Runtime/GameEditModule.h>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QTimer>
#include <QApplication>
#include <QAbstractEventDispatcher>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QScreen>
#include <QTextCursor>
#include <QAction>
#include <QComboBox>
#include <QColorDialog>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QImage>
#include <QPixmap>
#include <QDockWidget>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QClipboard>
#include <QFrame>
#include <QStyle>
#include <QLineEdit>
#include <QToolButton>
#include <QGroupBox>
#include <QTreeView>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include "ViewportWidget.h"
#include "SceneObjectsPanel.h"
#include "CollapsibleCard.h"
#ifndef QT_WIDGETS_LIB
#define QT_WIDGETS_LIB
#endif
#include <qtestmouse.h>
#include <qtestkeyboard.h>
#include <QTabBar>
#include <QMenu>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include "ViewportDpiTests.h"
#include "ViewportCompletionTests.h"
#include "GizmoCompletionTests.h"
#include "ContentBrowserTests.h"
#include "ContentBrowserTypeTests.h"
#include "FolderCreationTests.h"
namespace
{
int checks = 0;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); ++checks; if(qEnvironmentVariableIsSet("ENGINE_UI_TEST_TRACE")) { std::printf("CHECK %d: %s\n",checks,message);std::fflush(stdout); } }
void Ui(const std::function<void(EditorMainWindow*)>& action)
{
    std::exception_ptr failure;
    QMetaObject::invokeMethod(QCoreApplication::instance(), [&] {
        try
        {
            for (auto* widget : QApplication::topLevelWidgets())
                if (auto* window = qobject_cast<EditorMainWindow*>(widget)) { action(window); return; }
            throw std::runtime_error("Editor window not found");
        }
        catch (...) { failure = std::current_exception(); }
    }, Qt::BlockingQueuedConnection);
    if (failure) std::rethrow_exception(failure);
}
QAction* Action(EditorMainWindow* window, const QString& label)
{
    QString id;
    if(label==QStringLiteral("▶ Play")) id="ActPlay";
    if(label==QStringLiteral("⏹ Stop")) id="ActStop";
    if(label==QStringLiteral("⏸ Pause")) id="ActPause";
    if(!id.isEmpty()) if(auto* a=window->findChild<QAction*>(id)) return a;
    QString text=label;
    if(text==QStringLiteral("Сбросить расположение панелей")) text=QStringLiteral("Сбросить раскладку окон");
    for (auto* action : window->findChildren<QAction*>()) if (action->text() == text) return action;
    throw std::runtime_error("Action not found: " + label.toStdString());
}
QAction* CreateAction(EditorMainWindow* window, const QString& type) {
    auto* action=window->findChild<QAction*>("CreateObject."+type);
    Check(action!=nullptr, "object type is available in categorized add menu"); return action;
}
void Add(EditorMainWindow* window, int index) {
    auto* panel=window->findChild<SceneObjectsPanel*>();Check(panel && panel->isEnabled(), "add palette enabled");
    QList<QAction*> actions;
    for(auto* action:panel->findChildren<QAction*>()) if(action->objectName().startsWith("CreateObject.")) actions.push_back(action);
    Check(actions.size()==14,"categorized add menu includes all placeable types");
    const QStringList types={"Primitive.Cube","Primitive.Sphere","Primitive.Plane","Primitive.Cylinder","Primitive.Cone","Primitive.Pyramid","Light.Point","Light.Directional","Particle.Sparks","Particle.Smoke","Environment.Sky","Gameplay.Character","Gameplay.Camera","Gameplay.Player"};
    Check(index>=0 && index<types.size(),"valid object menu index");CreateAction(window,types[index])->trigger();
}
}
void ReloadUiChecks(const std::filesystem::path& root, const std::filesystem::path& engineContent, bool buildCode=false)
{
    const auto manifest=(root/"Project.json").u8string(), content=(root/"Content").u8string();
    auto initialModule=root/"Game.dll";
    if(buildCode){std::ifstream input(root/"Project.json",std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(input),{}};initialModule=root/std::filesystem::u8path(QJsonDocument::fromJson(QByteArray::fromStdString(bytes)).object()["module"].toString().toStdString());}
    auto ownedModule=std::make_unique<GameEditModule>(initialModule,engineContent,root/"Content");
    auto& module=*ownedModule;
    char error[1024]{};uint64_t id=0;
    Check(module.Api().loadScene(module.Api().context,(root/"Content/Scenes/Main.scene.json").u8string().c_str(),&id,error,sizeof(error))==EngineSceneOk,"reload UI scene");
    struct Context { bool exit=false; GameEditModule* module; EngineEditorReloadApi service{}; } context{false,&module};
    context.service={1,&context,[](void* p,const char* path,const char* checkpoint,void* hwnd,uint32_t w,uint32_t h,EngineEditorReloadBindings* output,char* error,uint32_t capacity)->uint32_t {
        auto& module=*static_cast<Context*>(p)->module;
        const auto result=module.Reload(std::filesystem::u8path(path),std::filesystem::u8path(checkpoint),hwnd,w,h,error,capacity);
        if(result==EngineSceneOk)*output={module.Api(),module.ProjectDataApi(),module.SelectionApi(),module.ViewportInputApi()};
        return result;
    }};
    EngineModuleHost callbacks{ENGINE_MODULE_VERSION,&context,[](void*,const char* text){std::puts(text);},[](void* p){static_cast<Context*>(p)->exit=true;},"Reload UI",manifest.c_str(),content.c_str()};
    callbacks.getService=[](void* p,uint32_t service,uint32_t version)->const void* {
        auto& c=*static_cast<Context*>(p);if(version>1)return nullptr;
        if(service==EngineEditorReloadService)return &c.service;
        if(service==EngineViewportOverlayService)return &c.module->ViewportOverlayApi();
        if(service==EngineViewportCursorService)return &c.module->ViewportCursorApi();
        if(service==EngineEditorBatchService)return &c.module->BatchApi();
        if(service==EngineAuthoringService)return &c.module->AuthoringApi();
        return nullptr;
    };
    EditorQtHost host(callbacks);host.SetModuleApi(module.Api());host.SetProjectDataApi(module.ProjectDataApi());host.SetSelectionApi(module.SelectionApi());host.SetViewportInputApi(module.ViewportInputApi());host.SetViewportCursorApi(module.ViewportCursorApi());
    struct Cleanup { EditorQtHost& host; std::unique_ptr<GameEditModule>& module; ~Cleanup(){host.SetModuleApi({});module.reset();host.Stop();} } cleanup{host,ownedModule};
    Check(host.Start(),"start reload UI");
    auto pump=[&]{host.Update(.016);Check(module.Api().frame(module.Api().context,.016)==EngineSceneOk,"reload UI render");Sleep(10);};
    int reloadStep=0;
    auto until=[&](const std::function<bool()>& predicate){const int step=++reloadStep;const auto end=GetTickCount64()+10000;while(!predicate()){if(GetTickCount64()>end){QString error;Ui([&](auto*){if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))error=box->text();});throw std::runtime_error("Reload UI timeout step "+std::to_string(step)+": "+error.toStdString());}pump();}for(int i=0;i<30;++i)pump();};
    until([&]{return host.ViewportActive();});
    const auto selected=host.ReadSnapshot().objects.front();
    EditorCommand select;select.type=EditorCommand::SelectObject;select.objectId=selected.id;host.PostCommand(select);pump();
    EditorCommand move;move.type=EditorCommand::UpdateObjectTransform;move.objectId=selected.id;move.transform=selected.transform;move.transform.position[0]=8;host.PostCommand(move);
    until([&]{return host.ReadSnapshot().dirty;});
    EditorMainWindow* windowBefore=nullptr;Ui([&](auto* w){windowBefore=w;});
    if(buildCode){
        const auto previous=module.Api().context;
        Ui([](auto* w){Action(w,QStringLiteral("Перекомпилировать / Hot Reload"))->trigger();});
        const auto deadline=GetTickCount64()+180000;
        while(module.Api().context==previous){
            QString failure;Ui([&](auto* w){if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){failure=box->text();if(auto* console=w->findChild<QPlainTextEdit*>("Console"))failure+="\n"+console->toPlainText();box->accept();}});
            if(!failure.isEmpty())throw std::runtime_error(failure.toStdString());
            Check(GetTickCount64()<deadline,"real code build timeout");pump();
        }
        for(int i=0;i<30;++i)pump();
        Ui([&](auto* w){Check(w==windowBefore,"real build preserves editor window");Check(CreateAction(w,"Spawner"),"new Spawner visible after real Hot Reload");});
        const auto view=host.ReadSnapshot();bool preserved=false;for(const auto& o:view.objects)if(o.id==view.selected)preserved=o.name==selected.name&&o.transform.position[0]==8;
        Check(view.dirty&&!view.canUndo&&preserved,"real build preserves unsaved scene and selection");
        Ui([](auto* w){w->close();});until([&]{bool modal=false;Ui([&](auto*){modal=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())!=nullptr;});return modal;});
        Ui([](auto*){qobject_cast<QMessageBox*>(QApplication::activeModalWidget())->button(QMessageBox::Discard)->click();});until([&]{return context.exit;});
        host.SetModuleApi({});ownedModule.reset();host.Stop();std::printf("Real code Hot Reload: %d checks passed\n",checks);return;
    }
    const auto candidate=root/"ReloadCandidate.dll";std::filesystem::copy_file(root/"Game.dll",candidate,std::filesystem::copy_options::overwrite_existing);
    const auto old=module.Api().context;EditorCommand reload;reload.type=EditorCommand::ReloadProject;reload.path=QString::fromStdWString(candidate.wstring());host.PostCommand(reload);
    until([&]{return module.Api().context!=old;});
    Ui([&](auto* w){Check(w==windowBefore,"reload keeps same editor window");});
    auto view=host.ReadSnapshot();Check(view.dirty&&!view.canUndo&&view.selected,"dirty scene preserved and history reset");
    bool preserved=false;for(const auto& o:view.objects)if(o.id==view.selected)preserved=o.name==selected.name&&o.transform.position[0]==8;
    Check(preserved,"reload UI selection and transform remapped");
    move.objectId=view.selected;move.transform.position[0]=9;host.PostCommand(move);until([&]{return host.ReadSnapshot().canUndo;});
    host.PostCommand(reload);for(int i=0;i<60;++i)pump();Check(host.ReadSnapshot().canUndo,"unchanged build preserves undo");
    const auto active=module.Api().context;reload.path=QString::fromStdWString((root/"Missing.dll").wstring());host.PostCommand(reload);
    until([&]{bool modal=false;Ui([&](auto*){modal=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())!=nullptr;});return modal;});
    Check(module.Api().context==active&&host.ReadSnapshot().dirty,"failed reload keeps live session and edits");
    Ui([](auto*){qobject_cast<QMessageBox*>(QApplication::activeModalWidget())->accept();});
    Ui([](auto* w){Action(w,QStringLiteral("▶ Play"))->trigger();});until([&]{return host.PlayState()==EditorQtHost::Play;});
    Ui([](auto* w){Action(w,QStringLiteral("⏹ Stop"))->trigger();});until([&]{return host.PlayState()==EditorQtHost::Edit;});
    Ui([](auto* w){w->close();});until([&]{bool modal=false;Ui([&](auto*){modal=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())!=nullptr;});return modal;});
    Ui([](auto*){qobject_cast<QMessageBox*>(QApplication::activeModalWidget())->button(QMessageBox::Discard)->click();});until([&]{return context.exit;});
    host.SetModuleApi({}); ownedModule.reset(); host.Stop();
    std::printf("Reload EditorUiTests: %d checks passed\n",checks);
}
int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc == 2 && std::wstring(argv[1]) == L"--viewport-dpi")
        {
            int qtArgc = 1;
            char name[] = "ViewportDpiTests";
            char* qtArgv[] = {name, nullptr};
            QApplication app(qtArgc, qtArgv);
            ViewportDpiTests::Run();
            return 0;
        }
        Check(argc == 3 || argc == 4, "usage: EditorUiTests <project directory> <EngineContent> [model]");
        const auto root = std::filesystem::absolute(argv[1]);
        // Isolate persisted UI preferences from the user's registry and other test runs.
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,QString::fromStdWString((root/"TestSettings").wstring()));
        if(argc==4&&std::wstring(argv[3])==L"--reload"){ReloadUiChecks(root,std::filesystem::absolute(argv[2]));return 0;}
        if(argc==4&&std::wstring(argv[3])==L"--code-reload"){ReloadUiChecks(root,std::filesystem::absolute(argv[2]),true);return 0;}
        const auto manifest = (root / "Project.json").u8string(), content = (root / "Content").u8string();
        const auto engineContent = std::filesystem::absolute(argv[2]).u8string();
        const bool propertiesProject = argc == 4 && std::wstring(argv[3]) == L"--properties";
        const bool codeProject = propertiesProject || (argc == 4 && std::wstring(argv[3]) == L"--code");
        std::filesystem::path modulePath = root / "Game.dll";
        if (codeProject) {
            // Do not create QObjects before the host starts its Qt GUI thread.
            std::ifstream project(root / "Project.json", std::ios::binary);
            Check(project.is_open(),"read code project");
            const std::string bytes{std::istreambuf_iterator<char>(project), std::istreambuf_iterator<char>()};
            modulePath = root / std::filesystem::u8path(QJsonDocument::fromJson(QByteArray::fromStdString(bytes)).object().value("module").toString().toUtf8().constData());
        }
        HMODULE library = LoadLibraryExW(modulePath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        Check(library != nullptr, "load project module");
        struct LibraryGuard { HMODULE module; ~LibraryGuard() { FreeLibrary(module); } } libraryGuard{library};
        auto get = reinterpret_cast<EngineGetEditModuleApiFn>(GetProcAddress(library, "EngineGetEditModuleApi"));
        EngineEditModuleApi factory{}; EngineEditSessionApi api{};
        const bool legacy = argc == 4 && std::wstring(argv[3]) == L"--legacy";
        EngineLegacyV4::Factory oldFactory{}; EngineLegacyV4::EditApi oldSession{};
        if (legacy)
        {
            auto oldGet = reinterpret_cast<EngineLegacyV4::GetFactory>(get);
            Check(oldGet && oldGet(4,sizeof(oldFactory),&oldFactory) && oldFactory.create(engineContent.c_str(),&oldSession) &&
                EngineLegacyV4::Complete(oldSession),"create frozen v4 UI session");
            api = EngineLegacyV4::Adapt(oldSession); factory.destroy = oldFactory.destroy;
        }
        else Check(get && get(ENGINE_EDIT_API_VERSION, sizeof(factory), &factory) && factory.create(engineContent.c_str(), &api), "create edit session");
        struct SessionGuard { EngineEditModuleApi factory; void* context; ~SessionGuard() { if (context) factory.destroy(context); } } sessionGuard{factory, legacy ? oldSession.context : api.context};
        char error[1024]{}; uint64_t scene = 0;
        Check(api.setContentRoot(api.context, content.c_str(), error, sizeof(error)) == EngineSceneOk, "configure UI project content");
        const auto sceneFile = root / "Content/Scenes/Main.scene.json";
        Check(api.loadScene(api.context, sceneFile.u8string().c_str(), &scene, error, sizeof(error)) == EngineSceneOk, "load UI fixture");
        bool exit = false;
        EngineModuleHost callbacks{ENGINE_MODULE_VERSION, &exit,
            [](void*, const char* text) { std::puts(text); }, [](void* p) { *static_cast<bool*>(p) = true; },
            "UI test", manifest.c_str(), content.c_str()};
        EditorQtHost host(callbacks); host.SetModuleApi(api);
        EngineProjectDataApi projectData{};
        auto query = reinterpret_cast<EngineGetEditExtensionFn>(GetProcAddress(library,"EngineGetEditExtension"));
        if (!legacy && query && query(api.context,EngineProjectDataExtension,1,sizeof(projectData),&projectData)) host.SetProjectDataApi(projectData);
        EngineSceneBatchApi batch{};
        if (!legacy && query && query(api.context,EngineSceneBatchExtension,1,sizeof(batch),&batch)) host.SetBatchApi(batch);
        EngineAuthoringApi authoring{};
        if (!legacy && query && query(api.context,EngineAuthoringExtension,1,sizeof(authoring),&authoring)) host.SetAuthoringApi(authoring);
        EngineSelectionApi selection{};
        if (!legacy && query && query(api.context,EngineSelectionExtension,1,sizeof(selection),&selection)) host.SetSelectionApi(selection);
        EngineViewportOverlayApi overlay{};
        if (!legacy && query && query(api.context,EngineViewportOverlayExtension,1,sizeof(overlay),&overlay)) host.SetViewportOverlayApi(overlay);
        EngineViewportCursorApi cursor{};if(!legacy&&query&&query(api.context,EngineViewportCursorExtension,1,sizeof(cursor),&cursor))host.SetViewportCursorApi(cursor);
        if(!legacy&&cursor.set){EngineViewportCursorApi rejected{99,nullptr,nullptr};
            Check(!query(api.context,EngineViewportCursorExtension,2,sizeof(rejected),&rejected)&&rejected.version==99,"cursor extension version rejected");
            Check(!query(api.context,EngineViewportCursorExtension,1,sizeof(rejected)-1,&rejected)&&rejected.version==99,"cursor extension size rejected");}
        EngineViewportInputApi viewportInput{};
        if (!legacy && query && query(api.context,EngineViewportInputExtension,1,sizeof(viewportInput),&viewportInput)) host.SetViewportInputApi(viewportInput);
        struct HostCleanup
        {
            EditorQtHost& host; EngineEditModuleApi factory; void*& context;
            ~HostCleanup() { host.SetModuleApi({}); if (context) { factory.destroy(context); context = nullptr; } host.Stop(); }
        } hostCleanup{host, factory, sessionGuard.context};
        if (argc == 4 && std::wstring(argv[3]) == L"--recovery-check") {
            const auto candidates = RecoveryStore::Candidates(QString::fromStdString(manifest));
            Check(!candidates.isEmpty(),"abruptly terminated editor has a recovery record");
            const auto info=RecoveryStore::Read(candidates.front());
            const auto saved=QFileInfo(candidates.front()).dir().filePath(info["snapshot"].toString());
            Check(api.loadScene(api.context,saved.toUtf8().constData(),&scene,error,sizeof(error))==EngineSceneOk,"recover actual scene through DLL");
            size_t count=0; EngineSceneInfo sceneInfo{};
            api.readActiveScene(api.context,&sceneInfo,[](void* p,const EngineSceneObjectInfo*){++*static_cast<size_t*>(p);},&count);
            Check(count==7,"all unsaved objects recovered");
            Check(!info["moduleSha256"].toString().isEmpty(),"recovery records game build identity");
            std::puts("Recovery process-restart checks passed"); return 0;
        }
        Check(host.Start(), "start Qt editor");
        Ui([](auto* window) {
            Check(QCoreApplication::eventDispatcher() == QAbstractEventDispatcher::instance(),
                  "Qt main event dispatcher belongs to the editor UI thread");
            QEvent event(QEvent::User);
            QApplication::sendEvent(window, &event);
        });
        auto pump = [&] { host.Update(0.016); Check(api.frame(api.context, 0.016) == EngineSceneOk, "editor render frame"); Sleep(10); };
        int waitIndex = 0;
        auto until = [&](const std::function<bool()>& predicate) {
            const int step = ++waitIndex;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
            while (!predicate())
            {
                if (std::chrono::steady_clock::now() > deadline)
                {
                    QString modal;
                    Ui([&](auto*) { if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) modal = box->text(); });
                    throw std::runtime_error("UI condition timed out at step " + std::to_string(step) + ": " + modal.toStdString());
                }
                pump();
            }
            // Allow the queued completion and the UI refresh timer to catch up.
            for (int i = 0; i < 30; ++i) pump();
        };
        until([&] { return host.ViewportActive(); });
        Ui([](auto* window) { ViewportCompletionTests::Run(window); });
        Ui([](auto* window) { GizmoCompletionTests::Run(window); });
        if (argc == 4 && std::wstring(argv[3]) == L"--viewport-completion") return 0;
        Ui([](auto* window) { ContentBrowserTests::RunNative(window); });
        if (argc == 4 && std::wstring(argv[3]) == L"--content-browser") return 0;
        Ui([](auto* window) { ContentBrowserTypeTests::RunNative(window); });
        if (argc == 4 && std::wstring(argv[3]) == L"--file-types") return 0;
        Ui([](auto* window) { FolderCreationTests::RunNative(window); });
        if (argc == 4 && std::wstring(argv[3]) == L"--create-folder") return 0;
        if (argc == 4 && std::wstring(argv[3]) == L"--hud-restart") {
            std::fprintf(stderr,"HUD restart: viewport ready\n");
            Ui([&](auto* w) {
                QTimer::singleShot(60,w,[]{
                    auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    std::fprintf(stderr,"HUD restart dialog: %s\n",dialog?dialog->windowTitle().toUtf8().constData():"none");
                    Check(dialog,"HUD settings after process restart");
                    auto* choice=dialog->findChild<QComboBox*>("ProjectHudChoice");
                    Check(choice&&choice->currentData().toString()=="UI/EditorHud.ui","HUD selection persists across process restart");
                    choice->setCurrentIndex(choice->findData(QString()));
                    std::fprintf(stderr,"HUD restart selection: %d '%s'\n",choice->currentIndex(),choice->currentData().toString().toUtf8().constData());
                    dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
                });
                w->ShowHudSettings();
                std::fprintf(stderr,"HUD restart: saved None\n");
                QFile setting(QString::fromStdWString((root/"Content/ProjectHud.txt").wstring()));
                Check(setting.open(QIODevice::ReadOnly)&&setting.readAll().trimmed().isEmpty(),"None saved through HUD settings");
                QTimer::singleShot(60,w,[]{
                    auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    Check(dialog->findChild<QComboBox*>("ProjectHudChoice")->currentData().toString().isEmpty(),"None persists on reopening");
                    dialog->reject();
                });
                w->ShowHudSettings();
            });
            std::fprintf(stderr,"HUD restart: settings checked\n");
            Ui([](auto* w){Action(w,QStringLiteral("▶ Play"))->trigger();});
            until([&]{return host.PlayState()==EditorQtHost::Play;});
            Ui([](auto* w){Action(w,QStringLiteral("⏹ Stop"))->trigger();});
            until([&]{return host.PlayState()==EditorQtHost::Edit;});
            std::fprintf(stderr,"HUD restart: Play/Stop checked\n");
            Ui([](auto* w){w->close();});until([&]{return exit;});
            host.SetModuleApi({}); factory.destroy(api.context); sessionGuard.context=nullptr; host.Stop();
            std::printf("HUD restart/None: %d checks passed\n",checks);return 0;
        }
        Ui([](auto* w){
            ObjectInspectorPanel panel(w);EditorQtHost::Snapshot view;
            EditorQtHost::SceneObject object{};object.id=123;object.name="Property probe";object.type="UiProbe";
            object.projectData=R"({"version":1,"speed":5,"enabled":true,"mode":"walk","target":123,"asset":"/Game/pixel.bmp","materialSlots":{"4":"/Game/Roof.mat"}})";
            object.propertySchema=R"({"version":1,"fields":[{"path":"/speed","type":"number","group":"Behavior/Movement","min":0,"max":20},{"path":"/enabled","type":"bool","group":"Behavior/Movement"},{"path":"/mode","type":"enum","options":[{"label":"Walk","value":"walk"}]},{"path":"/target","type":"object"},{"path":"/asset","type":"asset"},{"path":"/materialSlots/4","type":"asset","extensions":[".mat"]}]})";
            view.objects.push_back(object);panel.Refresh(view,123,true);
            auto* number=panel.findChild<QDoubleSpinBox*>("Property/speed");auto* check=panel.findChild<QCheckBox*>("Property/enabled");
            auto* mode=panel.findChild<QComboBox*>("Property/mode");auto* target=panel.findChild<QComboBox*>("Property/target");
            Check(number&&number->value()==5&&number->maximum()==20,"typed numeric value and bounds");
            Check(check&&check->isChecked(),"typed boolean");Check(mode&&mode->currentText()=="Walk","typed enum");Check(target&&target->currentText()=="Property probe","typed object reference");
            Check(panel.findChild<QLineEdit*>("Property/asset")->text()=="/Game/pixel.bmp","typed asset path");
            auto* slotEdit=panel.findChild<QPushButton*>("EditMaterial/materialSlots/4");
            Check(slotEdit && slotEdit->isEnabled(),"material slot has an enabled editor button");
            QString editedMaterial;QObject::connect(&panel,&ObjectInspectorPanel::EditMaterialRequested,&panel,[&](const QString& path){editedMaterial=path;});
            slotEdit->click();Check(editedMaterial=="/Game/Roof.mat","material slot button opens its own asset");
            auto* slotPath=panel.findChild<QLineEdit*>("Property/materialSlots/4");
            slotPath->setText("");Check(!slotEdit->isEnabled(),"empty material slot cannot open material editor");
            QString path,value;quint64 selected=0;QObject::connect(&panel,&ObjectInspectorPanel::PropertyEditingFinished,&panel,[&](auto id,const auto& p,const auto& v){selected=id;path=p;value=v;});
            number->setValue(7);QMetaObject::invokeMethod(number,"editingFinished");Check(selected==123&&path=="/speed"&&value=="7","typed property signal contains scalar JSON");
            auto* header=panel.findChild<QToolButton*>("Section.UiProbe/Behavior/Movement");Check(header!=nullptr,"nested collapsible section");
            const bool wasOpen=header->isChecked();header->setChecked(false);
            Check(static_cast<PropertySection*>(header->parentWidget())->body->isHidden(),"collapse hides section body");
            panel.Refresh(view,123,false);Check(panel.findChild<QDoubleSpinBox*>("Property/speed")==number&&!header->isChecked(),"refresh preserves widget and collapse state");
            {ObjectInspectorPanel restored(w);restored.Refresh(view,123,true);Check(!restored.findChild<QToolButton*>("Section.UiProbe/Behavior/Movement")->isChecked(),"collapsed hierarchy persists across inspector recreation");}
            header->setChecked(wasOpen);
        });
        if(argc==4&&std::wstring(argv[3])==L"--layout") {
            int height=0;QPoint boundary; Ui([&](auto* w){
                QDockWidget* files=nullptr;for(auto* d:w->template findChildren<QDockWidget*>())if(d->objectName()==QStringLiteral("ContentDock"))files=d;
                Check(files!=nullptr,"content dock exists");files->raise();w->resize(1600,950);
                for (auto* bar : w->findChildren<QTabBar*>()) {
                    if (!bar->isVisible() || bar->count() < 2) continue;
                    for (int c = 0; c < 5; ++c) {
                        QTest::mouseClick(bar, Qt::LeftButton, Qt::KeyboardModifiers(), bar->tabRect(0).center()); Check(bar->currentIndex() == 0, "tab switched to Console");
                        QTest::mouseClick(bar, Qt::LeftButton, Qt::KeyboardModifiers(), bar->tabRect(1).center()); Check(bar->currentIndex() == 1, "tab switched to Content");
                    }
                    std::printf("[TabDiag] 5 consecutive switch cycles succeeded!\n"); }
            });for(int i=0;i<15;++i)pump();
            Ui([](auto* w){w->resize(1800,1100);});for(int i=0;i<15;++i)pump();
            Ui([](auto* w){w->resizeDocks({w->template findChild<QDockWidget*>("SceneObjectsDock"),w->template findChild<QDockWidget*>("ViewportDock"),w->template findChild<QDockWidget*>("InspectorDock")},{80,1600,80},Qt::Horizontal);});
            for(int i=0;i<10;++i)pump();
            Ui([](auto* w){Check(w->template findChild<QDockWidget*>("ViewportDock")->width()>w->width()*0.75,"viewport expands beyond 75 percent of window width");});
            Ui([&](auto* w){QDockWidget* files=nullptr;for(auto* d:w->template findChildren<QDockWidget*>())if(d->objectName()==QStringLiteral("ContentDock"))files=d;
                height=files->height();std::printf("Layout resized: content=%d\n",height);for(auto* d:w->findChildren<QDockWidget*>())std::printf("Dock %s: area=%d size=%dx%d min=%d visible=%d\n",d->objectName().toUtf8().constData(),int(w->dockWidgetArea(d)),d->width(),d->height(),d->minimumHeight(),d->isVisible());                boundary=QPoint(w->width()/2,files->mapTo(w,QPoint()).y()-w->style()->pixelMetric(QStyle::PM_DockWidgetSeparatorExtent)/2);
                QMouseEvent press(QEvent::MouseButtonPress,boundary,w->mapToGlobal(boundary),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(w,&press);
            });for(int i=0;i<3;++i)pump();
            const uint64_t beforeDrag = host.ViewportResizeCount();
            for (int s = 1; s <= 50; ++s) { Ui([&](auto* w) { const auto pt = boundary + QPoint(0, -std::min(90, s * 2)); QMouseEvent move(QEvent::MouseMove, pt, w->mapToGlobal(pt), Qt::NoButton, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(w, &move); }); Sleep(40); pump(); }
            Ui([&](auto* w) { const auto end = boundary + QPoint(0, -90); QMouseEvent rel(QEvent::MouseButtonRelease, end, w->mapToGlobal(end), Qt::LeftButton, Qt::NoButton, Qt::NoModifier); QApplication::sendEvent(w, &rel); }); for (int i = 0; i < 15; ++i) pump();
            const uint64_t diff = host.ViewportResizeCount() - beforeDrag;
            std::printf("ResizeDiag during 2s drag: diff=%llu\n", diff); Check(diff >= 1 && diff <= 2, "viewportResize called <= 2 times during continuous 2s splitter drag");
            Ui([&](auto* w){QDockWidget* files=nullptr;for(auto* d:w->template findChildren<QDockWidget*>())if(d->objectName()==QStringLiteral("ContentDock"))files=d;std::printf("Layout dragged: content=%d (before %d)\n",files->height(),height);Check(files->height()>height+40,"content separator drag resizes panel");
                files->setFloating(true);Check(files->isFloating(),"content floats");w->addDockWidget(Qt::LeftDockWidgetArea,files);files->setFloating(false);Check(w->dockWidgetArea(files)==Qt::LeftDockWidgetArea,"content can dock at side");
                const auto layout=w->saveState(1);Action(w,QStringLiteral("Сбросить расположение панелей"))->trigger();Check(!files->isFloating()&&w->dockWidgetArea(files)==Qt::BottomDockWidgetArea,"reset returns content below viewport");
                Check(w->restoreState(layout,1)&&w->dockWidgetArea(files)==Qt::LeftDockWidgetArea,"panel layout restores by stable names");
            });
            Ui([](auto* w){w->showMaximized();});for(int i=0;i<5;++i)pump();
            Ui([](auto* w){w->showNormal();});for(int i=0;i<5;++i)pump();
            Ui([](auto* w){auto* vpDock=w->template findChild<QDockWidget*>("ViewportDock");Check(vpDock!=nullptr,"viewport dock exists");vpDock->setFloating(true);});for(int i=0;i<5;++i)pump();
            Ui([](auto* w){auto* vpDock=w->template findChild<QDockWidget*>("ViewportDock");w->addDockWidget(Qt::TopDockWidgetArea,vpDock);vpDock->setFloating(false);});for(int i=0;i<5;++i)pump();
            Ui([&](auto* w){auto* modal=QApplication::activeModalWidget();bool isError=false;
                if(auto* box=qobject_cast<QMessageBox*>(modal))if(box->icon()==QMessageBox::Critical||box->icon()==QMessageBox::Warning||box->windowTitle().contains(QStringLiteral("Ошибка"))||box->text().contains(QStringLiteral("код 1")))isError=true;
                Check(!isError&&modal==nullptr,"no error modal dialog during maximize/restore/drag/undock");
                Action(w,QStringLiteral("Сбросить расположение панелей"))->trigger();w->grab().save(QString::fromStdWString((root/"editor-layout.png").wstring()));w->close();});until([&]{return exit;});host.SetModuleApi({});factory.destroy(api.context);sessionGuard.context=nullptr;host.Stop();
            std::printf("Layout EditorUiTests: %d checks passed\n",checks);return 0;
        }
        if(propertiesProject) {
            EditorCommand add;add.type=EditorCommand::CreateObject;add.path="RotatingCube";add.name="PropertyCube";host.PostCommand(add);
            auto value=[&](){for(const auto& o:host.ReadSnapshot().objects)if(o.name=="PropertyCube")return QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object()["speed"].toDouble(-1);return -1.0;};
            until([&]{return value()==90;});
            Ui([](auto* w){auto* spin=w->template findChild<QDoubleSpinBox*>("Property/speed");Check(spin&&spin->value()==90,"generated class property appears in inspector");spin->setValue(180);QMetaObject::invokeMethod(spin,"editingFinished");});
            until([&]{return value()==180;});
            bool brightness=false;Ui([&](auto* w){if(auto* field=w->template findChild<QDoubleSpinBox*>("Property/brightness")){brightness=true;field->setValue(0.25);QMetaObject::invokeMethod(field,"editingFinished");}});
            if(brightness){
                until([&]{for(const auto& o:host.ReadSnapshot().objects)if(o.name=="PropertyCube")return QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object()["brightness"].toDouble()==0.25;return false;});
                Ui([&](auto* w){w->grab().save(QString::fromStdWString((root/"tutorial-properties.png").wstring()));Action(w,QStringLiteral("Отменить"))->trigger();});
                until([&]{for(const auto& o:host.ReadSnapshot().objects)if(o.name=="PropertyCube")return QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object()["brightness"].toDouble()==1;return false;});
            }
            Ui([](auto* w){Action(w,QStringLiteral("Отменить"))->trigger();});until([&]{return value()==90;});
            Ui([](auto* w){Action(w,QStringLiteral("Повторить"))->trigger();});until([&]{return value()==180;});
            Ui([](auto* w){Action(w,QStringLiteral("Сохранить сцену"))->trigger();});until([&]{return !host.ReadSnapshot().dirty;});
            Ui([](auto* w){Action(w,QStringLiteral("▶ Play"))->trigger();});until([&]{return host.PlayState()==EditorQtHost::Play;});Check(value()==180,"typed value survives Play copy");
            Ui([](auto* w){Action(w,QStringLiteral("⏹ Stop"))->trigger();});until([&]{return host.PlayState()==EditorQtHost::Edit;});Check(value()==180,"Stop preserves authoring value");
            Ui([](auto* w){w->close();});until([&]{return exit;});host.SetModuleApi({});factory.destroy(api.context);sessionGuard.context=nullptr;host.Stop();
            std::printf("Properties EditorUiTests: %d checks passed\n",checks);return 0;
        }
        if(argc==4&&std::wstring(argv[3])==L"--authoring") {
            Check(authoring.context!=nullptr,"authoring extension exposed");
            const auto source=root/"Reimport.bmp";QImage original(2,2,QImage::Format_RGB32);original.fill(Qt::red);Check(original.save(QString::fromStdWString(source.wstring())),"write reimport source");
            EditorCommand import;import.type=EditorCommand::ImportTexture;import.path=QString::fromStdWString(source.wstring());import.w=1;host.PostCommand(import);
            const auto asset=root/"Content/Textures/Reimport.bmp";
            until([&]{return std::filesystem::exists(asset);});
            original.fill(Qt::blue);Check(original.save(QString::fromStdWString(source.wstring())),"change reimport source");
            Ui([&](auto* w){w->UpdateAsset(QString::fromStdWString(asset.wstring()),false,QString::fromStdWString(source.wstring()));});
            until([&]{return QImage(QString::fromStdWString(asset.wstring())).pixelColor(0,0)==QColor(Qt::blue);});
            Check(host.ReadSnapshot().dirty&&!host.ReadSnapshot().canUndo,"asset update marks dirty and clears stale undo");
            Ui([&](auto* w){w->UpdateAsset(QString::fromStdWString(asset.wstring()),true,"Images/Renamed.bmp");});
            until([&]{return std::filesystem::exists(root/"Content/Images/Renamed.bmp");});
            Check(!std::filesystem::exists(asset),"asset move replaces old location");
            Ui([](auto* w){Action(w,QStringLiteral("Сохранить сцену"))->trigger();});until([&]{return !host.ReadSnapshot().dirty;});
            Ui([](auto* w){w->close();});until([&]{return exit;});
            host.SetModuleApi({});factory.destroy(api.context);sessionGuard.context=nullptr;host.Stop();
            std::printf("Authoring EditorUiTests: %d checks passed\n",checks);return 0;
        }
        if (argc == 4 && std::wstring(argv[3]) == L"--workflow") {
            for(int i=0;i<3;++i){EditorCommand add;add.type=EditorCommand::CreateObject;add.path="Primitive.Cube";add.name=QString("Batch %1").arg(i);host.PostCommand(add);}
            until([&]{return host.ReadSnapshot().objects.size()==7;});
            auto view=host.ReadSnapshot();std::vector<uint64_t> ids;
            for(const auto& o:view.objects)if(o.name.find("Batch ")==0)ids.push_back(o.id);
            Check(ids.size()==3,"three group objects");
            EditorCommand select;select.type=EditorCommand::SelectObject;select.objectIds=ids;select.objectId=ids.front();host.PostCommand(select);
            until([&]{return host.ReadSnapshot().selectedIds.size()==3;});
            Ui([&](auto* w) {
                auto* tree=w->template findChild<QTreeWidget*>();
                Check(tree->selectionMode()==QAbstractItemView::ExtendedSelection,"group hierarchy is enabled");
                tree->clearSelection();
                for(int i=0;i<tree->topLevelItemCount();++i) {
                    auto* item=tree->topLevelItem(i);
                    if(item->text(0).startsWith("Batch "))item->setSelected(true);
                }
            });
            until([&]{return host.ReadSnapshot().selectedIds.size()==3;});
            EditorCommand drag;drag.type=EditorCommand::BeginGizmoDrag;drag.objectId=ids.front();host.PostCommand(drag);
            EditorCommand change;change.type=EditorCommand::UpdateObjectTransform;change.objectId=ids.front();change.path="gizmo";change.transform.position[0]=5;host.PostCommand(change);
            change.transform.position[0]=10;host.PostCommand(change);
            drag.type=EditorCommand::EndGizmoDrag;host.PostCommand(drag);
            until([&]{for(const auto& o:host.ReadSnapshot().objects)if(o.id==ids.front())return o.transform.position[0]==10;return false;});
            EditorCommand duplicate;duplicate.type=EditorCommand::DuplicateObject;duplicate.objectId=ids.front();host.PostCommand(duplicate);
            until([&]{return host.ReadSnapshot().objects.size()==10;});
            EditorCommand undo;undo.type=EditorCommand::Undo;host.PostCommand(undo);
            until([&]{return host.ReadSnapshot().objects.size()==7;});
            Check(host.ReadSnapshot().dirty,"group undo leaves document unsaved");
            host.PostCommand(undo);
            until([&]{for(const auto& o:host.ReadSnapshot().objects)if(o.name.find("Batch ")==0 && o.transform.position[0]!=0)return false;return true;});
            EditorCommand redo;redo.type=EditorCommand::Redo;host.PostCommand(redo);
            until([&]{for(const auto& o:host.ReadSnapshot().objects)if(o.name.find("Batch ")==0 && o.transform.position[0]!=10)return false;return true;});
            const QJsonValue before=RecoveryStore::Read(QString::fromStdString(manifest))["startupScene"];
            EditorCommand save;save.type=EditorCommand::SaveScene;save.path=QString::fromStdWString((root/"Content/Scenes/Other.scene.json").wstring());host.PostCommand(save);
            until([&]{return !host.ReadSnapshot().dirty;});
            Check(RecoveryStore::Read(QString::fromStdString(manifest))["startupScene"]==before,"Save does not change startupScene");
            Ui([](auto* w){Action(w,QStringLiteral("Сделать стартовой сценой"))->trigger();});
            until([&]{return RecoveryStore::Read(QString::fromStdString(manifest))["startupScene"].toString().endsWith("Other.scene.json");});
            view=host.ReadSnapshot();select.objectIds.clear();select.objectId=view.objects.front().id;host.PostCommand(select);
            until([&]{return host.ReadSnapshot().selected==select.objectId;});
            change.objectId=select.objectId;change.path.clear();change.transform=view.objects.front().transform;change.transform.position[1]+=3;host.PostCommand(change);
            until([&]{return host.ReadSnapshot().dirty;});
            EditorCommand recover;recover.type=EditorCommand::WriteRecovery;host.PostCommand(recover);
            for(int i=0;i<30;++i)pump();
            Check(RecoveryStore::Candidates(QString::fromStdString(manifest)).isEmpty(),"live editor is not offered for recovery");
            std::puts("Group UI and save checks passed; simulating process crash"); std::fflush(stdout);
            TerminateProcess(GetCurrentProcess(),0); return 0;
        }
        Ui([&](auto* w) {
            CodeClassDialog dialog(QString::fromStdWString(root.wstring()), w);
            auto* name = dialog.findChild<QLineEdit*>("ClassName");
            auto* base = dialog.findChild<QLineEdit*>("ClassBase");
            auto* kind = dialog.findChild<QComboBox*>("ClassKind");
            auto* button = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
            Check(!button->isEnabled(), "empty class name rejected in wizard");
            for (const auto& invalid : {QStringLiteral("МойПерсонаж"), QStringLiteral("My Character"), QStringLiteral("MyCharacter.h"), QStringLiteral("1Hero"), QStringLiteral("class"), QStringLiteral("Character"), QStringLiteral("CON")}) {
                name->setText(invalid);
                Check(!button->isEnabled() && !dialog.findChild<QLabel*>("ClassError")->text().isEmpty(), "wizard explains invalid name");
                Check(name->text() == invalid, "wizard preserves rejected input");
            }
            kind->setCurrentText("Character"); name->setText("  WizardHero  ");
            Check(button->isEnabled() && base->text() == "Character", "character wizard default parent");
            Check(dialog.Arguments().contains("WizardHero") && dialog.Arguments().contains("Engine/Components/Character.h"), "wizard trims name and sends character header");
            base->setText("WizardHero"); Check(!button->isEnabled(), "self inheritance rejected in wizard");
            kind->setCurrentText("Object");
            Check(button->isEnabled() && !dialog.Arguments().contains("-Base"), "independent object omits empty PowerShell argument");
            const auto existing = root / "Source/WizardExisting.cpp";
            std::filesystem::create_directories(existing.parent_path());
            { QFile file(QString::fromStdWString(existing.wstring())); Check(file.open(QIODevice::WriteOnly), "wizard duplicate fixture"); file.write("// preserved"); }
            name->setText("WizardExisting"); Check(!button->isEnabled(), "existing source rejected in wizard");
            std::filesystem::remove(existing);
        });
        if (codeProject)
        {
            const auto original = host.ReadSnapshot().objects.size();
            Ui([](auto* w) {
                Check(Action(w,QStringLiteral("Добавить C++ в проект…")) && Action(w,QStringLiteral("Добавить C++ класс…")) && Action(w,QStringLiteral("Перекомпилировать / Hot Reload")),"C++ workflow actions visible");
                CreateAction(w,"RulesObject")->trigger();
            });
            until([&]{return host.ReadSnapshot().objects.size()==original+1;});
            bool classesVisible=false;
            Ui([&](auto* w) {
                QTimer::singleShot(50,[&]{
                    auto* modal=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    if(!modal)return;
                    auto* tree=modal->findChild<QTreeWidget*>();
                    bool game=false,object=false,component=false;
                    if(tree)for(int i=0;i<tree->topLevelItemCount();++i){auto* row=tree->topLevelItem(i);game|=row->text(1)=="Game";object|=row->text(1)=="Object";component|=row->text(1)=="Component";}
                    classesVisible=game&&object&&component;modal->reject();
                });
                Action(w,QStringLiteral("Типы C++ проекта…"))->trigger();
            });
            Check(classesVisible,"class browser shows three families");
            Ui([](auto* w){Action(w,QStringLiteral("Дублировать объект"))->trigger();});
            until([&]{return host.ReadSnapshot().objects.size()==original+2;});
            Ui([](auto* w){Action(w,QStringLiteral("Отменить"))->trigger();});
            until([&]{return host.ReadSnapshot().objects.size()==original+1;});
              Ui([](auto* w) {
                  CreateAction(w,"ProjectComponent")->trigger();
              });
              until([&]{return host.ReadSnapshot().objects.size()==original+2;});
              const auto dataObject=host.ReadSnapshot().selected;
              EditorCommand writeData;writeData.type=EditorCommand::SetProjectData;writeData.objectId=dataObject;writeData.path=QStringLiteral("{\"version\":1,\"value\":99}");
              host.PostCommand(writeData);
              auto savedValue=[&](uint64_t id) { for(const auto& object:host.ReadSnapshot().objects)if(object.id==id)return QJsonDocument::fromJson(QByteArray::fromStdString(object.projectData)).object().value("value").toInt(-1);return -1; };
              until([&]{return savedValue(dataObject)==99;});
              Ui([](auto* w){Action(w,QStringLiteral("Дублировать объект"))->trigger();});
              until([&]{return host.ReadSnapshot().objects.size()==original+3;});
              Check(savedValue(host.ReadSnapshot().selected)==99,"duplicate keeps custom payload");
              Ui([](auto* w){Action(w,QStringLiteral("Отменить"))->trigger();});
              until([&]{return host.ReadSnapshot().objects.size()==original+2;});
              Ui([](auto* w){Action(w,QStringLiteral("Отменить"))->trigger();});
              until([&]{for(const auto& object:host.ReadSnapshot().objects)if(object.type=="ProjectComponent"&&savedValue(object.id)==7)return true;return false;});
            Ui([](auto* w){Action(w,QStringLiteral("▶ Play"))->trigger();});
            until([&]{return host.PlayState()==EditorQtHost::Play;});
            Ui([](auto* w){Action(w,QStringLiteral("⏹ Stop"))->trigger();});
            until([&]{return host.PlayState()==EditorQtHost::Edit;});
            Ui([](auto* w){Action(w,QStringLiteral("Сохранить сцену"))->trigger();});
            until([&]{return !host.ReadSnapshot().dirty;});
            Ui([](auto* w){w->close();});until([&]{return exit;});
            host.SetProjectDataApi({});host.SetModuleApi({});factory.destroy(api.context);sessionGuard.context=nullptr;host.Stop();
            std::printf("Code project EditorUiTests: %d checks passed\n",checks);return 0;
        }
        if (legacy)
        {
            const auto originalCount = host.ReadSnapshot().objects.size();
            Ui([](auto* w) {
                QList<QAction*> actions;for(auto* a:w->findChildren<QAction*>())if(a->objectName().startsWith("CreateObject."))actions.push_back(a);
                Check(actions.size()==6,"legacy menu excludes unavailable lights");actions.front()->trigger();
            });
            until([&] { return host.ReadSnapshot().objects.size() == originalCount + 1; });
            auto selectedX = [&] { const auto view = host.ReadSnapshot(); for (const auto& o : view.objects) if (o.id == view.selected) return o.transform.position[0]; return -999.0f; };
            Ui([](auto* w) {
                Check(!w->findChild<QPushButton*>(QStringLiteral("LightColor"))->isVisible(),"legacy light inspector hidden");
                Check(!w->findChild<QPushButton*>(QStringLiteral("EditMaterial"))->isEnabled(),"legacy material editing disabled");
                w->OpenMaterialEditor(QStringLiteral("/Game/Materials/Unavailable.mat"));
                Check(w->findChild<MaterialEditorWindow*>() == nullptr,"legacy DLL never receives material calls");
                auto* x = w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0"));
                x->setValue(3.5); QMetaObject::invokeMethod(x,"editingFinished",Qt::DirectConnection);
            });
            until([&] { return selectedX() == 3.5f; });
            Ui([](auto* w) { Action(w,QStringLiteral("Отменить"))->trigger(); });
            until([&] { return !host.ReadSnapshot().selected; });
            Ui([](auto* w) { Action(w,QStringLiteral("Повторить"))->trigger(); });
            until([&] { for (const auto& o : host.ReadSnapshot().objects) if (o.transform.position[0] == 3.5f) return true; return false; });
            Ui([](auto* w) { Action(w,QStringLiteral("Сохранить сцену"))->trigger(); });
            until([&] { return !host.ReadSnapshot().dirty; });
            Ui([](auto* w) { Action(w,QStringLiteral("▶ Play"))->trigger(); });
            until([&] { return host.PlayState() == EditorQtHost::Play; });
            Ui([](auto* w) { Action(w,QStringLiteral("⏹ Stop"))->trigger(); });
            until([&] { return host.PlayState() == EditorQtHost::Edit; });
            Ui([&](auto* w) { w->OpenScene(QString::fromStdWString(sceneFile.wstring())); });
            for (int i = 0; i < 60; ++i) pump();
            Check(host.ReadSnapshot().objects.size() == originalCount + 1,"legacy reopen preserves objects");
            Ui([](auto* w) { w->close(); }); until([&] { return exit; });
            host.SetModuleApi({}); factory.destroy(sessionGuard.context); sessionGuard.context = nullptr; host.Stop();
            std::printf("Legacy EditorUiTests: %d checks passed\n",checks);
            return 0;
        }
        Ui([](auto* w) {
            Check(CreateAction(w,"Particle.Sparks") && CreateAction(w,"Particle.Smoke"),"particle presets available");
            Check(CreateAction(w,"Environment.Sky"),"sky available");
            Check(!w->findChild<QAction*>("CreateObject.Gameplay.PlaySettings"),"service settings not placeable");
            Check(w->findChild<QAction*>("CreateObject.Gameplay.Player"),"Player is available for placement");
            Check(w->findChild<QTreeWidget*>()->topLevelItemCount() == 4, "starter hierarchy"); Add(w, 0);
        });
        until([&] { return host.ReadSnapshot().objects.size() == 5; });
        Ui([](auto* w) { Add(w, 1); });
        until([&] { return host.ReadSnapshot().objects.size() == 6; });
        const auto imagePath = QString::fromStdWString((root / L"outside.bmp").wstring());
        Ui([&](auto* w) {
            Check(w->findChild<QToolButton*>("ImportAssets")->menu() != nullptr, "imports grouped into menu");
            Check(w->findChild<QPushButton*>("ChooseModel") != nullptr, "model picker exists");
            Check(w->findChild<QLineEdit*>("ModelAsset")->isReadOnly() && w->findChild<QLineEdit*>("TextureAsset") == nullptr, "model path locked and texture row removed");
            for (const auto& title : {QStringLiteral("Общие"),QStringLiteral("Трансформация"),QStringLiteral("Геометрия"),QStringLiteral("Материал")}) {
                bool found=false; for (auto* group : w->template findChildren<QGroupBox*>()) found |= group->title()==title;
                for(auto* header:w->template findChildren<QToolButton*>())found|=header->objectName().startsWith("Section.")&&header->text()==title;
                for(auto* button:w->template findChildren<QPushButton*>()) found|=button->objectName()=="CardHeaderBtn" && button->text().contains(title);
                Check(found,"inspector card categories");
            }
            for (auto* button : w->template findChildren<QPushButton*>()) Check(button->text()!=QStringLiteral("Дубликат") && button->text()!=QStringLiteral("Удалить"),"no redundant hierarchy buttons");
            auto* visibility=w->findChild<QComboBox*>("FileVisibility");
            auto* files=w->findChild<QTreeView*>("ProjectFiles");
            Check(visibility && visibility->currentIndex()==0,"content and source default");
            for(int i=0;i<files->model()->rowCount(files->rootIndex());++i) {
                const auto name=files->model()->index(i,0,files->rootIndex()).data().toString();
                Check(name=="Content" || name=="Source","service files hidden");
            }
            visibility->setCurrentIndex(1);
            Check(files->model()->rowCount(files->rootIndex())>1,"full project switch reveals files");
            visibility->setCurrentIndex(0);
            QImage image(4,4,QImage::Format_RGB32); image.fill(Qt::green); Check(image.save(imagePath), "write UI texture fixture");
            w->ImportAsset(true, imagePath);
        });
        const auto importedImage = root / "Content/Textures/outside.bmp";
        until([&] { return std::filesystem::exists(importedImage); });
        Ui([&](auto* w) { w->UseAsset(QString::fromStdWString(importedImage.wstring())); });
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (!o.textureAsset.empty()) return true; return false; });
        Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (!o.textureAsset.empty()) return false; return true; });
        Ui([](auto* w) { Action(w, QStringLiteral("Повторить"))->trigger(); });
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (!o.textureAsset.empty()) return true; return false; });

        // Content Browser Delete key test (column 0 and column 1)
        {
            const size_t objectCountBefore = host.ReadSnapshot().objects.size();
            Check(objectCountBefore > 0, "scene has objects before asset delete");
            const auto targetObj = host.ReadSnapshot().objects.front();
            {
                EditorCommand sel;
                sel.type = EditorCommand::SelectObject;
                sel.objectId = targetObj.id;
                host.PostCommand(sel);
            }
            until([&] { return host.ReadSnapshot().selected == targetObj.id; });
            Check(host.ReadSnapshot().selected == targetObj.id, "scene object selected before delete");

            // --- Column 0 test ---
            const auto delImg0 = root / "Content/Textures/del_col0.bmp";
            QImage img0(4, 4, QImage::Format_RGB32);
            img0.fill(Qt::yellow);
            Check(img0.save(QString::fromStdWString(delImg0.wstring())), "save del_col0.bmp");
            until([&] { return std::filesystem::exists(delImg0); });

            until([&] {
                bool ready = false;
                Ui([&](auto* w) {
                    auto* files = w->template findChild<QTreeView*>("ProjectFiles");
                    auto* proxy = files ? static_cast<QSortFilterProxyModel*>(files->model()) : nullptr;
                    auto* fsModel = proxy ? static_cast<QFileSystemModel*>(proxy->sourceModel()) : nullptr;
                    if (fsModel && proxy) {
                        const auto idx = fsModel->index(QString::fromStdWString(delImg0.wstring()));
                        ready = idx.isValid() && proxy->mapFromSource(idx).isValid();
                    }
                });
                return ready;
            });

            Ui([&](auto* w) {
                auto* files = w->template findChild<QTreeView*>("ProjectFiles");
                auto* proxy = static_cast<QSortFilterProxyModel*>(files->model());
                auto* fsModel = static_cast<QFileSystemModel*>(proxy->sourceModel());
                const QModelIndex srcIdx = fsModel->index(QString::fromStdWString(delImg0.wstring()));
                const QModelIndex proxyIdx = proxy->mapFromSource(srcIdx);
                const QModelIndex col0 = proxyIdx.sibling(proxyIdx.row(), 0);
                files->setCurrentIndex(col0);
                files->setFocus();
                Check(files->currentIndex().column() == 0, "del_col0 selected in column 0");

                QTimer::singleShot(60, w, [] {
                    if (auto* mb = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                        if (auto* btn = mb->button(QMessageBox::Yes)) btn->click();
                        else mb->accept();
                    }
                });

                QTest::keyClick(files, Qt::Key_Delete);
            });

            until([&] { return !std::filesystem::exists(delImg0); });
            Check(host.ReadSnapshot().objects.size() == objectCountBefore, "scene object count unchanged on asset col 0 delete");
            bool objExists0 = false;
            for (const auto& o : host.ReadSnapshot().objects) if (o.id == targetObj.id) { objExists0 = true; break; }
            Check(objExists0, "selected scene object NOT deleted on asset col 0 delete");

            // --- Column 1 test ---
            const auto delImg1 = root / "Content/Textures/del_col1.bmp";
            QImage img1(4, 4, QImage::Format_RGB32);
            img1.fill(Qt::cyan);
            Check(img1.save(QString::fromStdWString(delImg1.wstring())), "save del_col1.bmp");
            until([&] { return std::filesystem::exists(delImg1); });

            until([&] {
                bool ready = false;
                Ui([&](auto* w) {
                    auto* files = w->template findChild<QTreeView*>("ProjectFiles");
                    auto* proxy = files ? static_cast<QSortFilterProxyModel*>(files->model()) : nullptr;
                    auto* fsModel = proxy ? static_cast<QFileSystemModel*>(proxy->sourceModel()) : nullptr;
                    if (fsModel && proxy) {
                        const auto idx = fsModel->index(QString::fromStdWString(delImg1.wstring()));
                        ready = idx.isValid() && proxy->mapFromSource(idx).isValid();
                    }
                });
                return ready;
            });

            Ui([&](auto* w) {
                auto* files = w->template findChild<QTreeView*>("ProjectFiles");
                auto* proxy = static_cast<QSortFilterProxyModel*>(files->model());
                auto* fsModel = static_cast<QFileSystemModel*>(proxy->sourceModel());
                const QModelIndex srcIdx = fsModel->index(QString::fromStdWString(delImg1.wstring()));
                const QModelIndex proxyIdx = proxy->mapFromSource(srcIdx);
                const QModelIndex col1 = proxyIdx.sibling(proxyIdx.row(), 1);
                files->setCurrentIndex(col1);
                files->setFocus();
                Check(files->currentIndex().column() == 1, "del_col1 selected in column 1");

                QTimer::singleShot(60, w, [] {
                    if (auto* mb = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                        if (auto* btn = mb->button(QMessageBox::Yes)) btn->click();
                        else mb->accept();
                    }
                });

                QTest::keyClick(files, Qt::Key_Delete);
            });

            until([&] { return !std::filesystem::exists(delImg1); });
            Check(host.ReadSnapshot().objects.size() == objectCountBefore, "scene object count unchanged on asset col 1 delete");
            bool objExists1 = false;
            for (const auto& o : host.ReadSnapshot().objects) if (o.id == targetObj.id) { objExists1 = true; break; }
            Check(objExists1, "selected scene object NOT deleted on asset col 1 delete");
        }

        // Step C: OpenMaterialEditor on nonexistent file shows warning and does NOT open window
        Ui([&](auto* w) {
            bool warningShown = false;
            QTimer::singleShot(50, w, [&] {
                if (auto* mb = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                    warningShown = true;
                    mb->accept();
                }
            });
            w->OpenMaterialEditor(QStringLiteral("/Game/Materials/NonExistent_12345.mat"));
            Check(warningShown, "warning dialog shown for nonexistent material");
            Check(w->template findChild<MaterialEditorWindow*>() == nullptr, "no MaterialEditorWindow created for nonexistent material");
        });

        // Material editor: template from Files, live parameter, HLSL errors, recompiled parameters, save and discard.
        const auto materialFile = root / "Content/Materials/Tint.mat", surfaceFile = root / "Content/Materials/Tint.hlsl";
        auto editorWidget = [](auto* w, const QString& name) -> QWidget* {
            auto* editor = w->template findChild<MaterialEditorWindow*>();
            return editor && editor->isVisible() ? editor->template findChild<QWidget*>(name) : nullptr;
        };
        auto inEditor = [&](const QString& name) { bool found = false; Ui([&](auto* w) { found = editorWidget(w, name) != nullptr; }); return found; };
        auto saveMaterial = [](auto* w) {
            for (auto* action : w->template findChild<MaterialEditorWindow*>()->template findChildren<QAction*>())
                if (action->text() == QStringLiteral("Сохранить")) { action->trigger(); return; }
            throw std::runtime_error("material Save action");
        };
        auto typeSource = [](QPlainTextEdit* source, const QString& text) {
            QTextCursor cursor(source->document()); // as typed: setPlainText would clear the modified flag
            cursor.select(QTextCursor::Document);
            cursor.insertText(text);
        };
        auto materialText = [&] { QFile f(QString::fromStdWString(materialFile.wstring())); f.open(QIODevice::ReadOnly); return QJsonDocument::fromJson(f.readAll()).object(); };
        Ui([&](auto* w) { Check(w->CreateMaterial({}, QStringLiteral("Tint")).endsWith(QStringLiteral("Tint.mat")), "create material"); });
        Check(std::filesystem::exists(materialFile) && std::filesystem::exists(surfaceFile), "material template files");
        until([&] { return inEditor(QStringLiteral("MaterialParam_Gloss_0")) && inEditor(QStringLiteral("MaterialParam_Tint_3")); });
        Ui([&](auto* w) {
            // The engine presents into the native window: grab the screen, not the widget.
            auto* preview = editorWidget(w, QStringLiteral("MaterialPreview"));
            const QImage frame = preview->screen()->grabWindow(preview->winId()).toImage();
            Check(frame.save(QString::fromStdWString((root / "editor-material-preview.png").wstring())), "save material preview frame");
            const QColor center = frame.pixelColor(frame.width() / 2, frame.height() / 2), corner = frame.pixelColor(4, 4);
            Check(corner.red() > 20 && corner.blue() > corner.red(), "preview clears to its background");
            Check(center != corner, "preview draws the shape");
        });
        Ui([&](auto* w) {
            Check(w->template findChildren<MaterialEditorWindow*>().size() == 1, "one window per material");
            w->OpenMaterialEditor(QStringLiteral("/Game/Materials/Tint.mat"));
            Check(w->template findChildren<MaterialEditorWindow*>().size() == 1, "reopen raises the same window");
            Check(editorWidget(w, QStringLiteral("MaterialColor_Tint")) != nullptr, "color picker for float4");
            static_cast<QDoubleSpinBox*>(editorWidget(w, QStringLiteral("MaterialParam_Gloss_0")))->setValue(0.9);
        });
        until([&] { bool modified = false; Ui([&](auto* w) { modified = w->template findChild<MaterialEditorWindow*>()->HasUnsavedChanges(); }); return modified; });
        Ui([&](auto* w) {
            auto* source = static_cast<QPlainTextEdit*>(editorWidget(w, QStringLiteral("MaterialSource")));
            typeSource(source, source->toPlainText().replace(QStringLiteral("o.SpecularWeight = Gloss;"), QStringLiteral("o.SpecularWeight = Missing;")));
            saveMaterial(w);
        });
        int errorLine = 0;
        until([&] {
            Ui([&](auto* w) {
                auto* errors = static_cast<QListWidget*>(editorWidget(w, QStringLiteral("MaterialErrors")));
                for (int i = 0; errors && i < errors->count() && !errorLine; ++i) errorLine = errors->item(i)->data(Qt::UserRole).toInt();
            });
            return errorLine != 0;
        });
        Check(errorLine == 13, "compile error points at the surface line");
        Check(std::abs(materialText().value(QStringLiteral("params")).toObject().value(QStringLiteral("Gloss")).toDouble() - 0.9) < 1e-4, "save writes edited parameter");
        Ui([&](auto* w) {
            auto* source = static_cast<QPlainTextEdit*>(editorWidget(w, QStringLiteral("MaterialSource")));
            typeSource(source, source->toPlainText().replace(QStringLiteral("Missing;"), QStringLiteral("Gloss * Rim;"))
                                     .replace(QStringLiteral("float Gloss = 0.5f;"), QStringLiteral("float Gloss = 0.5f;\n    float Rim = 0.25f;")));
            saveMaterial(w);
        });
        until([&] { return inEditor(QStringLiteral("MaterialParam_Rim_0")); });
        Ui([&](auto* w) {
            Check(static_cast<QListWidget*>(editorWidget(w, QStringLiteral("MaterialErrors")))->count() == 0, "fixed HLSL compiles");
            Check(std::abs(static_cast<QDoubleSpinBox*>(editorWidget(w, QStringLiteral("MaterialParam_Gloss_0")))->value() - 0.9) < 1e-4, "recompile keeps edited value");
            static_cast<QDoubleSpinBox*>(editorWidget(w, QStringLiteral("MaterialParam_Gloss_0")))->setValue(0.1);
            static_cast<QComboBox*>(editorWidget(w, QStringLiteral("MaterialPreviewShape")))->setCurrentIndex(1);
        });
        for (int i = 0; i < 30; ++i) pump();
        Ui([](auto* w) { QMetaObject::invokeMethod(w->template findChild<MaterialEditorWindow*>(), "close", Qt::QueuedConnection); });
        until([&] { bool modal = false; Ui([&](auto*) { modal = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) != nullptr; }); return modal; });
        // question() reports the clicked button; done() alone would read as NoButton
        Ui([](auto*) { qobject_cast<QMessageBox*>(QApplication::activeModalWidget())->button(QMessageBox::Discard)->click(); });
        until([&] { bool gone = false; Ui([&](auto* w) { gone = w->template findChild<MaterialEditorWindow*>() == nullptr; }); return gone; });
        Check(std::abs(materialText().value(QStringLiteral("params")).toObject().value(QStringLiteral("Gloss")).toDouble() - 0.9) < 1e-4, "discard keeps the saved file");
        Ui([&](auto* w) { w->OpenMaterialEditor(QString::fromStdWString(materialFile.wstring())); });
        until([&] { return inEditor(QStringLiteral("MaterialParam_Rim_0")); });
        Ui([&](auto* w) {
            Check(std::abs(static_cast<QDoubleSpinBox*>(editorWidget(w, QStringLiteral("MaterialParam_Gloss_0")))->value() - 0.9) < 1e-4, "discard reverts the engine material");
            Check(w->template findChild<MaterialEditorWindow*>()->grab().save(QString::fromStdWString((root / "editor-material.png").wstring())), "save material editor preview");
            static_cast<QDoubleSpinBox*>(editorWidget(w, QStringLiteral("MaterialParam_Gloss_0")))->setValue(0.6);
        });
        for (int i = 0; i < 10; ++i) pump();
        const DWORD materialAttributes = GetFileAttributesW(materialFile.c_str());
        Check(materialAttributes != INVALID_FILE_ATTRIBUTES && SetFileAttributesW(materialFile.c_str(), materialAttributes | FILE_ATTRIBUTE_READONLY), "make material unwritable for close regression");
        Ui([](auto* w) {
            QMetaObject::invokeMethod(w->template findChild<MaterialEditorWindow*>(), "close", Qt::QueuedConnection);
        });
        until([&] { bool modal = false; Ui([&](auto*) { modal = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) != nullptr; }); return modal; });
        Ui([](auto*) { qobject_cast<QMessageBox*>(QApplication::activeModalWidget())->button(QMessageBox::Save)->click(); });
        until([&] { bool finished = false; Ui([&](auto* w) { auto* e=w->template findChild<MaterialEditorWindow*>(); finished=e && e->isEnabled(); }); return finished; });
        Check(SetFileAttributesW(materialFile.c_str(),materialAttributes), "restore writable material");
        Ui([](auto* w) {
            auto* editor=w->template findChild<MaterialEditorWindow*>();
            Check(editor && editor->isVisible() && editor->HasUnsavedChanges(), "failed save keeps material window and edits alive");
            QMetaObject::invokeMethod(editor, "close", Qt::QueuedConnection);
        });
        until([&] { bool modal = false; Ui([&](auto*) { modal = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) != nullptr; }); return modal; });
        Ui([](auto*) { qobject_cast<QMessageBox*>(QApplication::activeModalWidget())->button(QMessageBox::Save)->click(); });
        until([&] { bool gone = false; Ui([&](auto* w) { gone = w->template findChild<MaterialEditorWindow*>() == nullptr; }); return gone; });
        Check(std::abs(materialText().value(QStringLiteral("params")).toObject().value(QStringLiteral("Gloss")).toDouble() - 0.6) < 1e-4, "close waits for successful material save");

        // Texture slots: HLSL edits, drag and drop, import, the picker, add and remove from the UI.
        {
            namespace Tex = MaterialTextures;
            QString text = QStringLiteral("void Surface(MaterialInput input, inout MaterialOutput output) { /* } */ output.Albedo = 1; }\n"), problem;
            Check(Tex::AddSlot(text, QStringLiteral("A"), Tex::Role::Color, problem) && text.contains(QStringLiteral("A.Sample(MaterialSampler, input.UV)")) &&
                  text.contains(QStringLiteral("output.Albedo *= aColor.rgb;")) && text.contains(QStringLiteral("Texture2D A : register(t6);")), "slot uses the surface's own argument names");
            Check(text.indexOf(QStringLiteral("output.Albedo *= aColor.rgb;")) > text.indexOf(QStringLiteral("output.Albedo = 1;")), "slot is read after the surface code");
            Check(Tex::AddSlot(text, QStringLiteral("B"), Tex::Role::Emissive, problem) && text.contains(QStringLiteral("register(t7)")) &&
                  text.contains(QStringLiteral("#define MATERIAL_EMISSIVE")), "next register and emissive define");
            Check(!Tex::AddSlot(text, QStringLiteral("B"), Tex::Role::Color, problem), "taken slot name rejected");
            Check(!Tex::RemoveSlot(text, QStringLiteral("output"), problem), "shared line is not cut");
            Check(Tex::RemoveSlot(text, QStringLiteral("A"), problem) && !text.contains(QStringLiteral("aColor")) && !text.contains(QStringLiteral("A.Sample")), "removal drops the slot and its local");
            QString mixed = QStringLiteral("Texture2D C : register(t6);\nvoid Surface(MaterialInput i, inout MaterialOutput o)\n{\n    float4 color = C.Sample(MaterialSampler, i.UV) * Tint;\n    o.Albedo *= color.rgb;\n}\n");
            const QString untouched = mixed;
            Check(!Tex::RemoveSlot(mixed, QStringLiteral("C"), problem) && mixed == untouched, "expression with other logic is not cut");
            QString noSurface = QStringLiteral("float4 Other() { return 0; }\n");
            Check(!Tex::AddSlot(noSurface, QStringLiteral("A"), Tex::Role::Color, problem), "no Surface function");
            QString boundary = QStringLiteral("Texture2D Prior : register(t31);\nvoid Surface(MaterialInput i, inout MaterialOutput o) {}\n");
            Check(Tex::AddSlot(boundary, QStringLiteral("Safe"), Tex::Role::Color, problem) &&
                boundary.contains(QStringLiteral("Safe : register(t33)")), "HLSL texture tools skip point shadow register");
            QString crowded = QStringLiteral("void Surface(MaterialInput i, inout MaterialOutput o) {}\n");
            for (int i = 0; i < 121; ++i)
                Check(Tex::AddSlot(crowded, QStringLiteral("Map%1").arg(i), Tex::Role::Color, problem), "available HLSL texture slot allocated");
            const QString unchanged = crowded;
            Check(!Tex::AddSlot(crowded, QStringLiteral("Overflow"), Tex::Role::Color, problem) && crowded == unchanged && !problem.isEmpty(),
                "exhausted texture slots fail without changing HLSL");
            QString high = QStringLiteral("Texture2D Prior : register(t127);\nvoid Surface(MaterialInput i, inout MaterialOutput o) {}\n");
            Check(Tex::AddSlot(high, QStringLiteral("Low"), Tex::Role::Color, problem) &&
                high.contains(QStringLiteral("Low : register(t6)")), "HLSL texture slots wrap into available gap");
        }
        const auto paintFile = root / "Content/Materials/Paint.mat", redSource = root / L"outside-red.bmp";
        auto textureField = [&](const QString& slot) { QString value; Ui([&](auto* w) { if (auto* field = qobject_cast<QLineEdit*>(editorWidget(w, QStringLiteral("MaterialTexture_") + slot))) value = field->text(); }); return value; };
        auto dropTexture = [](QWidget* slot, const QString& file, bool accepted) {
            QMimeData mime;
            mime.setUrls({QUrl::fromLocalFile(file)});
            QDragEnterEvent enter(QPoint(4, 4), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(slot, &enter);
            Check(enter.isAccepted() == accepted, "slot filters dragged files");
            if (!accepted) return;
            QDropEvent drop(QPointF(4, 4), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(slot, &drop);
        };
        auto previewCenter = [&] {
            QColor color;
            Ui([&](auto* w) {
                auto* preview = editorWidget(w, QStringLiteral("MaterialPreview"));
                const QImage frame = preview->screen()->grabWindow(preview->winId()).toImage();
                color = frame.pixelColor(frame.width() / 2, frame.height() / 2);
            });
            return color;
        };
        auto modalIs = [&](const QString& name) { bool found = false; Ui([&](auto*) { auto* modal = QApplication::activeModalWidget(); found = modal && modal->objectName() == name; }); return found; };
        auto pickTexture = [&](const QString& file) {
            Ui([&](auto*) {
                auto* dialog = QApplication::activeModalWidget();
                auto* list = dialog->template findChild<QListWidget*>(QStringLiteral("MaterialTextureList"));
                for (int i = 0; i < list->count(); ++i) if (list->item(i)->text() == file) list->setCurrentRow(i);
                Check(list->currentItem() && list->currentItem()->text() == file, "picker lists project texture");
                dialog->template findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            });
        };
        Ui([&](auto* w) {
            Check(w->template findChild<QTreeView*>(QStringLiteral("ProjectFiles"))->dragEnabled(), "files can be dragged");
            QImage image(4, 4, QImage::Format_RGB32); image.fill(Qt::red); Check(image.save(QString::fromStdWString(redSource.wstring())), "write outside texture fixture");
            Check(!w->CreateMaterial({}, QStringLiteral("Paint")).isEmpty(), "create textured material");
        });
        until([&] { return inEditor(QStringLiteral("MaterialSlot_BaseMap")) && inEditor(QStringLiteral("MaterialAddTextureButton")); });
        const QString greenFile = QString::fromStdWString(importedImage.wstring()), redFile = QString::fromStdWString(redSource.wstring());
        Ui([&](auto* w) {
            auto* slot = editorWidget(w, QStringLiteral("MaterialSlot_BaseMap"));
            dropTexture(slot, QStringLiteral("C:/Windows/notes.txt"), false);
            dropTexture(slot, greenFile, true);
        });
        until([&] { return textureField(QStringLiteral("BaseMap")) == QStringLiteral("/Game/Textures/outside.bmp"); });
        until([&] { const QColor c = previewCenter(); return c.green() > c.red() + 40 && c.green() > c.blue() + 40; });
        Ui([&](auto* w) { dropTexture(editorWidget(w, QStringLiteral("MaterialSlot_BaseMap")), redFile, true); });
        until([&] { return textureField(QStringLiteral("BaseMap")) == QStringLiteral("/Game/Textures/outside-red.bmp"); });
        Check(std::filesystem::exists(root / "Content/Textures/outside-red.bmp"), "file from outside the project is imported");
        until([&] { const QColor c = previewCenter(); return c.red() > c.green() + 40 && c.red() > c.blue() + 40; });
        Ui([&](auto* w) { static_cast<QPushButton*>(editorWidget(w, QStringLiteral("MaterialTextureChoose_BaseMap")))->click(); });
        until([&] { return modalIs(QStringLiteral("MaterialTexturePicker")); });
        Ui([&](auto*) {
            auto* dialog = QApplication::activeModalWidget();
            auto* list = dialog->template findChild<QListWidget*>(QStringLiteral("MaterialTextureList"));
            Check(list->count() == 3 && list->currentItem()->text() == QStringLiteral("outside-red.bmp"), "picker shows None, project textures and the current one");
            dialog->template findChild<QLineEdit*>(QStringLiteral("MaterialTextureSearch"))->setText(QStringLiteral("RED"));
            Check(!list->item(0)->isHidden() && list->findItems(QStringLiteral("outside.bmp"), Qt::MatchExactly).first()->isHidden(), "picker search");
            Check(dialog->grab().save(QString::fromStdWString((root / "editor-material-picker.png").wstring())), "save texture picker");
            dialog->template findChild<QLineEdit*>(QStringLiteral("MaterialTextureSearch"))->clear();
        });
        pickTexture(QStringLiteral("outside.bmp"));
        until([&] { return textureField(QStringLiteral("BaseMap")) == QStringLiteral("/Game/Textures/outside.bmp"); });
        Ui([&](auto* w) { static_cast<QPushButton*>(editorWidget(w, QStringLiteral("MaterialAddTextureButton")))->click(); });
        until([&] { return modalIs(QStringLiteral("MaterialAddTexture")); });
        Ui([&](auto*) {
            auto* dialog = QApplication::activeModalWidget();
            auto* name = dialog->template findChild<QLineEdit*>(QStringLiteral("MaterialAddTextureName"));
            Check(name->text() == QStringLiteral("BaseMap2"), "free default slot name");
            auto* ok = dialog->template findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
            name->setText(QStringLiteral("Tint"));
            Check(!ok->isEnabled(), "name taken by a parameter rejected");
            name->setText(QStringLiteral("BaseMap2"));
            dialog->template findChild<QComboBox*>(QStringLiteral("MaterialAddTextureRole"))->setCurrentIndex(3);
            Check(name->text() == QStringLiteral("EmissiveMap") && ok->isEnabled(), "default name follows the purpose");
            ok->click();
        });
        until([&] { return modalIs(QStringLiteral("MaterialTexturePicker")); });
        pickTexture(QStringLiteral("outside-red.bmp"));
        until([&] { return textureField(QStringLiteral("EmissiveMap")) == QStringLiteral("/Game/Textures/outside-red.bmp"); });
        Ui([&](auto* w) {
            const QString source = static_cast<QPlainTextEdit*>(editorWidget(w, QStringLiteral("MaterialSource")))->toPlainText();
            Check(source.contains(QStringLiteral("Texture2D EmissiveMap : register(t7);")) && source.contains(QStringLiteral("#define MATERIAL_EMISSIVE")), "slot written to HLSL");
            Check(static_cast<QListWidget*>(editorWidget(w, QStringLiteral("MaterialErrors")))->count() == 0, "added slot compiles");
            Check(!static_cast<QPlainTextEdit*>(editorWidget(w, QStringLiteral("MaterialSource")))->document()->isModified(), "added slot is written to disk");
            w->template findChild<MaterialEditorWindow*>()->AddTextureSlot(QStringLiteral("MaskMap"), 1, {});
        });
        until([&] { return inEditor(QStringLiteral("MaterialSlot_MaskMap")); });
        Ui([&](auto* w) {
            Check(static_cast<QComboBox*>(editorWidget(w, QStringLiteral("MaterialBlend")))->currentIndex() == 1, "opacity map switches an opaque material to masked");
            w->template findChild<MaterialEditorWindow*>()->RemoveTextureSlot(QStringLiteral("EmissiveMap"));
        });
        until([&] { return !inEditor(QStringLiteral("MaterialSlot_EmissiveMap")); });
        Ui([&](auto* w) {
            const QString cleaned = static_cast<QPlainTextEdit*>(editorWidget(w, QStringLiteral("MaterialSource")))->toPlainText();
            Check(!cleaned.contains(QStringLiteral("EmissiveMap")) && !cleaned.contains(QStringLiteral("MATERIAL_EMISSIVE")), "removed slot leaves HLSL");
            Check(static_cast<QListWidget*>(editorWidget(w, QStringLiteral("MaterialErrors")))->count() == 0, "HLSL compiles after removal");
            Check(w->template findChild<MaterialEditorWindow*>()->grab().save(QString::fromStdWString((root / "editor-material-textures.png").wstring())), "save texture slots preview");
            Check(editorWidget(w, QStringLiteral("MaterialSlot_BaseMap"))->parentWidget()->grab().save(QString::fromStdWString((root / "editor-material-slots.png").wstring())), "save texture slots");
            saveMaterial(w);
        });
        until([&] { bool saved = false; Ui([&](auto* w) { auto* e = w->template findChild<MaterialEditorWindow*>(); saved = e->isEnabled() && !e->HasUnsavedChanges(); }); return saved; });
        {
            QFile saved(QString::fromStdWString(paintFile.wstring())); Check(saved.open(QIODevice::ReadOnly), "saved material readable");
            const auto document = QJsonDocument::fromJson(saved.readAll()).object();
            Check(document.value(QStringLiteral("textures")).toObject().value(QStringLiteral("BaseMap")).toString() == QStringLiteral("../Textures/outside.bmp"), "texture written relative to the .mat");
            Check(document.value(QStringLiteral("render")).toObject().value(QStringLiteral("blend")).toString() == QStringLiteral("masked"), "blend change saved");
        }
        Ui([](auto* w) { QMetaObject::invokeMethod(w->template findChild<MaterialEditorWindow*>(), "close", Qt::QueuedConnection); });
        until([&] { bool gone = false; Ui([&](auto* w) { gone = w->template findChild<MaterialEditorWindow*>() == nullptr; }); return gone; });
        Ui([&](auto* w) { w->OpenMaterialEditor(QString::fromStdWString(paintFile.wstring())); });
        until([&] { return textureField(QStringLiteral("BaseMap")) == QStringLiteral("/Game/Textures/outside.bmp"); });
        Ui([](auto* w) { QMetaObject::invokeMethod(w->template findChild<MaterialEditorWindow*>(), "close", Qt::QueuedConnection); });
        until([&] { bool gone = false; Ui([&](auto* w) { gone = w->template findChild<MaterialEditorWindow*>() == nullptr; }); return gone; });

        // Node graph: the graph generates the HLSL; edits go to disk at once and are undone by Discard until Save.
        {
            const auto graphMat = root / "Content/Materials/Graphy.mat", graphHlsl = root / "Content/Materials/Graphy.hlsl";
            const auto graphJson = root / "Content/Materials/Graphy.mat.graph.json";
            auto readText = [](const std::filesystem::path& file) { QFile f(QString::fromStdWString(file.wstring())); f.open(QIODevice::ReadOnly); return QString::fromUtf8(f.readAll()); };
            auto editor = [](auto* w) { return w->template findChild<MaterialEditorWindow*>(); };
            auto masterId = [](MaterialGraphWidget* g) { for (const auto& n : g->Model().Nodes()) if (n.type == QStringLiteral("MaterialOutput")) return n.id; return 0; };
            auto hasLink = [](MaterialGraphWidget* g, int from, const QString& fromPin, int to, const QString& toPin) {
                for (const auto& c : g->Model().Connections()) if (c.fromNode == from && c.fromPin == fromPin && c.toNode == to && c.toPin == toPin) return true;
                return false;
            };
            auto mouse = [](QGraphicsView* view, QEvent::Type type, const QPointF& scenePos) {
                const QPointF local = view->mapFromScene(scenePos);
                QMouseEvent event(type, local, view->viewport()->mapToGlobal(local), Qt::LeftButton,
                                  type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(view->viewport(), &event);
            };
            const QString flatFile = QString::fromStdWString((root / "Content/Textures/flat-normal.png").wstring());
            const QString tiltFile = QString::fromStdWString((root / "Content/Textures/tilt-normal.png").wstring());
            Ui([&](auto* w) {
                QImage flat(4, 4, QImage::Format_RGB32); flat.fill(QColor(128, 128, 255)); Check(flat.save(flatFile), "write flat normal map");
                QImage tilt(4, 4, QImage::Format_RGB32); tilt.fill(QColor(250, 128, 140)); Check(tilt.save(tiltFile), "write tilted normal map");
                Check(!w->CreateMaterial({}, QStringLiteral("Graphy")).isEmpty(), "create material for graph");
            });
            until([&] { return inEditor(QStringLiteral("MaterialParam_Tint_0")); });
            Ui([&](auto* w) {
                Check(!editor(w)->HasGraph() && !std::filesystem::exists(graphJson), "new material starts as code");
                editor(w)->EnableGraph();
            });
            until([&] { return inEditor(QStringLiteral("MaterialParam_BaseColor_0")) && !inEditor(QStringLiteral("MaterialParam_Tint_0")); });
            int texture = 0, multiply = 0, scalar = 0;
            Ui([&](auto* w) {
                auto* e = editor(w);
                auto* source = static_cast<QPlainTextEdit*>(editorWidget(w, QStringLiteral("MaterialSource")));
                Check(e->HasGraph() && std::filesystem::exists(graphJson) && source->isReadOnly(), "graph mode: json on disk, HLSL read-only");
                Check(source->toPlainText().contains(QStringLiteral("Generated by")) && readText(graphHlsl) == source->toPlainText(), "HLSL generated from the graph");
                Check(!editorWidget(w, QStringLiteral("MaterialAddTextureButton")), "graph materials add textures with nodes");
                auto* g = e->Graph();
                const int master = masterId(g);
                int color = 0;
                for (const auto& n : g->Model().Nodes()) if (n.type == QStringLiteral("ColorParam")) color = n.id;
                texture = g->AddNode(QStringLiteral("Texture2D"), QPointF(0, 400));
                multiply = g->AddNode(QStringLiteral("Multiply"), QPointF(300, 400));
                Check(g->Connect(texture, QStringLiteral("RGB"), multiply, QStringLiteral("A")) &&
                      g->Connect(color, QStringLiteral("RGB"), multiply, QStringLiteral("B")) &&
                      g->Connect(multiply, QStringLiteral("Out"), master, QStringLiteral("Albedo")), "connect nodes");
                Check(!hasLink(g, color, QStringLiteral("RGB"), master, QStringLiteral("Albedo")), "an input keeps one link");
                Check(!g->Connect(multiply, QStringLiteral("Out"), texture, QStringLiteral("UV")), "cycles are rejected");
                Check(!g->Connect(texture, QStringLiteral("UV"), multiply, QStringLiteral("A")), "inputs cannot feed inputs");
                const int second = g->AddNode(QStringLiteral("Texture2D"), QPointF(0, 600));
                Check(g->Model().FindNode(second)->properties.value(QStringLiteral("textureName")).toString() == QStringLiteral("BaseMap2"), "new texture node gets a free name");
                g->RemoveNodes({second, master});
                Check(g->Model().FindNode(master) && !g->Model().FindNode(second), "material output cannot be deleted");

                // Drag a link with the mouse, then pull it off the input again.
                scalar = g->AddNode(QStringLiteral("ScalarParam"), QPointF(0, 700));
                mouse(g->View(), QEvent::MouseButtonPress, g->PinScenePos(scalar, QStringLiteral("Value")));
                mouse(g->View(), QEvent::MouseMove, g->PinScenePos(master, QStringLiteral("AmbientModel")));
                mouse(g->View(), QEvent::MouseButtonRelease, g->PinScenePos(master, QStringLiteral("AmbientModel")));
                Check(hasLink(g, scalar, QStringLiteral("Value"), master, QStringLiteral("AmbientModel")), "link dragged from an output to an input");
                mouse(g->View(), QEvent::MouseButtonPress, g->PinScenePos(master, QStringLiteral("AmbientModel")));
                mouse(g->View(), QEvent::MouseButtonRelease, g->PinScenePos(master, QStringLiteral("AmbientModel")) + QPointF(-200, 300));
                Check(!hasLink(g, scalar, QStringLiteral("Value"), master, QStringLiteral("AmbientModel")), "dragging off an input removes its link");
                e->ApplyGraph();
            });
            until([&] { return inEditor(QStringLiteral("MaterialSlot_BaseMap")); });
            Ui([&](auto* w) {
                Check(!editorWidget(w, QStringLiteral("MaterialTextureRemove_BaseMap"))->isVisible(), "graph slots are removed with nodes");
                Check(static_cast<QListWidget*>(editorWidget(w, QStringLiteral("MaterialErrors")))->count() == 0, "graph HLSL compiles");
                editor(w)->AssignTexture(QStringLiteral("BaseMap"), greenFile);
            });
            until([&] { return textureField(QStringLiteral("BaseMap")) == QStringLiteral("/Game/Textures/outside.bmp"); });
            until([&] { const QColor c = previewCenter(); return c.green() > c.red() + 40 && c.green() > c.blue() + 40; });
            Ui([&](auto* w) {
                static_cast<QDoubleSpinBox*>(editorWidget(w, QStringLiteral("MaterialParam_Specular_0")))->setValue(0.8);
                for (const auto& n : editor(w)->Graph()->Model().Nodes())
                    if (n.properties.value(QStringLiteral("name")).toString() == QStringLiteral("Specular"))
                        Check(std::abs(n.properties.value(QStringLiteral("value")).toDouble() - 0.8) < 1e-4, "settings value reaches the graph node");
            });
            for (int i = 0; i < 20; ++i) pump();
            const QColor baseline = previewCenter();
            Ui([&](auto* w) {
                auto* g = editor(w)->Graph();
                const int normal = g->AddNode(QStringLiteral("NormalMap"), QPointF(0, 900));
                Check(g->Connect(normal, QStringLiteral("Normal"), masterId(g), QStringLiteral("Normal")), "connect normal map");
                editor(w)->ApplyGraph();
            });
            until([&] { return inEditor(QStringLiteral("MaterialSlot_NormalMap")); });
            Ui([&](auto* w) { editor(w)->AssignTexture(QStringLiteral("NormalMap"), flatFile); });
            until([&] { return textureField(QStringLiteral("NormalMap")) == QStringLiteral("/Game/Textures/flat-normal.png"); });
            for (int i = 0; i < 20; ++i) pump();
            const QColor flat = previewCenter();
            Ui([&](auto* w) { editor(w)->AssignTexture(QStringLiteral("NormalMap"), tiltFile); });
            until([&] { return textureField(QStringLiteral("NormalMap")) == QStringLiteral("/Game/Textures/tilt-normal.png"); });
            for (int i = 0; i < 20; ++i) pump();
            const QColor tilted = previewCenter();
            Check(std::abs(flat.red() - baseline.red()) <= 8 && std::abs(flat.green() - baseline.green()) <= 8 &&
                  std::abs(flat.blue() - baseline.blue()) <= 8, "flat normal map keeps the surface normal");
            Check(std::abs(tilted.green() - flat.green()) > 12, "tilted normal map changes the lighting");

            // A texture is chosen on a node that is not connected yet: kept in the node, applied once the slot compiles.
            int detail = 0;
            Ui([&](auto* w) {
                auto* g = editor(w)->Graph();
                detail = g->AddNode(QStringLiteral("Texture2D"), QPointF(0, 1100), {{QStringLiteral("textureName"), QStringLiteral("Detail")}});
                editor(w)->ApplyGraph();
                QMetaObject::invokeMethod(w, [g] { g->textureRequested(QStringLiteral("Detail")); }, Qt::QueuedConnection);
            });
            until([&] { return modalIs(QStringLiteral("MaterialTexturePicker")); });
            pickTexture(QStringLiteral("flat-normal.png"));
            until([&] { QString stored; Ui([&](auto* w) { stored = editor(w)->Graph()->NodeTextures().value(QStringLiteral("Detail")); }); return !stored.isEmpty(); });
            Ui([&](auto* w) {
                Check(editor(w)->Graph()->NodeTextures().value(QStringLiteral("Detail")) == QStringLiteral("/Game/Textures/flat-normal.png"), "texture of an unconnected node is kept in the graph");
                Check(!editorWidget(w, QStringLiteral("MaterialSlot_Detail")), "unconnected node has no slot yet");
                auto* g = editor(w)->Graph();
                Check(g->Connect(detail, QStringLiteral("RGB"), masterId(g), QStringLiteral("Emissive")), "connect the detail node");
                editor(w)->ApplyGraph();
            });
            until([&] { return textureField(QStringLiteral("Detail")) == QStringLiteral("/Game/Textures/flat-normal.png"); });

            // Project presets: the current graph is saved as a file in Content/MaterialGraphs and listed in the menu.
            const auto presetFile = root / L"Content/MaterialGraphs/Мой вид.graph.json";
            Ui([&](auto* w) {
                auto* g = editor(w)->Graph();
                QString problem;
                Check(!g->SaveUserPreset(QStringLiteral("../escape"), problem) && !problem.isEmpty(), "preset name cannot leave the folder");
                Check(g->SaveUserPreset(QStringLiteral("Мой вид"), problem), "save the current graph as a preset");
                Check(std::filesystem::exists(presetFile) && g->UserPresets() == QStringList{QStringLiteral("Мой вид")}, "preset file in the project");
                auto* menu = g->template findChild<QMenu*>(QStringLiteral("MaterialGraphPresetMenu"));
                QMetaObject::invokeMethod(menu, "aboutToShow", Qt::DirectConnection);
                bool listed = false;
                for (auto* action : menu->actions()) listed = listed || action->text() == QStringLiteral("Мой вид");
                Check(listed, "preset listed in the menu");
                const int nodes = static_cast<int>(g->Model().Nodes().size());
                g->RemoveNodes({multiply});
                Check(g->LoadUserPreset(QStringLiteral("Мой вид"), problem) && static_cast<int>(g->Model().Nodes().size()) == nodes &&
                      g->Model().FindNode(multiply) != nullptr, "loading the preset restores the graph");
                Check(g->RemoveUserPreset(QStringLiteral("Мой вид")) && !std::filesystem::exists(presetFile) && g->UserPresets().isEmpty(), "preset deleted");
                editor(w)->ApplyGraph();
            });
            for (int i = 0; i < 20; ++i) pump();

            // Undo and redo: graph edits, a dragged slider as one step, values and textures go back to the material too.
            QList<int> kept;
            auto key = [](QWidget* target, int code, Qt::KeyboardModifiers modifiers) {
                QKeyEvent shortcut(QEvent::ShortcutOverride, code, modifiers);
                QApplication::sendEvent(target, &shortcut);
                QKeyEvent press(QEvent::KeyPress, code, modifiers);
                QApplication::sendEvent(target, &press);
            };
            auto specular = [&] { double v = 0; Ui([&](auto* w) { v = static_cast<QDoubleSpinBox*>(editorWidget(w, QStringLiteral("MaterialParam_Specular_0")))->value(); }); return v; };
            Ui([&](auto* w) {
                auto* g = editor(w)->Graph();
                for (const auto& n : g->Model().Nodes()) kept << n.id;
                const int nodes = static_cast<int>(g->Model().Nodes().size());
                const int add = g->AddNode(QStringLiteral("Add"), QPointF(600, 1100));
                Check(g->CanUndo() && !g->CanRedo(), "an edit can be undone");
                g->Undo();
                Check(!g->Model().FindNode(add) && static_cast<int>(g->Model().Nodes().size()) == nodes && g->CanRedo(), "undo removes the added node");
                g->Redo();
                Check(g->Model().FindNode(add) && g->Model().FindNode(add)->type == QStringLiteral("Add"), "redo brings it back with its id");
                Check(g->Connect(add, QStringLiteral("Out"), masterId(g), QStringLiteral("AmbientModel")), "connect for undo");
                g->Undo();
                Check(!hasLink(g, add, QStringLiteral("Out"), masterId(g), QStringLiteral("AmbientModel")) && g->Model().FindNode(add), "undo removes only the link");
                g->View()->setFocus();
                key(g->View(), Qt::Key_Z, Qt::ControlModifier);
                Check(!g->Model().FindNode(add), "Ctrl+Z on the canvas undoes");
                key(g->View(), Qt::Key_Y, Qt::ControlModifier);
                Check(g->Model().FindNode(add) != nullptr, "Ctrl+Y on the canvas redoes");
                g->RemoveNodes({add});

                auto* spin = static_cast<QDoubleSpinBox*>(editorWidget(w, QStringLiteral("MaterialParam_Specular_0")));
                for (const double v : {0.31, 0.32, 0.33}) spin->setValue(v);
                g->Undo();
                for (const auto& n : g->Model().Nodes())
                    if (n.properties.value(QStringLiteral("name")).toString() == QStringLiteral("Specular"))
                        Check(std::abs(n.properties.value(QStringLiteral("value")).toDouble() - 0.8) < 1e-4, "a dragged value is one undo step");
                editor(w)->AssignTexture(QStringLiteral("Detail"), tiltFile);
            });
            until([&] { return std::abs(specular() - 0.8) < 1e-4; });
            until([&] { return textureField(QStringLiteral("Detail")) == QStringLiteral("/Game/Textures/tilt-normal.png"); });
            Ui([&](auto* w) {
                auto* g = editor(w)->Graph();
                Check(g->NodeTextures().value(QStringLiteral("Detail")) == QStringLiteral("/Game/Textures/tilt-normal.png"), "texture choice reaches the node");
                g->Undo();
                Check(g->NodeTextures().value(QStringLiteral("Detail")) == QStringLiteral("/Game/Textures/flat-normal.png"), "undo restores the node texture");
            });
            until([&] { return textureField(QStringLiteral("Detail")) == QStringLiteral("/Game/Textures/flat-normal.png"); });
            Check(std::abs(specular() - 0.8) < 1e-4, "undone value reaches the material settings");

            // Copy, paste and duplicate: links inside the selection come along, names stay unique, textures are shared.
            Ui([&](auto* w) {
                auto* g = editor(w)->Graph();
                Check(!g->CopyNodes({masterId(g)}), "the material output is not copied");
                Check(g->CopyNodes({texture, multiply}) && QGuiApplication::clipboard()->text().contains(QStringLiteral("materialGraphNodes")) && g->CanPaste(),
                      "selection copied as graph JSON");
                const QList<int> pasted = g->PasteNodes(QPointF(700, 1200));
                Check(pasted.size() == 2 && g->SelectedNodes() == pasted, "paste selects the new nodes");
                int newTexture = 0, newMultiply = 0;
                for (const int id : pasted)
                    (g->Model().FindNode(id)->type == QStringLiteral("Texture2D") ? newTexture : newMultiply) = id;
                Check(newTexture && newMultiply && hasLink(g, newTexture, QStringLiteral("RGB"), newMultiply, QStringLiteral("A")), "link inside the selection is pasted");
                Check(g->Model().FindNode(newTexture)->properties.value(QStringLiteral("textureName")).toString() == QStringLiteral("BaseMap"),
                      "pasted texture node reads the same texture");
                Check(std::none_of(g->Model().Connections().begin(), g->Model().Connections().end(),
                                   [&](const auto& c) { return c.toNode == newMultiply && c.toPin == QStringLiteral("B"); }), "links from outside the selection stay behind");
                const QList<int> twin = g->DuplicateNodes({scalar});
                Check(twin.size() == 1 && g->Model().FindNode(twin.first())->properties.value(QStringLiteral("name")).toString() != g->Model().FindNode(scalar)->properties.value(QStringLiteral("name")).toString(),
                      "a duplicated parameter gets its own name");
                const QPointF offset = g->Model().FindNode(twin.first())->pos - g->Model().FindNode(scalar)->pos;
                Check(offset.x() > 0 && offset.y() > 0, "duplicate is placed next to the original");
                g->Undo();
                g->Undo();
                Check(!g->Model().FindNode(twin.first()) && !g->Model().FindNode(newTexture), "paste and duplicate are undone");

                // Keyboard: select, copy, paste, delete.
                g->SelectNodes({scalar});
                g->View()->setFocus();
                key(g->View(), Qt::Key_C, Qt::ControlModifier);
                key(g->View(), Qt::Key_V, Qt::ControlModifier);
                const QList<int> keyed = g->SelectedNodes();
                Check(keyed.size() == 1 && keyed.first() != scalar && g->Model().FindNode(keyed.first())->type == QStringLiteral("ScalarParam"), "Ctrl+C, Ctrl+V");
                key(g->View(), Qt::Key_Delete, Qt::NoModifier);
                Check(!g->Model().FindNode(keyed.first()), "Delete removes the pasted node");
                key(g->View(), Qt::Key_D, Qt::ControlModifier);
                Check(g->SelectedNodes().isEmpty() || !kept.contains(g->SelectedNodes().first()), "Ctrl+D without a selection adds nothing old");
                key(g->View(), Qt::Key_A, Qt::ControlModifier);
                Check(g->SelectedNodes().size() == static_cast<int>(g->Model().Nodes().size()), "Ctrl+A selects every node");
                key(g->View(), Qt::Key_F, Qt::NoModifier);
                g->SelectNodes({});
            });

            // Node search: Russian and English names, synonyms; Enter adds the first match where the list was opened.
            Ui([&](auto* w) {
                auto* g = editor(w)->Graph();
                auto search = [&](const QString& text) {
                    g->ShowNodeSearch(QPoint(200, 200), QPointF(900, 1300));
                    auto* popup = g->template findChild<QFrame*>(QStringLiteral("MaterialGraphNodeSearch"));
                    auto* field = popup ? popup->template findChild<QLineEdit*>(QStringLiteral("MaterialGraphNodeSearchText")) : nullptr;
                    auto* list = popup ? popup->template findChild<QListWidget*>(QStringLiteral("MaterialGraphNodeSearchList")) : nullptr;
                    Check(field && list && popup->isVisible(), "search opens as a popup");
                    field->setText(text);
                    if (text == QStringLiteral("умнож"))
                        Check(popup->grab().save(QString::fromStdWString((root / "editor-material-graph-search.png").wstring())), "save node search");
                    QString first = list->currentItem() ? list->currentItem()->data(Qt::UserRole).toString() : QString();
                    popup->setObjectName({}); // закрытое окно удаляется позже: следующий поиск найдёт новое
                    return std::make_pair(first, field);
                };
                auto [byRussian, field] = search(QStringLiteral("умнож"));
                Check(byRussian == QStringLiteral("Multiply"), "Russian name finds Multiply");
                const int before = static_cast<int>(g->Model().Nodes().size());
                key(field, Qt::Key_Return, Qt::NoModifier);
                Check(static_cast<int>(g->Model().Nodes().size()) == before + 1 && g->Model().Nodes().last().type == QStringLiteral("Multiply") &&
                      g->Model().Nodes().last().pos == QPointF(900, 1300), "Enter adds the node where the list was opened");
                Check(g->SelectedNodes() == QList<int>{g->Model().Nodes().last().id}, "the added node is selected");
                g->RemoveNodes(g->SelectedNodes());
                auto [byEnglish, englishField] = search(QStringLiteral("fresnel"));
                Check(byEnglish == QStringLiteral("Fresnel"), "English type name works");
                key(englishField, Qt::Key_Escape, Qt::NoModifier);
                auto [bySynonym, synonymField] = search(QStringLiteral("край"));
                Check(bySynonym == QStringLiteral("Fresnel"), "a synonym finds the node");
                key(synonymField, Qt::Key_Escape, Qt::NoModifier);
                auto [nothing, nothingField] = search(QStringLiteral("телепорт"));
                Check(nothing.isEmpty(), "unknown words find nothing");
                key(nothingField, Qt::Key_Return, Qt::NoModifier);
                Check(static_cast<int>(g->Model().Nodes().size()) == before, "Enter on no match adds nothing");
                key(nothingField, Qt::Key_Escape, Qt::NoModifier);
            });

            // A compile error is shown on its node: the list names the node, a double click selects it, undo fixes it.
            int clash = 0;
            Ui([&](auto* w) {
                auto* g = editor(w)->Graph();
                clash = g->AddNode(QStringLiteral("ScalarParam"), QPointF(600, 1300), {{QStringLiteral("name"), QStringLiteral("TimeParams")}});
                Check(!g->RenameNode(scalar, QStringLiteral("TimeParams")) && !g->RenameNode(scalar, QStringLiteral("float3")), "rename refuses engine names and HLSL words");
                Check(g->Connect(clash, QStringLiteral("Value"), masterId(g), QStringLiteral("AmbientModel")), "connect the clashing parameter");
                editor(w)->ApplyGraph();
            });
            QListWidgetItem* clashItem = nullptr;
            until([&] {
                Ui([&](auto* w) {
                    auto* errors = static_cast<QListWidget*>(editorWidget(w, QStringLiteral("MaterialErrors")));
                    clashItem = nullptr;
                    for (int i = 0; i < errors->count(); ++i)
                        if (errors->item(i)->data(Qt::UserRole + 2).toInt() == clash && errors->item(i)->text().contains(QStringLiteral("redefinition"))) clashItem = errors->item(i);
                });
                return clashItem != nullptr;
            });
            Ui([&](auto* w) {
                auto* g = editor(w)->Graph();
                auto* errors = static_cast<QListWidget*>(editorWidget(w, QStringLiteral("MaterialErrors")));
                Check(clashItem->text().contains(QStringLiteral("TimeParams")) && !clashItem->text().contains(QStringLiteral(".hlsl(")), "error names the node, not the generated file");
                bool marked = false;
                for (auto* item : g->View()->scene()->items()) marked = marked || item->toolTip().contains(QStringLiteral("TimeParams"));
                Check(marked, "the node shows the error");
                g->SelectNodes({});
                emit errors->itemActivated(clashItem);
                Check(g->SelectedNodes() == QList<int>{clash}, "double click on the error selects the node");
                g->SelectNodes({});
                Check(editor(w)->grab().save(QString::fromStdWString((root / "editor-material-graph-error.png").wstring())), "save graph error");
                g->Undo(); // связь
                g->Undo(); // нода
                Check(!g->Model().FindNode(clash), "undo removes the clashing node");
                editor(w)->ApplyGraph();
            });
            until([&] { int count = -1; Ui([&](auto* w) { count = static_cast<QListWidget*>(editorWidget(w, QStringLiteral("MaterialErrors")))->count(); }); return count == 0; });
            Ui([&](auto* w) {
                auto* g = editor(w)->Graph();
                bool marked = false;
                for (auto* item : g->View()->scene()->items()) marked = marked || item->toolTip().contains(QStringLiteral("TimeParams"));
                Check(!marked, "fixed error leaves the nodes");
                QList<int> extra;
                for (const auto& n : g->Model().Nodes()) if (!kept.contains(n.id)) extra << n.id;
                g->RemoveNodes(extra);
                editor(w)->ApplyGraph();
            });
            for (int i = 0; i < 20; ++i) pump();

            // Every node type at once, every input fed a float3: the engine must compile whatever the links are.
            QList<int> sink;
            Ui([&](auto* w) {
                auto* g = editor(w)->Graph();
                const int source = g->AddNode(QStringLiteral("Vector3Param"), QPointF(-400, 1400), {{QStringLiteral("value"), QVariantList{0.2, 0.4, 0.6}}});
                sink << source;
                int sum = 0;
                QString sumPin;
                const QStringList types{
                    "ColorParam", "ScalarParam", "Vector2Param", "Vector3Param", "Constant", "Split", "Combine", "ObjectColor",
                    "SurfaceDefaults", "InstanceData", "WorldPosition", "SurfaceNormal", "ViewDirection", "FrontFace", "Texture2D",
                    "NormalMap", "UVCoord", "Panner", "Time", "Add", "Subtract", "Multiply", "Divide", "Power", "Min", "Max", "Lerp",
                    "Clamp", "Saturate", "OneMinus", "Abs", "Frac", "Sine", "Normalize", "Step", "Smoothstep", "Dot", "Fresnel"};
                for (const QString& type : types)
                {
                    const int node = g->AddNode(type, QPointF(0, 1400 + 40 * sink.size()));
                    sink << node;
                    for (const auto& pin : EditorCore::HlslGraphCompiler::NodePins(type))
                    {
                        if (pin.isInput) { Check(g->Connect(source, QStringLiteral("Value"), node, pin.name), "feed every input"); continue; }
                        if (!sum) { sum = node; sumPin = pin.name; continue; }
                        const int add = g->AddNode(QStringLiteral("Add"), QPointF(300, 1400 + 40 * sink.size()));
                        sink << add;
                        Check(g->Connect(sum, sumPin, add, QStringLiteral("A")) && g->Connect(node, pin.name, add, QStringLiteral("B")), "sum every output");
                        sum = add;
                        sumPin = QStringLiteral("Out");
                    }
                }
                Check(g->Connect(sum, sumPin, masterId(g), QStringLiteral("AmbientModel")), "connect the sum");
                editor(w)->ApplyGraph();
                const QString generated = static_cast<QPlainTextEdit*>(editorWidget(w, QStringLiteral("MaterialSource")))->toPlainText();
                Check(generated.contains(QStringLiteral("smoothstep(")) && generated.contains(QStringLiteral("defaults.Albedo")) &&
                      generated.contains(QStringLiteral("i.InstanceData")), "every node is in the HLSL");
            });
            for (int i = 0; i < 40; ++i) pump();
            Ui([&](auto* w) {
                auto* errors = static_cast<QListWidget*>(editorWidget(w, QStringLiteral("MaterialErrors")));
                Check(errors->count() == 0, errors->count() ? qPrintable(QStringLiteral("graph with every node compiles: ") + errors->item(0)->text())
                                                            : "graph with every node compiles");
                editor(w)->Graph()->RemoveNodes(sink);
                editor(w)->ApplyGraph();
            });
            for (int i = 0; i < 20; ++i) pump();
            Ui([&](auto* w) {
                Check(static_cast<QListWidget*>(editorWidget(w, QStringLiteral("MaterialErrors")))->count() == 0, "normal map HLSL compiles");
                editor(w)->Graph()->FrameNodes();
                Check(editor(w)->grab().save(QString::fromStdWString((root / "editor-material-graph.png").wstring())), "save graph editor");
                saveMaterial(w);
            });
            until([&] { bool saved = false; Ui([&](auto* w) { saved = editor(w)->isEnabled() && !editor(w)->HasUnsavedChanges(); }); return saved; });
            const QString savedHlsl = readText(graphHlsl), savedGraph = readText(graphJson);
            Check(savedHlsl.contains(QStringLiteral("BaseMap.Sample")) && savedHlsl.contains(QStringLiteral("NormalFromMap")), "saved HLSL comes from the graph");
            Check(readText(graphMat).contains(QStringLiteral("\"NormalMap\": \"../Textures/tilt-normal.png\"")) &&
                  readText(graphMat).contains(QStringLiteral("\"Detail\": \"../Textures/flat-normal.png\"")), "textures saved in the .mat");
            Check(savedGraph.contains(QStringLiteral("\"textureName\": \"Detail\"")) &&
                  savedGraph.contains(QStringLiteral("\"texture\": \"/Game/Textures/flat-normal.png\"")), "graph file keeps names and node textures as text");

            // Deny replacement/deletion but allow reading: QSaveFile can stage bytes, yet its commit fails.
            struct ReplacementLock {
                HANDLE handle;
                explicit ReplacementLock(const std::filesystem::path& file)
                    : handle(CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr))
                { Check(handle != INVALID_HANDLE_VALUE, "lock material file against replacement"); }
                void Release() { if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); handle = INVALID_HANDLE_VALUE; } }
                ~ReplacementLock() { Release(); }
            };
            const auto answerMaterialDialog = [&](QMessageBox::StandardButton answer, const QString& failedPath = QString()) {
                until([&] { bool modal = false; Ui([&](auto*) { modal = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) != nullptr; }); return modal; });
                Ui([&](auto*) {
                    auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                    if (!failedPath.isEmpty()) {
                        Check(dialog->icon() == QMessageBox::Warning &&
                              QString(dialog->text()).replace('\\', '/').contains(QString(failedPath).replace('\\', '/')),
                              qPrintable(QStringLiteral("restore failure identifies the locked file: ") + dialog->text()));
                        Check(dialog->text().contains(QStringLiteral("Резервная копия")), "restore failure explains that backup survives");
                    }
                    Check(dialog->button(answer) != nullptr, "material dialog offers expected action");
                    dialog->button(answer)->click();
                });
            };
            for (const bool discard : {false, true})
            {
                Ui([&](auto* w) { editor(w)->Graph()->RemoveNodes({multiply}); editor(w)->ApplyGraph(); });
                until([&] { return !readText(graphHlsl).contains(QStringLiteral("BaseMap.Sample")); });
                const auto lockedPath = discard ? graphJson : graphHlsl;
                ReplacementLock lock(lockedPath);
                Ui([&](auto* w) {
                    QTimer::singleShot(0, w, [&, w] {
                        if (discard) w->close();
                        else editor(w)->Revert();
                    });
                });
                answerMaterialDialog(discard ? QMessageBox::Discard : QMessageBox::Yes);
                answerMaterialDialog(QMessageBox::Ok, QString::fromStdWString(lockedPath.wstring()));
                if (discard) {
                    Ui([](auto* w) { Check(w->isVisible(), "file restore failure vetoes closing the main editor"); });
                    Check(readText(graphHlsl) == savedHlsl && readText(graphJson) != savedGraph,
                          "graph write failure follows a partial HLSL restore");
                }
                Ui([&](auto* w) {
                    Check(editor(w) && editor(w)->isVisible() && editor(w)->HasUnsavedChanges(), "failed restore keeps dirty editor open");
                    Check(!editor(w)->Graph()->Model().FindNode(multiply), "failed restore does not reset the editable graph");
                });
                lock.Release();
                Ui([&](auto* w) {
                    if (discard) QMetaObject::invokeMethod(editor(w), "close", Qt::QueuedConnection);
                    else QTimer::singleShot(0, editor(w), [&, w] { editor(w)->Revert(); });
                });
                answerMaterialDialog(discard ? QMessageBox::Discard : QMessageBox::Yes);
                until([&] {
                    bool done = false;
                    Ui([&](auto* w) { auto* e = w->template findChild<MaterialEditorWindow*>(); done = discard ? !e : e && !e->HasUnsavedChanges(); });
                    return done;
                });
                Check(readText(graphHlsl) == savedHlsl && readText(graphJson) == savedGraph,
                      "retry after unlocking restores both original files");
            }

            Ui([&](auto* w) { w->OpenMaterialEditor(QString::fromStdWString(graphMat.wstring())); });
            until([&] { return inEditor(QStringLiteral("MaterialSlot_BaseMap")); });
            Ui([&](auto* w) {
                auto* e = editor(w);
                Check(e->HasGraph() && e->Graph()->Model().FindNode(multiply) != nullptr, "graph reopens from its file");
                Check(static_cast<QPlainTextEdit*>(editorWidget(w, QStringLiteral("MaterialSource")))->isReadOnly(), "reopened graph HLSL is read-only");
                e->DisableGraph();
                Check(!e->HasGraph() && !std::filesystem::exists(graphJson) &&
                      !static_cast<QPlainTextEdit*>(editorWidget(w, QStringLiteral("MaterialSource")))->isReadOnly(), "back to code: graph removed, HLSL editable");
                saveMaterial(w);
            });
            until([&] { bool saved = false; Ui([&](auto* w) { saved = editor(w)->isEnabled() && !editor(w)->HasUnsavedChanges(); }); return saved; });
            Check(!std::filesystem::exists(graphJson) && readText(graphHlsl) == savedHlsl, "code material keeps the last graph HLSL");
            // A newly enabled graph must be deleted on Discard; failed deletion must also keep the backup.
            Ui([&](auto* w) { editor(w)->EnableGraph(); });
            ReplacementLock newGraphLock(graphJson);
            Ui([](auto* w) { QMetaObject::invokeMethod(w->template findChild<MaterialEditorWindow*>(), "close", Qt::QueuedConnection); });
            answerMaterialDialog(QMessageBox::Discard);
            answerMaterialDialog(QMessageBox::Ok, QString::fromStdWString(graphJson.wstring()));
            Ui([&](auto* w) { Check(editor(w)->isVisible() && editor(w)->HasUnsavedChanges(), "failed graph deletion keeps backup and window"); });
            Check(std::filesystem::exists(graphJson), "locked new graph has not been deleted");
            newGraphLock.Release();
            Ui([](auto* w) { QMetaObject::invokeMethod(w->template findChild<MaterialEditorWindow*>(), "close", Qt::QueuedConnection); });
            answerMaterialDialog(QMessageBox::Discard);
            until([&] { bool gone = false; Ui([&](auto* w) { gone = w->template findChild<MaterialEditorWindow*>() == nullptr; }); return gone; });
            Check(!std::filesystem::exists(graphJson) && readText(graphHlsl) == savedHlsl, "retry deletes new graph and restores original code material");
        }
        Ui([](auto* w) { w->activateWindow(); });

        // "Редактировать" on the default material: a new material is created, assigned and opened.
        Ui([](auto* w) {
            auto* tree = w->findChild<QTreeWidget*>();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->text(1) == QStringLiteral("Primitive.Sphere")) tree->setCurrentItem(tree->topLevelItem(i));
        });
        until([&] { return host.ReadSnapshot().selected != 0; });
        const uint64_t selected = host.ReadSnapshot().selected;
        Ui([](auto* w) {
            auto* edit = w->template findChild<QPushButton*>(QStringLiteral("EditMaterial"));
            Check(edit && edit->isEnabled(), "edit enabled with the default material");
            edit->click();
        });
        std::string assigned;
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (o.id == selected) assigned = o.materialAsset; return !assigned.empty(); });
        Check(assigned.rfind("/Game/Materials/", 0) == 0 && std::filesystem::exists(root / "Content" / std::filesystem::u8path(assigned.substr(6))),
              "default material replaced by a new asset");
        until([&] { return inEditor(QStringLiteral("MaterialParam_Gloss_0")); });
        Ui([](auto* w) { QMetaObject::invokeMethod(w->template findChild<MaterialEditorWindow*>(), "close", Qt::QueuedConnection); });
        until([&] { bool gone = false; Ui([&](auto* w) { gone = w->template findChild<MaterialEditorWindow*>() == nullptr; }); return gone; });
        // F6 is the editor's shader reload (games no longer have it): every scene and preview recompiles.
        Ui([](auto* w) {
            QAction* reload = nullptr;
            for (auto* action : w->template findChildren<QAction*>()) if (action->shortcut() == QKeySequence(Qt::Key_F6)) reload = action;
            Check(reload && reload->shortcutContext() == Qt::ApplicationShortcut, "F6 reloads shaders in every editor window");
            reload->trigger();
        });
        until([&] { bool done = false; Ui([&](auto* w) {
            done = w->template findChild<QPlainTextEdit*>(QStringLiteral("Console"))->toPlainText().contains(QStringLiteral("перекомпилированы")); }); return done; });
        Ui([](auto* w) { w->activateWindow(); }); // focused-field checks below need the main window active
        if (argc == 4)
        {
            const auto modelSource = std::filesystem::absolute(argv[3]);
            Ui([&](auto* w) { w->ImportAsset(false, QString::fromStdWString(modelSource.wstring())); });
            auto importedName = modelSource.filename();
            if (modelSource.extension() == ".blend") importedName.replace_extension(".glb");
            const auto importedModel = root / "Content/Models" / modelSource.stem() / importedName;
            until([&] { return std::filesystem::exists(importedModel); });
            const auto beforeReplace = host.ReadSnapshot();
            for (const auto& o : beforeReplace.objects) if (o.type == "Primitive.Sphere") {
                EditorCommand select; select.type=EditorCommand::SelectObject; select.objectId=o.id; host.PostCommand(select); pump();
                EditorCommand replace; replace.type=EditorCommand::SetModel; replace.objectId=o.id;
                replace.path=QStringLiteral("/Game/")+QString::fromStdWString(std::filesystem::relative(importedModel,root/"Content").generic_wstring()); host.PostCommand(replace);
            }
            until([&] { for(const auto& o:host.ReadSnapshot().objects) if(o.type=="Primitive.Sphere" && !o.modelAsset.empty()) return true; return false; });
            Check(host.ReadSnapshot().objects.size()==6 && host.ReadSnapshot().selected!=0,"replace primitive model preserves count and selection");
            Ui([](auto* w) { Action(w,QStringLiteral("Отменить"))->trigger(); });
            until([&] { for(const auto& o:host.ReadSnapshot().objects) if(o.type=="Primitive.Sphere" && o.modelAsset.empty()) return true; return false; });
            Ui([&](auto* w) { w->AddModel(QString::fromStdWString(importedModel.wstring())); });
            until([&] { return host.ReadSnapshot().objects.size() == 7; });
            if (std::filesystem::exists(std::filesystem::path(importedModel.wstring() + L".import.json")))
            {
                const auto snapshot = host.ReadSnapshot();
                for (const auto& object : snapshot.objects)
                    if (object.id == snapshot.selected)
                        Check(!object.materialAsset.empty() && object.textureAsset.empty(), "new imported model has its BaseMap material");
                Ui([&](auto* w) {
                    Check(w->template findChild<QPlainTextEdit*>(QStringLiteral("Console"))->toPlainText().contains(QStringLiteral("Создан базовый материал")),
                          "editor reports automatic material creation");
                });
            }
            Ui([&](auto* w) { w->UseAsset(QString::fromStdWString(importedImage.wstring())); });
            until([&] { for (const auto& o : host.ReadSnapshot().objects) if (!o.modelAsset.empty() && !o.textureAsset.empty()) return true; return false; });
            Ui([&](auto* w) {
                for (auto* dock : w->template findChildren<QDockWidget*>()) if (dock->windowTitle() == QStringLiteral("Ресурсы и код")) dock->raise();
                Check(w->grab().save(QString::fromStdWString((root / "editor-assets.png").wstring())), "save editor asset UI preview");
            });
            Ui([](auto* w) { Action(w, QStringLiteral("Дублировать объект"))->trigger(); });
            until([&] { return host.ReadSnapshot().objects.size() == 8; });
            for (const auto& o : host.ReadSnapshot().objects) if (!o.modelAsset.empty()) Check(!o.textureAsset.empty(), "duplicate preserves model texture");
            Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
            until([&] { return host.ReadSnapshot().objects.size() == 7; });
            Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
            until([&] { for (const auto& o : host.ReadSnapshot().objects) if (!o.modelAsset.empty() && !o.textureAsset.empty()) return false; return true; });
            Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
            until([&] { return host.ReadSnapshot().objects.size() == 6; });
        }
        Ui([](auto* w) {
            auto* tree = w->findChild<QTreeWidget*>();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->text(1) == QStringLiteral("Primitive.Sphere")) tree->setCurrentItem(tree->topLevelItem(i));
        });
        until([&] { return host.ReadSnapshot().selected != 0; });
        auto selectedX = [&] { const auto view = host.ReadSnapshot(); for (const auto& o : view.objects) if (o.id == view.selected) return o.transform.position[0]; return -999.0f; };
        const float originalX = selectedX();
        Ui([](auto* w) { auto* x = w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0")); Check(x && x->isEnabled(), "inspector enabled"); x->setValue(3.25); QMetaObject::invokeMethod(x, "editingFinished", Qt::DirectConnection); });
        until([&] { return selectedX() == 3.25f; });
        Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
        until([&] { return host.ReadSnapshot().selected == 0; });
        bool restored = false; for (const auto& o : host.ReadSnapshot().objects) if (o.type == "Primitive.Sphere" && o.transform.position[0] == originalX) restored = true;
        Check(restored, "UI undo restores transform");
        Ui([](auto* w) { Action(w, QStringLiteral("Повторить"))->trigger(); });
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (o.type == "Primitive.Sphere" && o.transform.position[0] == 3.25f) return true; return false; });
        // The merged panels must keep main's gizmo modes and one Undo per drag.
        Ui([](auto* w) {
            auto* viewport = w->findChild<ViewportWidget*>();
            w->activateWindow(); viewport->setFocus();
        });
        for (const auto& mode : {std::pair<int, EditorGizmoMode>{Qt::Key_2, EditorGizmoRotate},
                                {Qt::Key_3, EditorGizmoScale}, {Qt::Key_1, EditorGizmoTranslate}}) {
            Ui([&](auto* w) {
                auto* viewport = w->findChild<ViewportWidget*>();
                QKeyEvent event(QEvent::KeyPress, mode.first, Qt::NoModifier);
                QApplication::sendEvent(viewport, &event);
                Check(viewport->gizmoMode() == mode.second, "1/2/3 select the gizmo mode");
            });
            until([&] { return host.GizmoMode() == mode.second; });
        }
        EditorQtHost::SceneObject sphere;
        for (const auto& o : host.ReadSnapshot().objects) if (o.type == "Primitive.Sphere") sphere = o;
        Check(sphere.id != 0, "gizmo target exists after undo/redo");
        Ui([](auto* w) {
            auto* tree = w->findChild<QTreeWidget*>();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->text(1) == QStringLiteral("Primitive.Sphere")) tree->setCurrentItem(tree->topLevelItem(i));
        });
        until([&] { return host.ReadSnapshot().selected == sphere.id; });
        QDoubleSpinBox* inspectorX = nullptr;
        Ui([&](auto* w) { inspectorX = w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0")); });
        Check(inspectorX != nullptr, "inspector position field exists before the drag");
        EditorCommand drag; drag.type = EditorCommand::BeginGizmoDrag; drag.objectId = sphere.id;
        host.PostCommand(drag);
        EditorCommand transform; transform.type = EditorCommand::UpdateObjectTransform;
        transform.objectId = sphere.id; transform.path = QStringLiteral("gizmo"); transform.transform = sphere.transform;
        for (int i = 1; i <= 200; ++i) { transform.transform.position[0] = sphere.transform.position[0] + i * 0.01f; host.PostCommand(transform); }
        const float dragX = transform.transform.position[0];
        transform.transform.scale[0] = 2; host.PostCommand(transform);
        transform.transform.scale[0] = 3; host.PostCommand(transform);
        drag.type = EditorCommand::EndGizmoDrag; host.PostCommand(drag);
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (o.id == sphere.id) return o.transform.scale[0] == 3 && o.transform.position[0] == dragX; return false; });
        until([&] { double shown = 0; Ui([&](auto* w) { if (auto* x = w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0"))) shown = x->value(); }); return std::abs(shown - dragX) < 0.01; });
        Ui([&](auto* w) { Check(w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0")) == inspectorX, "gizmo drag updates inspector Transform fields without rebuilding them"); });
        Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (o.name == sphere.name) return o.transform.scale[0] == sphere.transform.scale[0] && o.transform.position[0] == sphere.transform.position[0]; return false; });
        Check(true, "one Undo restores the transform before the whole gizmo drag");
        // Select the sphere through the actual tree before testing a focused-field save.
        Ui([](auto* w) {
            auto* tree = w->findChild<QTreeWidget*>();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->text(1) == QStringLiteral("Primitive.Sphere")) tree->setCurrentItem(tree->topLevelItem(i));
        });
        until([&] { return selectedX() == 3.25f; });
        Ui([](auto* w) { auto* x = w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0")); x->setFocus(); x->setValue(4.5); Action(w, QStringLiteral("Сохранить сцену"))->trigger(); });
        until([&] { return selectedX() == 4.5f && !host.ReadSnapshot().dirty; });
        QFile file(QString::fromStdWString(sceneFile.wstring())); Check(file.open(QIODevice::ReadOnly), "saved scene readable");
        Check(QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("objects")).toArray().size() == 6, "UI save persists all objects"); file.close();
        Ui([](auto* w) { Add(w, 6); });
        until([&] { return host.ReadSnapshot().objects.size() == 7; });
        auto selectedLight = [&] {
            const auto view = host.ReadSnapshot();
            for (const auto& o : view.objects) if (o.id == view.selected && o.hasPointLight) return o;
            return EditorQtHost::SceneObject{};
        };
        Check(selectedLight().hasPointLight, "new light selected");
        Ui([](auto* w) {
            auto* button = w->findChild<QPushButton*>(QStringLiteral("LightColor"));
            Check(button && button->isVisible(), "light color button visible"); button->click();
            auto* dialog = w->findChild<QColorDialog*>(); Check(dialog != nullptr, "color picker opens");
            dialog->setCurrentColor(QColor::fromRgbF(0.2f,0.8f,0.4f)); dialog->accept();
        });
        until([&] { return std::abs(selectedLight().pointLight.color[1] - 0.8f) < 0.001f; });
        Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
        until([&] { return !host.ReadSnapshot().selected; });
        bool whiteRestored = false;
        for (const auto& o : host.ReadSnapshot().objects) if (o.name == "Точечный свет") whiteRestored = o.pointLight.color[0] == 1;
        Check(whiteRestored, "undo restores previous light color");
        Ui([](auto* w) { Action(w, QStringLiteral("Повторить"))->trigger(); });
        until([&] {
            for (const auto& o : host.ReadSnapshot().objects) if (o.name == "Точечный свет") return std::abs(o.pointLight.color[1] - 0.8f) < 0.001f;
            return false;
        });
        Ui([](auto* w) {
            auto* tree = w->findChild<QTreeWidget*>();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->text(0) == QStringLiteral("Точечный свет")) tree->setCurrentItem(tree->topLevelItem(i));
        });
        until([&] { return selectedLight().id != 0; });
        Ui([](auto* w) {
            auto* intensity = w->findChild<QDoubleSpinBox*>(QStringLiteral("LightValue3"));
            intensity->setValue(4); QMetaObject::invokeMethod(intensity, "editingFinished", Qt::DirectConnection);
        });
        until([&] { return selectedLight().pointLight.intensity == 4; });
        Ui([](auto* w) { w->findChild<QCheckBox*>(QStringLiteral("LightEnabled"))->click(); });
        until([&] { return !selectedLight().pointLight.enabled; });
        Ui([](auto* w) { w->findChild<QCheckBox*>(QStringLiteral("LightEnabled"))->click(); });
        until([&] { return selectedLight().pointLight.enabled; });
        Ui([](auto* w) {
            auto* x = w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0"));
            x->setValue(-3); QMetaObject::invokeMethod(x, "editingFinished", Qt::DirectConnection);
        });
        until([&] { return selectedLight().transform.position[0] == -3; });
        Ui([&](auto* w) {
            Check(w->findChild<QLabel*>(QStringLiteral("SelectedObjectLabel"))->text().contains(QStringLiteral("Точечный свет")),"inspector identifies selected object");
            Check(w->grab().save(QString::fromStdWString((root / "editor-light.png").wstring())), "save light inspector preview");
        });
        Ui([](auto* w) { Action(w, QStringLiteral("Дублировать объект"))->trigger(); });
        until([&] { return host.ReadSnapshot().objects.size() == 8; });
        Check(selectedLight().pointLight.intensity == 4 && std::abs(selectedLight().pointLight.color[1] - 0.8f) < 0.001f &&
            selectedLight().transform.position[0] == -2, "duplicate preserves light properties and offsets position");
        Ui([](auto* w) { Action(w, QStringLiteral("Удалить объект"))->trigger(); });
        until([&] { return host.ReadSnapshot().objects.size() == 7; });
        Ui([](auto* w) { Action(w, QStringLiteral("Сохранить сцену"))->trigger(); });
        until([&] { return !host.ReadSnapshot().dirty; });
        Ui([](auto* w) { Action(w, QStringLiteral("▶ Play"))->trigger(); });
        until([&] { return host.PlayState() == EditorQtHost::Play; });
        Ui([](auto* w) { Check(!w->findChild<QPushButton*>(QStringLiteral("SceneAddButton"))->isEnabled(), "editing disabled during Play"); Action(w, QStringLiteral("⏹ Stop"))->trigger(); });
        until([&] { return host.PlayState() == EditorQtHost::Edit; });
        Ui([](auto* w) {
            auto* tree = w->findChild<QTreeWidget*>();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->text(0) == QStringLiteral("Точечный свет")) tree->setCurrentItem(tree->topLevelItem(i));
        });
        until([&] { return selectedLight().id != 0; });
        Ui([](auto* w) { Action(w, QStringLiteral("Удалить объект"))->trigger(); });
        until([&] { return host.ReadSnapshot().objects.size() == 6; });
        Ui([](auto* w) { Add(w, 4); Action(w, QStringLiteral("Новая сцена"))->trigger(); });
        until([&] { bool modal = false; Ui([&](auto*) { modal = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) != nullptr; }); return modal; });
        Check(host.ReadSnapshot().objects.size() == 7, "queued addition preserved before New scene prompt");
        Ui([](auto*) { qobject_cast<QMessageBox*>(QApplication::activeModalWidget())->done(QMessageBox::Cancel); });
        for (int i = 0; i < 30; ++i) pump();
        Check(host.ReadSnapshot().objects.size() == 7 && host.ReadSnapshot().dirty, "cancel New keeps scene");
        Ui([&](auto* w) {
            const auto file=QString::fromStdWString((root/"Content/UI/EditorHud.ui").wstring());
            Check(!QFile::exists(QString::fromStdWString((root/"Content/ProjectHud.txt").wstring())),"new project has no HUD selection");
            Check(w->CreateUI(file)==file,"create UI through editor");
            UIEditorWindow* ui=nullptr;for(auto* widget:QApplication::topLevelWidgets())if(auto* candidate=dynamic_cast<UIEditorWindow*>(widget))if(QFileInfo(candidate->FilePath()).absoluteFilePath()==QFileInfo(file).absoluteFilePath())ui=candidate;
            Check(ui&&ui->Root(),"visual UI editor loaded");
            ui->AddElement("Panel");auto* panel=dynamic_cast<UIPanel*>(ui->Selected());Check(panel,"add panel");
            panel->Padding={8,9,10,11};panel->BorderWidth=3;panel->bInteractive=true;
            ui->AddElement("Label");auto* label=dynamic_cast<UILabel*>(ui->Selected());Check(label&&label->GetParent()==panel,"label nested in panel");
            auto* text=ui->findChild<QLineEdit*>("UIText");text->setText(QStringLiteral("Здоровье & энергия"));QMetaObject::invokeMethod(text,"editingFinished",Qt::DirectConnection);
            ui->findChild<QDoubleSpinBox*>("UIRect0")->setValue(24);
            ui->findChild<QComboBox*>("UIAnchor")->setCurrentIndex(int(UIAnchor::TopRight));
            Check(label->GetText()=="Здоровье & энергия"&&label->Rect.X==24&&label->Rect.Anchor==UIAnchor::TopRight,"inspector changes label");
            ui->Select(panel);ui->AddElement("Button");Check(dynamic_cast<UIButton*>(ui->Selected()),"add button");
            ui->findChild<QDoubleSpinBox*>("UIRect1")->setValue(80);
            Check(ui->Selected()->Rect.Y==80,"UI rectangle value change updates the model without losing focus");
            QTimer::singleShot(60,ui,[]{auto* dialog=qobject_cast<QColorDialog*>(QApplication::activeModalWidget());Check(dialog,"UI color dialog");dialog->setCurrentColor(QColor(32,180,210,128));dialog->accept();});
            ui->findChild<QPushButton*>("UIColor")->click();
            Check(std::abs(dynamic_cast<UIButton*>(ui->Selected())->Color.a-128.0f/255)<0.01,"UI color alpha");
            ui->Select(panel);ui->AddElement("Bar");ui->findChild<QDoubleSpinBox*>("UIRect1")->setValue(130);
            Check(ui->Save(),"save UI atomically");Check(ui->Reload(),"reload UI");
            auto* loaded=dynamic_cast<UIPanel*>(ui->Root()->Find("panel1"));Check(loaded&&loaded->Padding.Left==8&&loaded->BorderWidth==3&&loaded->bInteractive,"unexposed supported properties survive save");
            Check(ui->Root()->FindAs<UILabel>("label1")->GetText()=="Здоровье & энергия","escaped UTF8 text round trip");
            ui->findChild<QComboBox*>("UIPreviewSize")->setCurrentIndex(2);QApplication::processEvents();
            Check(ui->findChild<QWidget*>("UICanvas")->grab().toImage().size().width()>0,"preview without Play");
            auto* canvas=ui->findChild<QWidget*>("UICanvas");auto* button=ui->Root()->FindAs<UIButton>("button1");
            const double scale=std::min(double(canvas->width())/800,double(canvas->height())/600);
            const auto box=button->GetBox();const QPointF point((box.X+8)*scale,(box.Y+8)*scale);const float oldX=button->Rect.X;
            QMouseEvent press(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(canvas,&press);
            Check(ui->Selected()==button,"canvas selection synchronizes hierarchy and inspector");
            QMouseEvent move(QEvent::MouseMove,point+QPointF(20*scale,10*scale),point+QPointF(20*scale,10*scale),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(canvas,&move);
            QMouseEvent release(QEvent::MouseButtonRelease,point,point,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(canvas,&release);
            Check(std::abs(button->Rect.X-oldX-20)<0.01,"canvas drag updates runtime rect");Check(ui->Save(),"save drag");
            QApplication::processEvents();
            ui->grab().save(QString::fromStdWString((root/"ui-editor.png").wstring()));
            ui->Select(loaded);ui->AddElement("Label");auto deleted=ui->Selected()->Name;ui->DeleteSelected();Check(!ui->Root()->Find(deleted),"delete UI element");Check(ui->Save(),"save after delete");
            QTimer::singleShot(60,w,[]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());Check(dialog,"HUD settings dialog");auto* choice=dialog->findChild<QComboBox*>("ProjectHudChoice");Check(choice&&choice->currentData().toString().isEmpty(),"HUD defaults to none");int index=choice->findData("UI/EditorHud.ui");Check(index>=0,"existing UI listed");choice->setCurrentIndex(index);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});
            w->ShowHudSettings();
            QTimer::singleShot(60,w,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());auto* choice=dialog->findChild<QComboBox*>("ProjectHudChoice");Check(choice->currentData().toString()=="UI/EditorHud.ui","HUD selection survives reopening settings");dialog->grab().save(QString::fromStdWString((root/"hud-settings.png").wstring()));dialog->reject();});w->ShowHudSettings();
            ui->close();
        });
        Ui([](auto* w) { w->close(); });
        until([&] { bool modal = false; Ui([&](auto*) { modal = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) != nullptr; }); return modal; });
        Check(!exit, "unsaved close waits for a decision");
        Ui([](auto*) { auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); Check(box, "unsaved changes prompt"); box->done(QMessageBox::Cancel); });
        for (int i = 0; i < 30; ++i) pump();
        Check(!exit && host.ReadSnapshot().dirty, "cancel preserves editing session");
        Ui([](auto* w) { Action(w, QStringLiteral("Сохранить сцену"))->trigger(); });
        until([&] { return !host.ReadSnapshot().dirty; });
        Ui([](auto* w) { Add(w,11); });
        until([&] { for(const auto& o:host.ReadSnapshot().objects)if(o.gameplayKind==1)return true;return false; });
        // Closing the HUD editor/dialogs can leave the main window inactive on Windows.
        // Establish keyboard focus before testing that an inspector refresh preserves it.
        Ui([](auto* w) { w->activateWindow(); w->raise(); });
        for (int i = 0; i < 10; ++i) pump();
        Ui([](auto* w) {
            auto* speed=w->findChild<QDoubleSpinBox*>("Property/moveSpeed"); Check(speed && speed->isVisible(),"character settings in inspector");
            speed->setFocus();
            Check(speed->hasFocus() || speed->isAncestorOf(QApplication::focusWidget()),"property focus established before refresh");
            speed->setValue(7); QMetaObject::invokeMethod(speed,"editingFinished",Qt::DirectConnection);
        });
        until([&] { for(const auto& o:host.ReadSnapshot().objects) if(o.gameplayKind==1) return QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object()["moveSpeed"].toDouble()==7; return false; });
        Ui([](auto* w) {
            auto* speed=w->findChild<QDoubleSpinBox*>("Property/moveSpeed"); Check(speed->hasFocus() || speed->isAncestorOf(QApplication::focusWidget()),"refresh does not steal property focus");
            auto* player=w->findChild<QCheckBox*>("Property/playerControlled");
            Check(player && w->findChild<QComboBox*>("Property/orthographic") && w->findChild<QComboBox*>("Property/viewPlane") && w->findChild<QDoubleSpinBox*>("Property/fov"),"player camera controls in properties");
            Check(!w->findChild<QToolButton*>("ConfigPlayBtn"),"Play settings removed from toolbar");
            for(auto* action:w->findChildren<QAction*>())Check(action->text()!=QStringLiteral("Настроить Play…"),"Play settings removed from menu");
            player->click();
        });
        until([&] {for(const auto& o:host.ReadSnapshot().objects)if(o.gameplayKind==1)return QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object()["playerControlled"].toBool();return false;});
        Ui([&](auto* w){w->grab().save(QString::fromStdWString((root/"player-properties.png").wstring()));});
        Ui([](auto* w) { Action(w,QStringLiteral("Сохранить сцену"))->trigger(); }); until([&] { return !host.ReadSnapshot().dirty; });
        Ui([](auto* w) { Action(w,QStringLiteral("▶ Play"))->trigger(); });until([&]{return host.PlayState()==EditorQtHost::Play;});
        auto heroZ=[&]{float z=999;EngineSceneInfo info{};api.readActiveScene(api.context,&info,[](void* p,const EngineSceneObjectInfo* o){if(std::string(o->typeName)=="Gameplay.Character")*static_cast<float*>(p)=o->transform.position[2];},&z);return z;};
        auto key=[&](QEvent::Type type,int value){Ui([&](auto* w){auto* viewport=w->template findChild<ViewportWidget*>();Check(viewport,"viewport widget");QKeyEvent event(type,value,Qt::NoModifier);QApplication::sendEvent(viewport,&event);});};
        Ui([](auto* w){w->activateWindow();w->raise();auto* v=w->template findChild<ViewportWidget*>();v->setFocus();});
        for(int i=0;i<10;++i)pump();key(QEvent::KeyPress,Qt::Key_W);for(int i=0;i<10;++i)pump();Check(heroZ()<-.2f,"real viewport key moves character");
        key(QEvent::KeyRelease,Qt::Key_W);pump();float stopped=heroZ();
        Ui([](auto* w){auto* text=new QLineEdit(w);text->setObjectName("InputTextProbe");text->show();text->setFocus();QKeyEvent event(QEvent::KeyPress,Qt::Key_W,Qt::NoModifier,"w");QApplication::sendEvent(text,&event);});
        for(int i=0;i<10;++i)pump();Check(heroZ()==stopped,"typing in editor does not move character");
        Ui([](auto* w){delete w->template findChild<QLineEdit*>("InputTextProbe");Action(w,QStringLiteral("⏹ Stop"))->trigger();});until([&]{return host.PlayState()==EditorQtHost::Edit;});
        Check(heroZ()==0,"Stop restores authored character");
        auto sunObject = [&] { for (const auto& object : host.ReadSnapshot().objects) if(object.type=="Light.Directional") return object; return EditorQtHost::SceneObject{}; };
        auto sunSettings = [&] { return QJsonDocument::fromJson(QByteArray::fromStdString(sunObject().projectData)).object(); };
        Ui([](auto* w) { w->ShowDirectionalLight(); }); until([&] { return sunObject().id && host.ReadSnapshot().selected==sunObject().id; });
        const auto withSun = host.ReadSnapshot().objects.size();
        Ui([](auto* w) { auto* intensity=w->template findChild<QDoubleSpinBox*>("Property/intensity"); auto* color=w->template findChild<QPushButton*>("Property/color"); Check(intensity && color && w->template findChild<QCheckBox*>("Property/shadows"),"directional light inspector controls"); intensity->setValue(2.5); QMetaObject::invokeMethod(intensity,"editingFinished",Qt::DirectConnection); });
        until([&] { return sunSettings()["intensity"].toDouble()==2.5; });
        Ui([](auto* w) { Action(w,QStringLiteral("Отменить"))->trigger(); }); until([&] { return sunSettings()["intensity"].toDouble()==1; });
        Ui([](auto* w) { Action(w,QStringLiteral("Повторить"))->trigger(); }); until([&] { return sunSettings()["intensity"].toDouble()==2.5; });
        Ui([](auto* w) { w->ShowDirectionalLight(); }); until([&] { return host.ReadSnapshot().selected==sunObject().id; });
        Check(host.ReadSnapshot().objects.size()==withSun,"global sun action selects existing singleton");
        Ui([](auto* w) { auto* yaw=w->template findChild<QDoubleSpinBox*>("Transform4"); yaw->setValue(65); QMetaObject::invokeMethod(yaw,"editingFinished",Qt::DirectConnection); w->template findChild<QCheckBox*>("Property/shadows")->click(); });
        until([&] { return !sunSettings()["shadows"].toBool(); });
        const auto sunRotation = sunObject().transform;
        Check(std::abs(sunRotation.rotation[1])>0.1f,"inspector rotates directional light");
        Ui([](auto* w) { QTimer::singleShot(80,w,[]{ auto* dialog=qobject_cast<QColorDialog*>(QApplication::activeModalWidget());Check(dialog,"sun color dialog"); dialog->setCurrentColor(QColor::fromRgbF(0.2,0.6,1));dialog->accept(); }); w->template findChild<QPushButton*>("Property/color")->click(); });
        until([&] { return std::abs(sunSettings()["color"].toArray()[0].toDouble()-0.2)<0.01; });
        Ui([](auto* w) { Action(w,QStringLiteral("Сохранить сцену"))->trigger(); }); until([&] { return !host.ReadSnapshot().dirty; });
        Ui([](auto* w) { Action(w,QStringLiteral("▶ Play"))->trigger(); }); until([&] { return host.PlayState()==EditorQtHost::Play; });
        Check(sunSettings()["intensity"].toDouble()==2.5,"Play keeps authored sun");
        Ui([](auto* w) { Action(w,QStringLiteral("⏹ Stop"))->trigger(); }); until([&] { return host.PlayState()==EditorQtHost::Edit; });
        Ui([](auto* w) { w->ShowDirectionalLight(); });
        until([&] { return host.ReadSnapshot().selected==sunObject().id; });
        Ui([&](auto* w) { w->grab().save(QString::fromStdWString((root / "directional-light-inspector.png").wstring())); auto* viewport=w->template findChild<ViewportWidget*>();Check(viewport,"viewport exists");viewport->setFocus();
            QKeyEvent key(QEvent::KeyPress,Qt::Key_Delete,Qt::NoModifier);QApplication::sendEvent(viewport,&key); });
        until([&] { return !sunObject().id; });
        Ui([](auto* w) { Action(w,QStringLiteral("Отменить"))->trigger(); }); until([&] { return sunObject().id && sunSettings()["intensity"].toDouble()==2.5; });
        Ui([](auto* w) { Action(w,QStringLiteral("Сохранить сцену"))->trigger(); }); until([&] { return !host.ReadSnapshot().dirty; });
        Ui([](auto* w) { Add(w,12); });
        until([&] { for(const auto& o:host.ReadSnapshot().objects)if(o.gameplayKind==2)return true;return false; });
        Ui([](auto* w) { auto* projection=w->template findChild<QComboBox*>("Property/orthographic"); auto* plane=w->template findChild<QComboBox*>("Property/viewPlane"); Check(projection && plane && projection->count()==2 && plane->count()==4,"camera projection and 2D plane choices"); projection->setCurrentIndex(1); QMetaObject::invokeMethod(projection,"activated",Q_ARG(int,1)); });
        until([&]{for(const auto& o:host.ReadSnapshot().objects)if(o.gameplayKind==2)return QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object()["orthographic"].toBool();return false;});
        Ui([](auto* w) { Action(w,QStringLiteral("Сохранить сцену"))->trigger(); }); until([&] { return !host.ReadSnapshot().dirty; });
        // Regression: an inactive window reports no focus widget, so CommitPendingEdits
        // posts the transform directly. With an unchanged transform the receiver must drop
        // it and create no Undo entry. Reload clears history so canUndo is a clean baseline.
        {
            EditorCommand load; load.type = EditorCommand::LoadScene;
            load.path = QString::fromStdWString(sceneFile.wstring());
            host.PostCommand(load);
            until([&] { return !host.ReadSnapshot().canUndo && !host.ReadSnapshot().objects.empty(); });
            Check(!host.ReadSnapshot().canUndo, "history cleared before inactive-window save test");
            Ui([](auto* w) {
                auto* tree = w->findChild<QTreeWidget*>();
                for (int i = 0; i < tree->topLevelItemCount(); ++i)
                    if (tree->topLevelItem(i)->text(1) == QStringLiteral("Primitive.Sphere")) tree->setCurrentItem(tree->topLevelItem(i));
            });
            until([&] { return host.ReadSnapshot().selected != 0; });
            for (int i = 0; i < 10; ++i) pump();
            auto selectedXNow = [&] { const auto view = host.ReadSnapshot(); for (const auto& o : view.objects) if (o.id == view.selected) return o.transform.position[0]; return -999.0f; };
            const float startX = selectedXNow();
            Ui([](auto* w) {
                auto* viewport = w->findChild<ViewportWidget*>(); viewport->setFocus();
                auto* panel = w->findChild<ObjectInspectorPanel*>();
                Check(panel && !panel->isAncestorOf(QApplication::focusWidget()), "focus moved outside the inspector");
                Action(w, QStringLiteral("Сохранить сцену"))->trigger();
            });
            for (int i = 0; i < 30; ++i) pump();
            Check(!host.ReadSnapshot().canUndo, "inactive-window save of unchanged transform adds no Undo entry");
            Check(selectedXNow() == startX, "inactive-window no-op save leaves transform unchanged");
        }
        Ui([](auto* w) { w->close(); });
        until([&] { return exit; });
        host.SetModuleApi({}); factory.destroy(api.context); sessionGuard.context = nullptr; host.Stop();
        std::printf("EditorUiTests: %d checks passed\n", checks);
        return 0;
    }
    catch (const std::exception& e) { std::fprintf(stderr, "EditorUiTests: %s\n", e.what()); return 1; }
}
