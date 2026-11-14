"""Create both editable slide sources from the preserved r7 tutorial."""
from pathlib import Path
import json, shutil, subprocess
root=Path(__file__).resolve().parent
baseline=root/'releases/PainEngine-Tutorial-2026-11-01-r7.content.json'
data=json.loads(baseline.read_text(encoding='utf-8'))
data.update(date='2026-11-07',revision=8,engineVersion='2026.11.07.6',sourceCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip())
data['slides'][0]['subtitle']='Редактор, материалы, звук\nи рабочие примеры C++'
data['slides'][0]['image']='r8/editor-assets.png'
data['slides'][0]['notes']='Практический маршрут прежнего туториала сохранён. Новые разделы дополняют его после основного курса. Полные исходники находятся в tutorial/examples. Снимки интерфейса сняты с текущего редактора; Qt grab не захватывает D3D viewport.'
shots={3:'editor-assets.png',4:'editor-assets.png',6:'editor-assets.png',8:'editor-assets.png',9:'editor-assets.png',11:'editor-material.png',12:'directional-light-inspector.png',13:'editor-light.png',14:'player-properties.png',16:'editor-assets.png',19:'hud-settings.png',20:'ui-editor.png'}
for n,s in enumerate(data['slides'],1):
 if s['type']=='shot':
  if n in shots:
   s['image']='r8/'+shots[n]
  else:
   s['type']='rows';s.pop('image',None)
  s['notes']=s.get('notes','')+'\nИнструкция сверена с выпуском 2026.11.07.6. Снимки r8 показывают текущий Qt-интерфейс; отдельный D3D viewport может быть чёрным в Qt grab.'
 if 'foot' in s:
  s['foot']=s['foot'].replace('O:/Engine/Проекты/Туториал-Свойства','tutorial/examples/PropertyTutorial')
data['slides'][4]['items']=[['Размер панели','Тяните разделитель между панелями.'],['Перемещение','Перетаскивайте заголовок панели в новое место.'],['Сброс','Вид → Сбросить раскладку окон.']]
data['slides'][5]['items']=[['Схема управления','Правка → Схема управления: Unreal / Unity или Blender.'],['Выделение','Выберите объект в дереве или viewport. F фокусирует его.'],['Трансформация','Используйте gizmo и числовые поля инспектора.']]
data['slides'][8]['items'][1]=['Материалы частей','Каждый source material ID получает .mat. Общий override заменяет все слоты.']
data['slides'][10]['items'][1]=['Режим материала','HLSL редактируется вручную или создаётся узловым графом.']
data['slides'][11]['items'][0]=['Добавление','Выберите направленный свет в каталоге объектов сцены.']
data['slides'][26]['items'][0]=['Учебные исходники','Скопируйте PropertyTutorial в новый C++ проект по README.']
data['slides'][26]['foot']='Исходники и сцена: tutorial/examples/PropertyTutorial. У каждого куба свои значения.'
data['slides'][43]['foot']='Battle включён в дополнительные встроенные примеры. Правки ресурсов делайте в копии проекта.'

def rows(title,items,sources,notes='',foot=''):
 return dict(type='rows',title=title,items=items,sources=sources,notes=notes,foot=foot)
def shot(title,image,items,sources,notes='',foot=''):
 return dict(type='shot',title=title,image='r8/'+image,items=items,sources=sources,notes=notes,foot=foot)
