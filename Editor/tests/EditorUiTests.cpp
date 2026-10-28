#define NOMINMAX
#include <Windows.h>
#include "QtHost.h"
#include "MainWindow.h"
#include "CodeClassDialog.h"
#include <Engine/Runtime/LegacyEditApi.h>
#include <Engine/Runtime/EditExtensions.h>
#include <Engine/Runtime/ProjectDataApi.h>
#include <Engine/Runtime/GameEditModule.h>
#include <QDialog>
#include <QTimer>
#include <QApplication>
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
#include <QLineEdit>
#include <QToolButton>
#include <QGroupBox>
#include <QTreeView>
#include "ViewportWidget.h"
#include <chrono>
#include <filesystem>
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
    Check(combo && combo->count() == 12, "categorized object palette"); combo->setCurrentIndex(index);
    auto* button = window->findChild<QPushButton*>(QStringLiteral("AddPrimitive"));
    Check(button && button->isEnabled(), "add button enabled"); button->click();
}
}
void ReloadUiChecks(const std::filesystem::path& root, const std::filesystem::path& engineContent)
{
    const auto manifest=(root/"Project.json").u8string(), content=(root/"Content").u8string();
    auto ownedModule=std::make_unique<GameEditModule>(root/"Game.dll",engineContent,root/"Content");
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
        const auto manifest = (root / "Project.json").u8string(), content = (root / "Content").u8string();
        const auto engineContent = std::filesystem::absolute(argv[2]).u8string();
        const bool codeProject = argc == 4 && std::wstring(argv[3]) == L"--code";
        std::filesystem::path modulePath = root / "Game.dll";
        if (codeProject) {
            QFile project(QString::fromStdWString((root / "Project.json").wstring()));
            Check(project.open(QIODevice::ReadOnly),"read code project");
            modulePath = root / std::filesystem::u8path(QJsonDocument::fromJson(project.readAll()).object().value("module").toString().toUtf8().constData());
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
        EngineSelectionApi selection{};
        if (!legacy && query && query(api.context,EngineSelectionExtension,1,sizeof(selection),&selection)) host.SetSelectionApi(selection);
        EngineViewportInputApi viewportInput{};
        if (!legacy && query && query(api.context,EngineViewportInputExtension,1,sizeof(viewportInput),&viewportInput)) host.SetViewportInputApi(viewportInput);
        struct HostCleanup
        {
            EditorQtHost& host; EngineEditModuleApi factory; void*& context;
            ~HostCleanup() { host.SetModuleApi({}); if (context) { factory.destroy(context); context = nullptr; } host.Stop(); }
        } hostCleanup{host, factory, sessionGuard.context};
        Check(host.Start(), "start Qt editor");
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
            Ui([](auto* w){Action(w,QStringLiteral("Сохранить и сделать стартовой"))->trigger();});
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
                auto* x = w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0"));
                x->setValue(3.5); QMetaObject::invokeMethod(x,"editingFinished",Qt::DirectConnection);
            });
            until([&] { return selectedX() == 3.5f; });
            Ui([](auto* w) { Action(w,QStringLiteral("Отменить"))->trigger(); });
            until([&] { return !host.ReadSnapshot().selected; });
            Ui([](auto* w) { Action(w,QStringLiteral("Повторить"))->trigger(); });
            until([&] { for (const auto& o : host.ReadSnapshot().objects) if (o.transform.position[0] == 3.5f) return true; return false; });
            Ui([](auto* w) { Action(w,QStringLiteral("Сохранить и сделать стартовой"))->trigger(); });
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
            Check(w->findChild<QToolButton*>()->menu() != nullptr, "imports grouped into menu");
            Check(w->findChild<QPushButton*>("ChooseModel") != nullptr, "model picker exists");
            Check(w->findChild<QLineEdit*>("ModelAsset")->isReadOnly() && w->findChild<QLineEdit*>("TextureAsset")->isReadOnly(), "asset paths locked");
            for (const auto& title : {QStringLiteral("Общие"),QStringLiteral("Позиция"),QStringLiteral("Поворот"),QStringLiteral("Масштаб"),QStringLiteral("Визуал"),QStringLiteral("Добавление объекта")}) {
                bool found=false; for (auto* group : w->template findChildren<QGroupBox*>()) found |= group->title()==title;
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
        if (argc == 4)
        {
            const auto modelSource = std::filesystem::absolute(argv[3]);
            Ui([&](auto* w) { w->ImportAsset(false, QString::fromStdWString(modelSource.wstring())); });
            const auto importedModel = root / "Content/Models" / modelSource.stem() / modelSource.filename();
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
        // Select the sphere through the actual tree before testing a focused-field save.
        Ui([](auto* w) {
            auto* tree = w->findChild<QTreeWidget*>();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->text(1) == QStringLiteral("Primitive.Sphere")) tree->setCurrentItem(tree->topLevelItem(i));
        });
        until([&] { return selectedX() == 3.25f; });
        Ui([](auto* w) { auto* x = w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0")); x->setFocus(); x->setValue(4.5); Action(w, QStringLiteral("Сохранить и сделать стартовой"))->trigger(); });
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
        Ui([](auto* w) { Action(w, QStringLiteral("Сохранить и сделать стартовой"))->trigger(); });
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
        Ui([](auto* w) { w->close(); });
        until([&] { bool modal = false; Ui([&](auto*) { modal = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) != nullptr; }); return modal; });
        Check(!exit, "unsaved close waits for a decision");
        Ui([](auto*) { auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); Check(box, "unsaved changes prompt"); box->done(QMessageBox::Cancel); });
        for (int i = 0; i < 30; ++i) pump();
        Check(!exit && host.ReadSnapshot().dirty, "cancel preserves editing session");
        Ui([](auto* w) { Action(w, QStringLiteral("Сохранить и сделать стартовой"))->trigger(); });
        until([&] { return !host.ReadSnapshot().dirty; });
        Ui([](auto* w) { Add(w,10); });
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
        Ui([](auto* w) { Action(w,QStringLiteral("Сохранить и сделать стартовой"))->trigger(); });
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
        Ui([](auto* w) { w->close(); });
        until([&] { return exit; });
        host.SetModuleApi({}); factory.destroy(api.context); sessionGuard.context = nullptr; host.Stop();
        std::printf("EditorUiTests: %d checks passed\n", checks);
        return 0;
    }
    catch (const std::exception& e) { std::fprintf(stderr, "EditorUiTests: %s\n", e.what()); return 1; }
}
