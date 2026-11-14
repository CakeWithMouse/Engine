// Offline manuals: node Engine/docs/build-guides.mjs [--check]
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const dir=path.dirname(fileURLToPath(import.meta.url));
const version=JSON.parse(fs.readFileSync(path.join(dir,'../Version.json'),'utf8')).version;
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const code=s=>`<pre><code>${esc(s)}</code></pre>`;
const sample=f=>`<h3>${esc(f)}</h3><a download href="tutorial/examples/${f}">Скачать исходник</a>${code(fs.readFileSync(path.join(dir,'tutorial/examples',f),'utf8'))}`;
const steps=xs=>'<ol>'+xs.map(s=>`<li>${s}</li>`).join('')+'</ol>';
const section=(title,body)=>`<section><h2>${title}</h2>${body}</section>`;
const shot=(file,caption)=>`<figure><img style="max-width:100%;height:auto" src="data:image/png;base64,${fs.readFileSync(path.join(dir,'tutorial/assets/r8',file)).toString('base64')}" alt="${esc(caption)}"><figcaption>${caption} Снимок текущего Qt-интерфейса: D3D viewport не входит в захват.</figcaption></figure>`;
const pages=[
['editor','Работа редактора','Панели, управление, редактирование и запуск.',
 section('Первый сеанс',steps([
 'Полностью распакуйте выпуск. Запустите EngineLauncher.exe, создайте проект в новой папке или откройте Project.json. Для готовых примеров C++ компилятор не нужен.',
 'Откройте редактор. В иерархии выберите объект, измените transform в инспекторе. Панели можно переносить и прикреплять, их расположение сохраняется.',
 'Меню «Вид» возвращает закрытые панели. «Вид → Сбросить раскладку окон» восстанавливает расположение. Там же доступны язык, тема и сглаживание FXAA.',
 'Выберите схему в «Правка → Схема управления»: Unreal / Unity или Blender. F фокусирует выбранный объект. Горячие клавиши зависят от схемы, поэтому ориентируйтесь на подсказки текущих инструментов.',
 'Ctrl+D дублирует объект, Ctrl+Z/Ctrl+Y отменяют и возвращают изменения. Delete в дереве удаляет выбранное. При вводе текста клавиши редактируют поле.',
 'Ctrl+S сохраняет сцену. «Запуск → Play» создаёт игровую копию, Pause приостанавливает её, Stop возвращает Edit. Изменения Play не записываются в исходную сцену.'
 ]))+
 section('Изменения редактора 0.3.0','<p>Панели сцены и инспектора можно прикрепить с любой стороны. При движении gizmo инспектор сохраняет раскрытие карточек и ввод. <a href="RELEASE-0.3.0.md">Изменения PR №24</a>.</p>')+
 section('Проверяемый результат','<p>Измените позицию куба, сохраните сцену, запустите и остановите Play. После Stop исходная позиция Edit сохраняется. Закройте и снова откройте сцену: сохранённое положение должно восстановиться.</p>')+
 section('Код, HUD и восстановление','<p>Команды C++ находятся в инструментах проекта. Ctrl+Alt+R собирает и проверяет DLL, затем восстанавливает edit-сцену. Остановите Play перед сборкой. При ошибке остаётся прежняя DLL, подробности доступны в Console и CodeBuild.log.</p><p>«Запуск → HUD игры…» выбирает .ui проекта. Двойной щелчок по .ui открывает визуальный редактор. «Сцена → Восстановить несохранённую сцену…» открывает восстановление. «Сцена → Экспорт игры…» создаёт отдельную поставку игры.</p>'),
 ['Editor/Source/MainWindow.cpp','Editor/Source/CodeTools.cpp','Editor/Source/UIEditorWindow.cpp']],
['materials','Материалы и редактор материалов','От назначения .mat до узлового графа и прозрачности.',
 section('Первый материал',steps([
 'В Edit выберите куб. В инспекторе откройте «Материал» и создайте новый материал. Он попадёт в Content/Materials. Также доступны команды «Новый материал…» и «Редактировать материал» в файловой панели.',
 'Измените цвет и параметры в окне материала. Для текстуры выберите BaseMap и изображение из Content/Textures. Назначение должно появиться на превью и объектах, использующих этот .mat.',
 'Нажмите «Сохранить» в окне материала: сохраняются .mat и HLSL. Ctrl+S в основном окне сохраняет сцену со ссылкой на материал, это отдельная операция.',
 'Закройте и повторно откройте сцену. Проверьте материал на объекте. Пурпурно-чёрная шахматка означает ошибку ресурса или компиляции, причину ищите в окне материала и журнале.'
 ]))+
 section('Библиотека проекта','<p>Выберите существующий .mat через поиск в библиотеке. В слотах материала откройте окно текстур с миниатюрами; при необходимости импортируйте изображение. Кнопки отдельной текстуры в карточке геометрии удалены.</p>')+
 section('Узловой граф',steps([
 'Нажмите «Нодовый граф». Рядом с .mat появится .mat.graph.json, HLSL будет генерироваться из графа.',
 'Tab, пробел, ПКМ по пустому месту или «+ Нода» открывают поиск. Найдите Color, соедините его выход с цветом Surface. Двойной щелчок по ноде открывает параметры.',
 'Тяните связь от выхода ко входу. У входа одна связь, циклы запрещены. F показывает весь граф или выделение. Колесо меняет масштаб, средняя кнопка или ПКМ перемещают холст.',
 'Ctrl+Z/Ctrl+Y меняют историю графа. Копирование и вставка переносят выделенные ноды. Сохраните материал и граф; проверьте результат после повторного открытия.'
 ]))+
 section('Текстуры, нормали и слоты','<p>BaseMap задаёт цвет. NormalFromMap использует tangent basis MeshVertex, включая знак зеркального масштаба; skeletal-формат использует производные UV. Для multi-material модели меняйте отдельный source material ID в свойствах модели. Общий материал объекта перекрывает все слоты. Номера слотов могут идти с пропусками.</p><p>Opaque рисует непрозрачную поверхность, Masked отбрасывает пиксели по AlphaClip, Translucent смешивает цвет, Additive добавляет свечение. В версии '+version+' первые 16 глубин прозрачных пересечений обрабатываются по пикселям. Дальний остаток и legacy/custom PS имеют ограничения сортировки. Это не неограниченная точная прозрачность.</p><p>Изменение общего Material влияет на всех владельцев. SetRenderState требует Compile. SetParameter возвращает false для неизвестного параметра. После правок файлов F6 обновляет ресурсы; при ошибке смотрите журнал.</p>')+
 section('Назначение из C++',sample('ModelExample.h')+sample('ModelExample.cpp')+'<p>Скопируйте оба файла в Source C++ проекта. Вызовите AssignMaterial для существующего компонента с путём созданного .mat. Сборка: Build.ps1 -Runtime &lt;папка выпуска&gt;/Runtime. Для SpawnModel сначала импортируйте модель, используйте уникальное имя.</p>'),
 ['Engine/Include/Engine/Render/Material.h','Engine/Source/Scene/SceneAssets.cpp','Editor/Source/MaterialEditorWindow.cpp','Editor/Source/MaterialGraphWidget.cpp','Engine/docs/MATERIALS.md']],
['audio','Звук и работа с ним','Импорт, предпрослушивание и игровой код.',
 section('Импорт и проверка',steps([
 'В Edit выберите «Сцена → Ресурсы → Импорт аудио…» или «Аудио…» в меню импорта файловой панели.',
 'Выберите WAV, OGG, MP3 или FLAC. Файл копируется в Content/Audio; совпадающее имя получает числовой суффикс.',
 'ПКМ по файлу в Content → «Прослушать». Следующее предпрослушивание останавливает предыдущее. «Остановить звук» завершает его вручную.',
 'Начните с PCM WAV. WAV декодируется встроенным декодером, остальные файлы передаются Windows Media Foundation. Наличие расширения в диалоге не гарантирует доступность декодера.',
 'Импорт не создаёт звуковой компонент сцены. Для игрового события добавьте вызов аудио API из своего компонента.'
 ]))+
 section('Готовый компонент', '<p>Создайте C++ проект. Скопируйте AudioTutorial.h/.cpp в Source и tutorial.wav из прилагаемых учебных ресурсов в Content/Audio. Выполните Build.ps1, откройте редактор, добавьте «Audio tutorial» из каталога классов, сохраните сцену и нажмите Play. Должен прозвучать короткий тон один раз при BeginPlay.</p>'+sample('AudioTutorial.h')+sample('AudioTutorial.cpp'))+
 section('2D, 3D и музыка','<p>Game уже владеет AudioEngine и вызывает Update. GetAudioEngine может вернуть nullptr при недоступном устройстве. SoundId принадлежит конкретному AudioEngine; после его пересоздания загрузите звук заново.</p><p>LoadSound принимает физический std::wstring путь. Преобразуйте /Game через EngineModuleVfs::Resolve, затем filesystem::u8path(...).wstring(). Для коротких звуков повторно используйте SoundId. Play2D принимает volume, pitchSemitones и loop. Play3D дополнительно принимает AudioEmitter3D, который копируется: публичного handle для движения уже играющего источника нет.</p><p>PlayStream читает музыку потоково и возвращает StreamHandle. StopStream останавливает этот поток. StopAll влияет на все звуки данного AudioEngine. SetListener задаёт слушателя, положение и ориентацию. Используйте mono WAV для первого 3D-примера.</p>')+
 section('Если звука нет','<p>Проверьте устройство вывода Windows, ненулевой GetAudioEngine, SoundId::IsValid и результат Play2D. Затем проверьте физический путь и PCM WAV. Проигрывание без ошибки не доказывает слышимость на выбранных колонках. После Stop начните новый Play для повторного BeginPlay.</p>'),
 ['Engine/Include/Engine/Sound/AudioEngine.h','Engine/Include/Engine/Sound/Audio3D.h','Engine/Source/Sound/DecoderFactory.cpp','Engine/Source/Runtime/SceneEditAudio.cpp','Editor/Source/FileBrowser.cpp']],
['code','Работа с кодом','Создание проекта, компоненты, сборка и отладка.',
 section('C++ проект',steps([
 'Для разработки установите Visual Studio C++ Build Tools и Windows SDK. Runtime и SDK должны относиться к одному выпуску. Для запуска готовых DLL компилятор не требуется.',
 'В лаунчере создайте проект с C++ либо добавьте C++ к существующему. CLI выполняет --create-project &lt;новая папка&gt; &lt;имя&gt;, затем --enable-code &lt;Project.json&gt;. Команды --create-code-project нет.',
 'Source содержит ProjectGame, Module и RotatingCube. Новые .cpp/.h в Source автоматически участвуют в сборке. ENGINE_COMPONENT регистрирует класс; сохраняйте стабильный ID при переименовании.',
 'Из папки проекта выполните ./Build.ps1 -Runtime &lt;папка выпуска&gt;/Runtime. В редакторе используйте Ctrl+Alt+R после Stop. Кандидат проходит проверку перед активацией.',
 'При ошибке прочитайте CodeBuild.log и validate/startup logs каталога кандидата. Game.sln удобен для редактирования и отладки, но активацию DLL выполняйте Build.ps1.'
 ]))+
 section('Вращающийся куб','<p>Ниже полный минимальный вариант файлов RotatingCube. В новом C++ шаблоне одноимённый класс уже существует: замените его учебную копию или изучите её, не создавайте второе определение. Добавьте куб из каталога проекта, нажмите Play. Поворот Y увеличивается на 90 градусов в секунду.</p>'+sample('RotatingCube.h')+sample('RotatingCube.cpp'))+
 section('Жизненный цикл и отладка','<p>Конструктор работает без Game и GPU. BeginPlay вызывается перед игровым вводом и Tick, Tick получает simulation deltaTime. Вызывайте непосредственную базу. Для спавна используйте Scene::AddComponent и проверяйте SceneResult. После удаления, загрузки SaveGame или hot reload прежние указатели могут быть недействительны.</p><p>Собирайте Debug с согласованным Debug SDK, подключайте Visual Studio debugger к EngineRuntime.exe и загружайте PDB своего модуля. Не смешивайте библиотеки Debug/Release. Для аппаратных проблем начните с логов и render diagnostics F7/F8.</p><p>Полные дополнительные примеры: InputCube для R/N, ModelExample для ресурсов, UI для HUD и PropertyTutorial для свойств. Они входят в комплект туториалов. В API Reference доступны полный код и объявления заголовков.</p>'),
 ['Engine/BuildSupport/ProjectTools.ps1','Editor/Source/CodeTools.cpp','Engine/Include/Engine/Components/GameComponents.h','Engine/docs/CODE-PROJECTS.md']],
['assets','Импорт ассетов','Модели, изображения, аудио и переносимые пути.',
 section('Модель и текстура',steps([
 'Остановите Play. Выберите «Сцена → Ресурсы → Импорт модели…». Поддерживаются FBX, OBJ, GLB, glTF и BLEND при наличии соответствующего импортёра.',
 'FBX/OBJ/glTF/GLB используют Assimp из выпуска. Для .blend нужен отдельно установленный Blender 4.2+; файл сначала преобразуется в GLB. Blender в этот комплект не включён.',
 'Сохраните все внешние buffers/images рядом с исходным glTF. Импорт создаёт уникальную папку Content/Models, копирует зависимости и формирует .import.json. Не переносите только один файл модели без её папки.',
 'Дважды щёлкните по импортированной модели в Content или используйте «Добавить модель…». Импорт файла и размещение объекта — отдельные действия.',
 'Импорт текстуры принимает PNG, JPG/JPEG, BMP, TGA. Для модели назначьте BaseMap материала или отдельную текстуру объекта осознанно: повторное назначение той же карты в обоих местах умножает цвет дважды.',
 'Сохраните сцену, закройте и откройте её. Модель, текстуры и слоты должны сохраниться. Пути сцены имеют вид /Game/Models/... и /Game/Textures/....'
 ]))+
 section('Материалы импорта','<p>Импорт переносит базовый цвет и цветовые текстуры материалов, создаёт Materials/Material_&lt;source ID&gt;.mat и Imported.hlsl. Это не полный перенос Blender node graph или любого PBR-шейдера. Slotted-модели сохраняют назначения в projectData.materialSlots. Общий override .mat перекрывает все части.</p>')+
 section('Выбор и организация ресурсов','<p>Модель и материал выбираются из Content в окне с поиском. Текстуры показывают миниатюры; модели и материалы — значки. Замена модели разрешена только для Engine.Model, включая старые сцены v1; для примитива добавьте новый модельный объект. Одна операция Undo возвращает модель. Неизвестный тип не преобразуется автоматически. Текстуры назначайте через BaseMap материала. В файловой панели ПКМ → Новая папка создаёт каталог и сразу включает ввод имени. Корни проекта и Content защищены.</p>')+
 section('Ресурсы для упражнений','<p>В tutorial/resources находятся <a href="tutorial/resources/pyramid.obj">OBJ-модель</a>, <a href="tutorial/resources/checker.png">PNG-текстура</a> и <a href="tutorial/resources/tutorial.wav">PCM WAV</a>. Импортируйте их штатными командами. Эти файлы созданы для обучения и включены в поставку. Модель должна появиться на сцене, PNG — назначиться на BaseMap, WAV — воспроизвестись через предпрослушивание.</p><p>При отсутствии модели проверьте журнал, поддерживаемый формат и внешние файлы. При отсутствии текстуры проверьте путь и .import.json. Повторный импорт создаёт новую папку, а не меняет все старые ссылки автоматически.</p>'),
 ['Engine/Source/Scene/ModelImport.cpp','Engine/Source/Scene/SceneAssets.cpp','Editor/Source/AssetTools.cpp','Engine/docs/PROJECT-ASSETS.md']],
['scene','Настройка сцены','Объекты, камера, освещение, сохранение и запуск.',
 section('Учебная сцена',steps([
 'Создайте сцену через «Сцена → Новая сцена». Добавьте куб и плоскость из каталога типов. Настройте transform в инспекторе и назначьте материал.',
 'Добавьте сценовую камеру и персонажа. В свойствах персонажа включите роль игрока и задайте камеру, которую должна использовать игра. Камера Edit и игровая камера различаются.',
 'Добавьте направленный или точечный свет. Для PointLight задайте цвет, интенсивность, радиус и castShadows. Две первые подходящие лампы получают динамические тени, остальные продолжают освещать без них.',
 'Сохраните сцену в Content/Scenes. Отдельно выберите «Сцена → Сделать стартовой сценой». Обычное сохранение не меняет startupScene проекта.',
 'Нажмите Play и щёлкните viewport для ввода. Pause приостанавливает симуляцию, Stop возвращает Edit. Отдельное окно игры использует сохранённые данные проекта.',
 'Для звука добавьте учебный AudioTutorial, для своего поведения — компонент C++. Сохраните сцену после успешной сборки классов.'
 ]))+
 section('Ожидаемый результат','<p>После повторного открытия проекта восстанавливаются объекты, иерархия, transform и ссылки на ресурсы. Игра стартует с назначенной сцены и камерой. Изменения в Play не меняют Edit. При пропавшей картинке сначала проверьте камеру, видимость, материал и освещение.</p>')+
 section('Сцена и SaveGame','<p>Файл сцены описывает редактируемый мир. Game::Saves() отдельно сохраняет игровой прогресс. RequestSave/RequestLoad ставят операцию в очередь, LastResult сообщает Saved, Loaded или Failed. Наследник расширяет SerializeSaveGame через Section, Version и Field и вызывает базу. После Load перепривяжите ссылки, не используйте прежние указатели.</p><p>SaveGame не является сериализацией произвольной памяти: GPU-буферы, callbacks и указатели восстанавливаются механизмами движка и игры. Настройте стабильный gameId, чтобы слоты разных игр не смешивались.</p>'),
 ['Editor/Source/MainWindow.cpp','Engine/Include/Engine/SaveGame/SaveGame.h','Engine/Include/Engine/Components/Character.h','Engine/Include/Engine/Components/Light/PointLightComponent.h']]
];
const css=':root{color-scheme:dark}*{box-sizing:border-box}body{margin:0;background:#101d28;color:#e7eef4;font:18px/1.65 Segoe UI,Arial,sans-serif}main,nav,footer{max-width:1100px;margin:auto;padding:24px 32px}nav{display:flex;flex-wrap:wrap;gap:18px;border-bottom:1px solid #3a5264}a{color:#8ce1db}h1{font-size:46px;line-height:1.15}h2{color:#8ce1db;margin-top:42px}h3{font-size:22px}pre{background:#08121b;padding:22px;overflow:auto;font:15px/1.55 Consolas,monospace;border-left:3px solid #5cc2b9}li{margin:14px 0}.lead{font-size:24px;color:#b9d0df}.meta,footer{color:#a4b7c5;font-size:14px}section{border-bottom:1px solid #294151;padding-bottom:24px}code{overflow-wrap:anywhere}table{border-collapse:collapse}td,th{padding:12px;text-align:left;border:1px solid #42596b}@media(max-width:650px){main,nav,footer{padding:18px}h1{font-size:34px}}@media print{body{background:white;color:black}nav{display:none}a,h2{color:#064c50}pre{white-space:pre-wrap;background:#eee;color:black}section{break-inside:avoid}}';
const nav='<a href="index.html">Начать здесь</a><a href="API-REFERENCE.html">API Reference</a>'+pages.map(p=>`<a href="${p[0]}.html">${p[1]}</a>`).join('');
const output=(name,title,body)=>{
 const html=`<!doctype html>\n<html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>${esc(title)} — PainEngine</title><style>${css}</style></head><body><nav aria-label="Документация">${nav}</nav><main><p class="meta">PainEngine ${version} · Windows x64 · 7 ноября 2026</p><h1>${title}</h1>${body}</main><footer>Офлайн-документация. Источник истины: реализация текущего выпуска. Примеры и ресурсы доступны рядом с HTML.</footer></body></html>\n`;
 const target=path.join(dir,name+'.html');
 if(process.argv.includes('--check')) {if(!fs.existsSync(target)||fs.readFileSync(target,'utf8')!==html)throw Error('Stale guide '+target);} else fs.writeFileSync(target,html);
};
for(const [id,title,summary,body,sources] of pages) output(id,title,`<p class="lead">${summary}</p>${body.replaceAll('в Console','в панели «Консоль»')}`+(id==='editor'?shot('editor-assets.png','Инспектор, дерево объектов и контент-браузер.'):id==='materials'?shot('editor-material-graph.png','Граф материала и параметры текстур.'):id==='scene'?shot('player-properties.png','Свойства персонажа сцены.'): '')+section('Источники реализации','<ul>'+sources.map(s=>`<li><code>${s}</code></li>`).join('')+'</ul>'));
output('index','Начать здесь',`<p class="lead">От первого запуска до своей сцены с кодом, материалом и звуком.</p>`+section('Порядок изучения',steps(pages.map(p=>`<a href="${p[0]}.html">${p[1]}</a> — ${p[2]}`)))+section('Справочник и учебные файлы','<p><a href="API-REFERENCE.html">API Reference с поиском, объявлениями и примерами</a> работает без сервера. Тематические страницы дополняют справочник рабочими инструкциями.</p><p><a href="tutorial/examples/README.txt">Исходники туториалов</a>: вращение, ввод, спавн, модели, материалы, HUD, свойства и звук. <a href="tutorial/resources/README.txt">Ресурсы упражнений</a>: OBJ, PNG и PCM WAV.</p><p>В релизе рядом находятся две презентации и четыре самостоятельных проекта: CalmCity, CuriousCity, TiredBomber и AITrash. Начните с README в корне комплекта. Для редактирования кода нужен C++ toolchain, для запуска готовых примеров достаточно Windows x64 с D3D11.</p>'));
console.log(`Guides ${process.argv.includes('--check')?'verified':'built'}: ${pages.length+1}`);