new=[
 rows('Редактор: схемы и раскладка',[
 ['Управление','Две схемы: Unreal / Unity и Blender. Выбор находится в меню «Правка».'],
 ['Окна','Перенос и сохранение панелей, темы, язык и сброс раскладки через «Вид».'],
 ['Viewport','Gizmo, сетка и FXAA получили исправления выбора, глубины и очистки кадра.']],
 ['Editor/Source/MainWindow.cpp','Editor/Core/GizmoMath.cpp','Editor/Core/ViewportOverlay.cpp'],
 'Изменения после r7: редактор v2, локализация, единые иконки, исправления gizmo и FXAA. Для проверки измените transform, отмените действие и перезапустите редактор.'),
 shot('Материалы: узловой граф','editor-material-graph.png',[
 ['Ноды и связи','Tab открывает поиск. Соедините выход ноды со входом Surface.'],
 ['Результат','Граф создаёт HLSL. Сохраните .mat и .mat.graph.json вместе.']],
 ['Editor/Source/MaterialGraphWidget.cpp','Editor/Core/HlslGraphCompiler.cpp'],
 'Ранее материал настраивался через параметры и ручной HLSL. Теперь граф генерирует HLSL, поддерживает текстуры и вычисления. Полный маршрут: materials.html.'),
 shot('Граф: поиск, отмена и ошибки','editor-material-graph-error.png',[
 ['Правки','Ctrl+Z / Ctrl+Y, копирование, вставка и дублирование нод.'],
 ['Диагностика','Проверьте ошибку графа. Откат восстанавливает согласованные файлы.']],
 ['Editor/Source/MaterialEditorWindow.cpp','Editor/Core/MaterialGraphModel.cpp'],
 'Исправлены согласованность rollback и сохранение пресетов проекта. Поиск принимает русские названия и английские типы. F показывает граф целиком.'),
 rows('Материалы импортированных моделей',[
 ['Раньше','Общий материал мог скрывать различия между частями модели.'],
 ['Теперь','Каждый source material ID имеет отдельный .mat. Слоты сохраняются в сцене и SaveGame.'],
 ['Практика','Меняйте слот в свойствах модели. Снимите общий override, чтобы увидеть отдельные назначения.']],
 ['Engine/Source/Scene/ModelImport.cpp','Engine/Include/Engine/Components/SpecificComponents/SkeletalMeshComponent.h'],
 'FBX, BLEND, GLB и glTF переносят базовый цвет и изображения. ID слотов могут иметь пропуски. Это не полный перенос PBR и Blender node graph.'),
 rows('Звук в редакторе',[
 ['Импорт','Сцена → Ресурсы → Импорт аудио… Файл попадёт в Content/Audio.'],
 ['Прослушивание','ПКМ по файлу → Прослушать. Для остановки выберите «Остановить звук».'],
 ['Форматы','Для первого опыта PCM WAV. MP3, FLAC и OGG зависят от декодеров Windows.']],
 ['Editor/Source/FileBrowser.cpp','Editor/Source/AssetTools.cpp','Engine/Source/Runtime/SceneEditAudio.cpp'],
 'Предпрослушивание доступно в Edit. Импорт не добавляет звуковой компонент автоматически. В учебных ресурсах приложен tutorial.wav.'),
 dict(type='code',title='Звук из игрового C++',code='auto* audio = GetGame()->GetAudioEngine();\nif (!audio) return;\nauto path = EngineModuleVfs::Resolve(\n    "/Game/Audio/tutorial.wav");\nauto id = audio->LoadSound(\n    std::filesystem::u8path(path).wstring());\nif (!id.IsValid()) return;\naudio::AudioEngine::PlayOptions options;\noptions.volume = 0.25f;\naudio->Play2D(id, options);',aside='Фрагмент BeginPlay.\n\nПолный класс и includes:\nAudioTutorial.h/.cpp\n\nGame управляет временем жизни AudioEngine.',sources=['Engine/docs/tutorial/examples/AudioTutorial.h','Engine/docs/tutorial/examples/AudioTutorial.cpp','Engine/Include/Engine/Sound/AudioEngine.h'],notes='Добавьте Audio tutorial в сцену после сборки. BeginPlay воспроизводит тон один раз. SoundId принадлежит создавшему его AudioEngine. Полный компонент проверяет ошибки.'),
 rows('3D-звук и потоковая музыка',[
 ['Короткий эффект','LoadSound и Play3D используют SoundId и копию AudioEmitter3D.'],
 ['Музыка','PlayStream читает файл потоково. Сохраните StreamHandle для StopStream.'],
 ['Ограничение','Для активного 3D-источника пока нет публичного handle перемещения.']],
 ['Engine/Include/Engine/Sound/AudioEngine.h','Engine/Include/Engine/Sound/Audio3D.h','Engine/Source/Sound/AudioEngine.cpp'],
 'Game обновляет listener по камере. PlayOptions задаёт volume, pitchSemitones и loop. StopAll останавливает всё в текущем AudioEngine. Физический wide path получают через VFS и filesystem.'),
 rows('Игровые подсистемы',[
 ['Один экземпляр','ENGINE_SUBSYSTEM создаёт GameSubsystem на игровую сессию.'],
 ['Смена сцены','Экземпляр сохраняется. OnSceneReady и OnSceneClosing обновляют ссылки.'],
 ['Stop и Reload','Stop уничтожает подсистемы. Hot Reload не переносит их состояние.']],
 ['Engine/Include/Engine/MainGame/GameSubsystem.h','Engine/docs/GAME-SUBSYSTEMS.md'],
 'По умолчанию bTickEnabled=false. GetSubsystem возвращает заимствованный указатель. Edit и material preview не создают игровые подсистемы. Пример SessionCounterSubsystem входит в SDK.'),
 rows('SaveGame: прогресс между запусками',[
 ['Запрос','Game::Saves().RequestSave / RequestLoad ставят операцию в очередь.'],
 ['Результат','Проверяйте LastResult: Saved, Loaded или Failed. Не считайте Accepted завершением.'],
 ['Свои данные','SerializeSaveGame вызывает базу, затем Section, Version и Field.']],
 ['Engine/Include/Engine/SaveGame/SaveGame.h','Engine/docs/SAVEGAME.md'],
 'Сохраняются сцена, свойства и поддержанное runtime-состояние. Configure задаёт стабильный gameId. После Load прежние указатели недействительны. Сохранение редакторской сцены и SaveGame решают разные задачи.'),
 rows('Код и диагностика',[
 ['CLI','--help и понятные ошибки. Создание C++: create-project, затем enable-code.'],
 ['Hot Reload','Ошибки кандидата видны пользователю. Рабочая DLL остаётся активной.'],
 ['Рендер','F7/F8 и командлеты помогают проверить LOD, draw calls, ввод и материалы.']],
 ['Game/Launcher/Source/Main.cpp','Editor/Source/CodeTools.cpp','Engine/docs/RENDER-DIAGNOSTICS.md'],
 'Не смешивайте runtime, Engine.lib и заголовки разных выпусков. После изменения C++ API пересоберите проект. Командлеты описаны в COMMANDLETS.md.'),
 rows('Исправления теней и нормалей',[
 ['Каскады','Исправлены выбор и устойчивость каскадов при изменении камеры.'],
 ['Точечный свет','Две теневые лампы, обновление движущихся источников и кастеров.'],
 ['Normal map','GPU получает TANGENT. Зеркальный масштаб корректирует знак базиса.']],
 ['Engine/docs/ENGINE-VERSIONS.md','Engine/docs/MATERIALS.md','Agent/Tasks/068-point-shadow-slots-lod.md'],
 'MeshVertex изменился до 64 байт: требуется пересборка C++ модулей. Для skeletal normal map остаётся путь через производные UV. Тени не означают наличие физики или коллизий.'),
 rows('Прозрачность и границы LOD',[
 ['Порядок слоёв','Единая очередь объектов и экземпляров. Исправлено двойное смешивание LOD.'],
 ['Пересечения','Версия 2026.11.07.6 обрабатывает 16 ближайших глубин по каждому пикселю.'],
 ['Границы','Дальний остаток и legacy/custom PS сохраняют ограничения сортировки.']],
 ['Engine/docs/MATERIALS.md','Engine/docs/ENGINE-VERSIONS.md','Agent/Tasks/071-transparent-intersections.md'],
 'Translucent и Additive используют последний mesh LOD вместо несовместимого impostor. Обработка пересечений имеет стоимость; не считать её бесконечным точным порядком всех поверхностей.'),
 rows('Четыре проекта для изучения',[
 ['Город','CalmCity: движение и SaveGame. CuriousCity: размещение домов и экономика.'],
 ['Игровое действие','TiredBomber: сброс бомбы и попадание. AITrash: броски предметов и счёт.'],
 ['Исходники','Это проекты агентского прогона r5. В комплекте есть Source, Content и README.']],
 ['Agent/Playtests/2026-11-07-2026.11.06.1-r5/report.md'],
 'Проекты восстанавливаются из сохранённых архивов ролей, затем пересобираются текущим SDK. Не являются обещанием законченных коммерческих игр.'),
 rows('Учебный комплект',[
 ['Документация','index.html и шесть руководств. API Reference содержит объявления и поиск.'],
 ['Код и ресурсы','Вращение, ввод, модели, материалы, HUD, свойства и AudioTutorial.'],
 ['Первый запуск','README в корне архива объясняет запуск, сборку и четыре примера.']],
 ['Engine/docs/build-guides.mjs','Engine/docs/tutorial/examples/README.txt'],
 'Документация читается офлайн. Для BLEND нужен отдельный Blender. Для изменения C++ нужны Visual Studio C++ tools и Windows SDK.')
]
data['slides']+=new
data=json.loads(json.dumps(data,ensure_ascii=False).replace('Ресурсы и код','Контент-браузер').replace('Console','Консоль').replace('Меню на снимке — ручной способ:', 'Ручной способ:'))
(root/'content.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
changes={k:v for k,v in data.items() if k!='slides'}
changes.update(title='PainEngine\nИзменения выпуска',outputPrefix='PainEngine-Changes',revision=1)
cover=dict(data['slides'][0]);cover.update(title=changes['title'],subtitle='После туториала от 1 ноября\nРедакторы, код, звук и материалы')
changes['slides']=[cover]+new
(root/'changes.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2),encoding='utf8')
print('Tutorial:',len(data['slides']),'Changes:',len(changes['slides']))
