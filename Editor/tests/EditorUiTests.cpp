#define NOMINMAX
#include <Windows.h>
#include "QtHost.h"
#include "MainWindow.h"
#include "MaterialEditorWindow.h"
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
#include <QStyle>
#include <QLineEdit>
#include <QToolButton>
#include <QGroupBox>
#include <QTreeView>
#include "ViewportWidget.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <stdexcept>

namespace
{
int checks = 0;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); ++checks; }
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
    for (auto* action : window->findChildren<QAction*>()) if (action->text() == label) return action;
    throw std::runtime_error("Action not found: " + label.toStdString());
}
void Add(EditorMainWindow* window, int index)
{
    auto* combo = window->findChild<QComboBox*>(QStringLiteral("PrimitiveType"));
    auto* category=window->findChild<QComboBox*>(QStringLiteral("ObjectCategory"));
    Check(category!=nullptr,"category picker"); category->setCurrentIndex(category->findData(QStringLiteral("all")));
    Check(combo && combo->count() == 13, "categorized object palette"); combo->setCurrentIndex(index);
    auto* button = window->findChild<QPushButton*>(QStringLiteral("AddPrimitive"));
    Check(button && button->isEnabled(), "add button enabled"); button->click();
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
    callbacks.getService=[](void* p,uint32_t service,uint32_t version)->const void* {return service==EngineEditorReloadService&&version<=1?&static_cast<Context*>(p)->service:nullptr;};
    EditorQtHost host(callbacks);host.SetModuleApi(module.Api());host.SetProjectDataApi(module.ProjectDataApi());host.SetSelectionApi(module.SelectionApi());host.SetViewportInputApi(module.ViewportInputApi());
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
        Ui([&](auto* w){Check(w==windowBefore,"real build preserves editor window");auto* category=w->findChild<QComboBox*>("ObjectCategory");category->setCurrentIndex(category->findData("project"));auto* types=w->findChild<QComboBox*>("PrimitiveType");Check(types->findData("Spawner")>=0,"new Spawner visible after real Hot Reload");});
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
        Check(argc == 3 || argc == 4, "usage: EditorUiTests <project directory> <EngineContent> [model]");
        const auto root = std::filesystem::absolute(argv[1]);
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
            object.projectData=R"({"version":1,"speed":5,"enabled":true,"mode":"walk","target":123,"asset":"/Game/pixel.bmp"})";
            object.propertySchema=R"({"version":1,"fields":[{"path":"/speed","type":"number","group":"Behavior/Movement","min":0,"max":20},{"path":"/enabled","type":"bool","group":"Behavior/Movement"},{"path":"/mode","type":"enum","options":[{"label":"Walk","value":"walk"}]},{"path":"/target","type":"object"},{"path":"/asset","type":"asset"}]})";
            view.objects.push_back(object);panel.Refresh(view,123,true);
            auto* number=panel.findChild<QDoubleSpinBox*>("Property/speed");auto* check=panel.findChild<QCheckBox*>("Property/enabled");
            auto* mode=panel.findChild<QComboBox*>("Property/mode");auto* target=panel.findChild<QComboBox*>("Property/target");
            Check(number&&number->value()==5&&number->maximum()==20,"typed numeric value and bounds");
            Check(check&&check->isChecked(),"typed boolean");Check(mode&&mode->currentText()=="Walk","typed enum");Check(target&&target->currentText()=="Property probe","typed object reference");
            Check(panel.findChild<QLineEdit*>("Property/asset")->text()=="/Game/pixel.bmp","typed asset path");
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
            int height=0;QPoint boundary;
            Ui([&](auto* w){
                QDockWidget* files=nullptr;for(auto* d:w->template findChildren<QDockWidget*>())if(d->windowTitle()==QStringLiteral("Ресурсы и код"))files=d;
                Check(files!=nullptr,"content dock exists");files->raise();w->resize(1600,950);
                std::printf("Layout before: content=%d min=%d max=%d features=%d area=%d\n",files->height(),files->minimumHeight(),files->maximumHeight(),int(files->features()),int(w->dockWidgetArea(files)));
                Action(w,QStringLiteral("Ресурсы: компактная панель"))->trigger();
            });for(int i=0;i<15;++i)pump();
            Ui([](auto* w){w->resize(1800,1100);});for(int i=0;i<15;++i)pump();
            Ui([&](auto* w){QDockWidget* files=nullptr;for(auto* d:w->template findChildren<QDockWidget*>())if(d->windowTitle()==QStringLiteral("Ресурсы и код"))files=d;
                height=files->height();std::printf("Layout resized: content=%d\n",height);Check(height<=240,"compact panel does not consume most of the window");
                boundary=QPoint(w->width()/2,files->mapTo(w,QPoint()).y()-w->style()->pixelMetric(QStyle::PM_DockWidgetSeparatorExtent)/2);
                QMouseEvent press(QEvent::MouseButtonPress,boundary,w->mapToGlobal(boundary),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(w,&press);
            });for(int i=0;i<3;++i)pump();
            Ui([&](auto* w){const auto end=boundary+QPoint(0,-90);QMouseEvent move(QEvent::MouseMove,end,w->mapToGlobal(end),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(w,&move);});for(int i=0;i<10;++i)pump();
            Ui([&](auto* w){const auto end=boundary+QPoint(0,-90);QMouseEvent release(QEvent::MouseButtonRelease,end,w->mapToGlobal(end),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(w,&release);});for(int i=0;i<10;++i)pump();
            Ui([&](auto* w){QDockWidget* files=nullptr;for(auto* d:w->template findChildren<QDockWidget*>())if(d->windowTitle()==QStringLiteral("Ресурсы и код"))files=d;std::printf("Layout dragged: content=%d (before %d)\n",files->height(),height);Check(files->height()>height+40,"content separator drag resizes panel");
                Action(w,QStringLiteral("Ресурсы: отдельное окно"))->trigger();Check(files->isFloating(),"content floats");w->addDockWidget(Qt::LeftDockWidgetArea,files);files->setFloating(false);Check(w->dockWidgetArea(files)==Qt::LeftDockWidgetArea,"content can dock at side");
                const auto layout=w->saveState(1);Action(w,QStringLiteral("Сбросить расположение панелей"))->trigger();Check(!files->isFloating()&&w->dockWidgetArea(files)==Qt::BottomDockWidgetArea,"reset returns content below viewport");
                Check(w->restoreState(layout,1)&&w->dockWidgetArea(files)==Qt::LeftDockWidgetArea,"panel layout restores by stable names");
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
                  auto* category=w->findChild<QComboBox*>(QStringLiteral("ObjectCategory"));category->setCurrentIndex(category->findData(QStringLiteral("all")));
                auto* palette=w->findChild<QComboBox*>(QStringLiteral("PrimitiveType"));
                const auto index=palette->findData(QStringLiteral("RulesObject"));
                Check(index>=0,"independent hierarchy visible in palette");
                palette->setCurrentIndex(index);w->findChild<QPushButton*>(QStringLiteral("AddPrimitive"))->click();
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
                  auto* palette=w->findChild<QComboBox*>(QStringLiteral("PrimitiveType"));
                  const auto index=palette->findData(QStringLiteral("ProjectComponent"));
                  Check(index>=0,"custom component visible");
                  palette->setCurrentIndex(index);w->findChild<QPushButton*>(QStringLiteral("AddPrimitive"))->click();
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
                auto* combo = w->findChild<QComboBox*>(QStringLiteral("PrimitiveType"));
                Check(combo && combo->count() == 6,"legacy palette excludes unavailable lights");
                combo->setCurrentIndex(0); w->findChild<QPushButton*>(QStringLiteral("AddPrimitive"))->click();
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
            auto* category=w->findChild<QComboBox*>(QStringLiteral("ObjectCategory"));
            auto* types=w->findChild<QComboBox*>(QStringLiteral("PrimitiveType"));
            Check(types->count()==6,"default shows only primitives");
            category->setCurrentIndex(category->findData(QStringLiteral("Particle.")));
            Check(types->count()==2 && types->findData(QStringLiteral("Particle.Sparks"))>=0 && types->findData(QStringLiteral("Particle.Smoke"))>=0,"two particle presets in category");
            category->setCurrentIndex(category->findData(QStringLiteral("Environment.")));
            Check(types->count()==1 && types->currentData().toString()=="Environment.Sky","sky category");
            category->setCurrentIndex(category->findData(QStringLiteral("project")));
            Check(types->findData("Gameplay.PlaySettings")<0,"service settings not placeable");
            Check(types->count()==0&&!w->findChild<QPushButton*>(QStringLiteral("AddPrimitive"))->isEnabled(),"empty category cannot spawn");
            Check(w->findChild<QTreeWidget*>()->topLevelItemCount() == 4, "starter hierarchy"); Add(w, 0);
        });
        until([&] { return host.ReadSnapshot().objects.size() == 5; });
        Ui([](auto* w) { Add(w, 1); });
        until([&] { return host.ReadSnapshot().objects.size() == 6; });
        const auto imagePath = QString::fromStdWString((root / L"outside.bmp").wstring());
        Ui([&](auto* w) {
            Check(w->findChild<QToolButton*>("ImportAssets")->menu() != nullptr, "imports grouped into menu");
            Check(w->findChild<QPushButton*>("ChooseModel") != nullptr, "model picker exists");
            Check(w->findChild<QLineEdit*>("ModelAsset")->isReadOnly() && w->findChild<QLineEdit*>("TextureAsset")->isReadOnly(), "asset paths locked");
            for (const auto& title : {QStringLiteral("Общие"),QStringLiteral("Позиция"),QStringLiteral("Поворот"),QStringLiteral("Масштаб"),QStringLiteral("Визуал"),QStringLiteral("Добавление объекта")}) {
                bool found=false; for (auto* group : w->template findChildren<QGroupBox*>()) found |= group->title()==title;
                for(auto* header:w->template findChildren<QToolButton*>())found|=header->objectName().startsWith("Section.")&&header->text()==title;
                Check(found,"inspector and placement categories");
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
        Check(errorLine == 11, "compile error points at the surface line");
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
            until([&] { for(const auto& o:host.ReadSnapshot().objects) if(o.type=="Engine.Model") return true; return false; });
            Check(host.ReadSnapshot().objects.size()==6 && host.ReadSnapshot().selected!=0,"replace primitive model preserves count and selection");
            Ui([](auto* w) { Action(w,QStringLiteral("Отменить"))->trigger(); });
            until([&] { for(const auto& o:host.ReadSnapshot().objects) if(o.type=="Primitive.Sphere") return true; return false; });
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
        EditorCommand drag; drag.type = EditorCommand::BeginGizmoDrag; drag.objectId = sphere.id;
        host.PostCommand(drag);
        EditorCommand transform; transform.type = EditorCommand::UpdateObjectTransform;
        transform.objectId = sphere.id; transform.path = QStringLiteral("gizmo"); transform.transform = sphere.transform;
        transform.transform.scale[0] = 2; host.PostCommand(transform);
        transform.transform.scale[0] = 3; host.PostCommand(transform);
        drag.type = EditorCommand::EndGizmoDrag; host.PostCommand(drag);
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (o.id == sphere.id) return o.transform.scale[0] == 3; return false; });
        Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (o.name == sphere.name) return o.transform.scale[0] == sphere.transform.scale[0]; return false; });
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
        Ui([](auto* w) { Check(!w->findChild<QPushButton*>(QStringLiteral("AddPrimitive"))->isEnabled(), "editing disabled during Play"); Action(w, QStringLiteral("⏹ Stop"))->trigger(); });
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
        Ui([](auto* w) {
            auto* speed=w->findChild<QDoubleSpinBox*>("CharacterSpeed"); Check(speed && speed->isVisible(),"character settings in inspector");
            speed->setFocus(); speed->setValue(7); QMetaObject::invokeMethod(speed,"editingFinished",Qt::DirectConnection);
        });
        until([&] { for(const auto& o:host.ReadSnapshot().objects) if(o.gameplayKind==1) return QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object()["moveSpeed"].toDouble()==7; return false; });
        Ui([](auto* w) {
            auto* speed=w->findChild<QDoubleSpinBox*>("CharacterSpeed"); Check(speed->hasFocus() || speed->isAncestorOf(QApplication::focusWidget()),"refresh does not steal property focus");
            QTimer::singleShot(50,w,[w]{
                auto* combo=w->template findChild<QComboBox*>("PlayCharacter");Check(combo&&combo->count()==2,"Play dialog recognizes Character");combo->setCurrentIndex(1);
                auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());Check(dialog,"Play settings dialog");dialog->accept();
            });
            Action(w,QStringLiteral("Настроить Play…"))->trigger();
        });
        until([&] { for(const auto& o:host.ReadSnapshot().objects)if(o.gameplayKind==3)return true;return false; });
        Ui([](auto* w) {
            auto hasSettings=[&]{for(int i=0;i<w->findChild<QTreeWidget*>()->topLevelItemCount();++i)if(w->findChild<QTreeWidget*>()->topLevelItem(i)->text(1)=="Gameplay.PlaySettings")return true;return false;};
            Check(!hasSettings(),"Play settings hidden in normal hierarchy");
            Action(w,QStringLiteral("Служебные объекты сцены"))->setChecked(true);Check(hasSettings(),"old service records remain accessible for repair");
            Action(w,QStringLiteral("Служебные объекты сцены"))->setChecked(false);Check(!hasSettings(),"service records can be hidden again");
        });
        Ui([](auto* w) { Action(w,QStringLiteral("Сохранить сцену"))->trigger(); });
        until([&] { return !host.ReadSnapshot().dirty; });
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
        auto sunObject = [&] {
            for (const auto& object : host.ReadSnapshot().objects) if(object.type=="Light.Directional") return object;
            return EditorQtHost::SceneObject{};
        };
        auto sunSettings = [&] { return QJsonDocument::fromJson(QByteArray::fromStdString(sunObject().projectData)).object(); };
        Ui([](auto* w) { Action(w,QStringLiteral("Направленный свет"))->trigger(); });
        until([&] { return sunObject().id && host.ReadSnapshot().selected==sunObject().id; });
        const auto withSun = host.ReadSnapshot().objects.size();
        Ui([](auto* w) {
            auto* intensity=w->template findChild<QDoubleSpinBox*>("Property/intensity");
            auto* color=w->template findChild<QPushButton*>("Property/color");
            Check(intensity && color && w->template findChild<QCheckBox*>("Property/shadows"),"directional light inspector controls");
            intensity->setValue(2.5);QMetaObject::invokeMethod(intensity,"editingFinished",Qt::DirectConnection);
        });
        until([&] { return sunSettings()["intensity"].toDouble()==2.5; });
        Ui([](auto* w) { Action(w,QStringLiteral("Отменить"))->trigger(); });
        until([&] { return sunSettings()["intensity"].toDouble()==1; });
        Ui([](auto* w) { Action(w,QStringLiteral("Повторить"))->trigger(); });
        until([&] { return sunSettings()["intensity"].toDouble()==2.5; });
        Ui([](auto* w) { Action(w,QStringLiteral("Направленный свет"))->trigger(); });
        until([&] { return host.ReadSnapshot().selected==sunObject().id; });
        Check(host.ReadSnapshot().objects.size()==withSun,"global sun action selects existing singleton");
        Ui([](auto* w) {
            auto* yaw=w->template findChild<QDoubleSpinBox*>("Transform4");yaw->setValue(65);
            QMetaObject::invokeMethod(yaw,"editingFinished",Qt::DirectConnection);
            w->template findChild<QCheckBox*>("Property/shadows")->click();
        });
        until([&] { return !sunSettings()["shadows"].toBool(); });
        const auto sunRotation = sunObject().transform;
        Check(std::abs(sunRotation.rotation[1])>0.1f,"inspector rotates directional light");
        Ui([](auto* w) {
            QTimer::singleShot(80,w,[]{
                auto* dialog=qobject_cast<QColorDialog*>(QApplication::activeModalWidget());Check(dialog,"sun color dialog");
                dialog->setCurrentColor(QColor::fromRgbF(0.2,0.6,1));dialog->accept();
            });
            w->template findChild<QPushButton*>("Property/color")->click();
        });
        until([&] { return std::abs(sunSettings()["color"].toArray()[0].toDouble()-0.2)<0.01; });
        Ui([](auto* w) { Action(w,QStringLiteral("Сохранить сцену"))->trigger(); });
        until([&] { return !host.ReadSnapshot().dirty; });
        Ui([](auto* w) { Action(w,QStringLiteral("▶ Play"))->trigger(); });
        until([&] { return host.PlayState()==EditorQtHost::Play; });
        Check(sunSettings()["intensity"].toDouble()==2.5,"Play keeps authored sun");
        Ui([](auto* w) { Action(w,QStringLiteral("⏹ Stop"))->trigger(); });
        until([&] { return host.PlayState()==EditorQtHost::Edit; });
        Ui([](auto* w) { Action(w,QStringLiteral("Направленный свет"))->trigger(); });
        until([&] { return host.ReadSnapshot().selected==sunObject().id; });
        Ui([&](auto* w) { w->grab().save(QString::fromStdWString((root / "directional-light-inspector.png").wstring())); Action(w,QStringLiteral("Удалить объект"))->trigger(); });
        until([&] { return !sunObject().id; });
        Ui([](auto* w) { Action(w,QStringLiteral("Отменить"))->trigger(); });
        until([&] { return sunObject().id && sunSettings()["intensity"].toDouble()==2.5; });
        Ui([](auto* w) { Action(w,QStringLiteral("Сохранить сцену"))->trigger(); });
        until([&] { return !host.ReadSnapshot().dirty; });
        Ui([](auto* w) { w->close(); });
        until([&] { return exit; });
        host.SetModuleApi({}); factory.destroy(api.context); sessionGuard.context = nullptr; host.Stop();
        std::printf("EditorUiTests: %d checks passed\n", checks);
        return 0;
    }
    catch (const std::exception& e) { std::fprintf(stderr, "EditorUiTests: %s\n", e.what()); return 1; }
}
