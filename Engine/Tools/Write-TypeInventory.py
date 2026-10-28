"""Documentation inventory, not runtime reflection. No third-party/test/archive types."""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]
pattern = re.compile(r'^[ \t]*(?:template\s*<[^\n]*>\s*)?(class|struct)\s+(\w+)\s*([^;{}\n]*(?:\n[ \t]*:[^;{}]+)?)\s*\{', re.M)

def strip_comments(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*', lambda m: ''.join('\n' if c=='\n' else ' ' for c in m[0]), text, flags=re.S)

def base_clause(tail):
    depth=0
    for i,char in enumerate(tail):
        if char=='<':depth+=1
        elif char=='>':depth=max(0,depth-1)
        elif char==':' and depth==0 and (i==0 or tail[i-1]!=':') and (i+1==len(tail) or tail[i+1]!=':'):
            return tail[i+1:].strip()
    return '—'

def inventory(roots):
    rows=[]
    for root in roots:
        for path in sorted((repo/root).rglob('*')):
            if path.suffix not in ('.h','.cpp','.inl') or any(p in ('build','Archive','ThirdParty','tests') for p in path.relative_to(repo).parts): continue
            source=path.read_text(encoding='utf-8-sig')
            clean=strip_comments(source)
            for match in pattern.finditer(clean):
                kind,name,tail=match.groups()
                if name=='name' and path.name=='Delegates.h': continue # macro parameter, not a concrete type
                base=base_clause(tail)
                base=re.sub(r'\s+',' ',base).replace('|','\\|')
                label=name
                if path.name=='StandardTypes.h': label='SceneProject::'+name
                elif path.name=='LegacyEditApi.h': label='EngineLegacyV4::'+name
                elif name=='PointLight' and path.name=='Scene.h': label='SceneComponentSnapshot::PointLight'
                elif name=='UpdateScope': label='Scene::UpdateScope'
                line=clean.count('\n',0,match.start())+1
                rows.append(dict(kind=kind,name=label,base=base,final=bool(re.search(r'\bfinal\b',tail)),path=path.relative_to(repo).as_posix(),line=line))
    return rows

public=inventory(['Engine/Include/Engine'])
internal=inventory(['Engine/Source','Engine/Runtime'])
games=inventory(['Game'])
by_name={r['name'].split('::')[-1]:r for r in public+games}
def derives(r, base, seen=None):
    seen=set() if seen is None else seen
    if r['name'] in seen:return False
    seen.add(r['name'])
    names=re.findall(r'\b[A-Za-z_]\w*\b',r['base'])
    return base in names or any(n in by_name and derives(by_name[n],base,seen) for n in names)

standard={'Cube':'Primitive.Cube','Sphere':'Primitive.Sphere','Plane':'Primitive.Plane','Cylinder':'Primitive.Cylinder','Cone':'Primitive.Cone','Pyramid':'Primitive.Pyramid','PointLight':'Light.Point','Sparks':'Particle.Sparks','Smoke':'Particle.Smoke','Sky':'Environment.Sky'}
def visibility(r):
    name=r['name']
    if name.startswith('SceneProject::') and name.split('::')[-1] in standard:
        return 'Да: '+standard[name.split('::')[-1]], 'Да, из палитры', 'final; используйте базовый компонент'
    if name=='Game': return 'Сам базовый тип скрыт; зарегистрированные наследники видны','Выбор Game, не объект сцены','Да: ENGINE_GAME'
    if name=='SkeletalMeshComponent': return 'Как Engine.Model после импорта; C++ имя скрыто','Импорт модели или зарегистрированный наследник','Да: ENGINE_COMPONENT; модель/ресурсы задаются явно'
    if name=='GameComponent' or (r['kind']=='class' and derives(r,'GameComponent') and name!='ProjectObjectComponent'):
        return 'Исходный C++ тип скрыт; зарегистрированные наследники видны','Через конкретного наследника и фабрику','Да: ENGINE_COMPONENT, если конструктор доступен'
    if name=='ProjectObjectComponent': return 'Служебная оболочка скрыта; виден зарегистрированный T','Оболочку создаёт фабрика Object','final; специализируется ProjectObjectAdapter<T>'
    if r['kind']=='struct': return 'Нет, описание данных / API / helper','Нет отдельного действия','Не регистрация; ENGINE_OBJECT + адаптер для своего типа'
    return 'Нет, служебный тип','Нет отдельного действия', ('Нет: final' if r['final'] else 'Служебный API; не готовый игровой тип')

out=['# Видимость типов и возможности проектов с C++','',
'Состояние реализации на 2026-10-26. Таблицы отделяют наличие типа в C++ SDK от его отображения и создания в редакторе. Регистрация пользовательских классов выполняется при сборке; неподписанные метаданными классы автоматически в палитру не попадают.','',
'## Возможности пользователя','',
'| Возможность | Где доступна | Условия и границы |','|---|---|---|',
'| Проект без кода | Лаунчер → Создать проект | Без флажка C++; готовая DLL, компилятор не требуется |',
'| Проект с кодом | Лаунчер → Создать проект → C++ | SDK закреплённого выпуска и Visual Studio C++ tools |',
'| Добавить C++ позже | Лаунчер или меню Проект / C++ | Для штатной DLL; сцены и ресурсы сохраняются; при ошибке прежний модуль остаётся активным |',
'| Новый класс | Проект / C++ → Добавить C++ класс | Component, Game, Object; имя, родитель и заголовок; файлы .h/.cpp |',
'| Родительские методы | Созданные исходники | Game: AfterInitialize, PreUpdate, TickComponents; Component: Tick; вызов непосредственного родителя |',
'| Свой компонент | ENGINE_COMPONENT | Публичное однозначное наследование; конкретный класс с доступным default constructor |',
'| Своя игра | ENGINE_GAME | Публичный конкретный наследник Game; доступный default constructor |',
'| Другая иерархия | ENGINE_OBJECT | Реальный T не обязан наследовать классы Engine; Scene владеет неотрисовываемой оболочкой |',
'| Параметризованный Object | ProjectObjectAdapter<T>::Create | Автор явно предоставляет фабрику и кодек; автоматического подбора аргументов нет |',
'| Классы проекта | Проект / C++ → Классы проекта | Каталог последней успешной сборки и встроенные типы; Game / Component / Object |',
'| Выбор Game | Диалог классов → Выбрать Game | Применяется к следующему Play и отдельному запуску; хранится в Content/ProjectGame.txt |',
'| Создание Component / Object | Палитра сцены | Object имеет пометку [Object], не рисует геометрию; выбирается в иерархии |',
'| Категории создания | Сцена / объекты | Примитивы, Свет, Частицы, Окружение, Классы проекта; общая кнопка добавления |',
'| Частицы | Частицы → Искры / Дым | GPU-пресеты, предпросмотр в Edit; Pause останавливает симуляцию |',
'| Скайбокс | Окружение → Базовое небо | Процедурный градиент без внешних текстур; не перехватывает выбор |',
'| Выделение | ЛКМ / иерархия / Tab | Золотистая рамка границ, подсветка строки, имя и тип в инспекторе; Esc снимает выбор |',
'| Встроенный Play | Панель редактора | Пользовательский Game, одна симуляция за кадр; нейтральный Game в edit preview |',
'| External / Играть | Редактор / лаунчер | Пользовательский Game, сохранённая стартовая сцена |',
'| Пользовательские значения | Данные выбранного объекта | JSON через явный SaveProjectData/LoadProjectData либо Object adapter; version обязателен |',
'| Undo / Redo / Duplicate / Play copy | Редактор | Используют кодек; ссылки remap-ятся объявленным автором RemapProjectReferences/Remap |',
'| Устойчивый ID | Второй аргумент аннотации | Например ENGINE_COMPONENT("Mover", "MyGame.Mover"); позволяет переименовать C++ класс |',
'| Сборка проекта | Лаунчер / меню C++ / Build.ps1 | Только проектная DLL, готовые библиотеки SDK; отдельная папка кандидата |',
'| Ошибка сборки | Console / CodeBuild.log / validate-error.log / startup-error.log | Активный module path не переключается до проверки сцен, Game factory и трёх кадров запуска |',
'| Применение | Повторное открытие редактора | Живое игровое состояние сбрасывается; автоматического переноса памяти нет |',
'| Visual Studio | Game.sln | SDK ищется по lock-выпуску в установленном store; применение сборки через Build.ps1 |',
'| Старые проекты | Лаунчер и адаптер Edit 4 / Scene 2 | Старые фиксированные C API сохранены; выпуск выбирается по lock |',
'| Редактирование всех полей C++ | Недоступно | Нужен объявленный кодек; private/transient поля не отражаются автоматически |',
'| Абстрактные и не создаваемые классы | Не появляются как создаваемые типы | Ошибка компиляции при неподходящей аннотации; обычный неотмеченный базовый класс допустим |',
'| Классы движка без регистрации | Только C++ API | Само присутствие заголовка не создаёт кнопку в редакторе |','',
'## Встроенные типы нового проекта','',
'| ID | Имя в редакторе | Реализация | Действие пользователя |', '|---|---|---|---|',
'| Primitive.Cube | Куб | SceneProject::Cube → CubeComponent | Добавить из палитры |',
'| Primitive.Sphere | Сфера | SceneProject::Sphere → SphereComponent | Добавить из палитры |',
'| Primitive.Plane | Плоскость | SceneProject::Plane → Mesh | Добавить из палитры |',
'| Primitive.Cylinder | Цилиндр | SceneProject::Cylinder → Mesh | Добавить из палитры |',
'| Primitive.Cone | Конус | SceneProject::Cone → Mesh | Добавить из палитры |',
'| Primitive.Pyramid | Пирамида | SceneProject::Pyramid → Mesh | Добавить из палитры |',
'| Light.Point | Точечный свет | SceneProject::PointLight → PointLightComponent | Добавить из палитры |',
'| Particle.Sparks | Искры | SceneProject::Sparks → ParticleSystemComponent | Частицы → Добавить |',
'| Particle.Smoke | Дым | SceneProject::Smoke → ParticleSystemComponent | Частицы → Добавить |',
'| Environment.Sky | Базовое небо | SceneProject::Sky → SkyboxComponent | Окружение → Добавить |',
'| Engine.Model | Импортированная модель | SkeletalMeshComponent | Импортировать ресурс и добавить модель; не пустая фабрика палитры |','',
'## Все определения классов в публичных заголовках Engine','',
'Именованные определения `class`, включая служебные и шаблонные. Forward declarations, enum, примеры в комментариях и внешние библиотеки исключены. Специализации делегатов указаны отдельно по строке исходника.','',
'| Класс | База | Видимость пользователю редактора | Создание | Наследование / применение | Источник |','|---|---|---|---|---|---|']
for r in public:
    if r['kind']!='class':continue
    visible,spawn,inherit=visibility(r)
    out.append(f"| `{r['name']}` | `{r['base']}` | {visible} | {spawn} | {inherit} | [{r['path']}:{r['line']}](../../{r['path']}) |")

out += ['', '## Структуры данных и API в публичных заголовках', '',
'Эти структуры доступны C++ коду, но не являются элементами палитры. Для самостоятельного игрового типа из другой иерархии нужна ENGINE_OBJECT и подходящий адаптер.', '',
'| Структура | База | Источник |','|---|---|---|']
for r in public:
    if r['kind']=='struct':out.append(f"| `{r['name']}` | `{r['base']}` | [{r['path']}:{r['line']}](../../{r['path']}) |")

out += ['', '## Классы и типы текущих примеров игр', '',
'Примеры Game/BasicExamples, Pong, SunGame, Battle, Katamari сохраняют существующий запуск. Их классы не копируются в пользовательский проект и автоматически не регистрируются в его каталоге. Архив Game/Archive и тестовые классы исключены.', '',
'| Тип | База | Видимость в новом пользовательском проекте | Применение | Источник |','|---|---|---|---|---|']
for r in games:
    application='Внутренняя обвязка / данные примера'
    if derives(r,'Game'):application='Game-наследник примера; нужен перенос исходников и ENGINE_GAME для выбора в своём проекте'
    elif derives(r,'GameComponent'):application='Компонент примера; нужен перенос исходников и ENGINE_COMPONENT'
    elif 'Preview' in r['name']:application='Встроенный preview выбранного примера'
    out.append(f"| `{r['name']}` | `{r['base']}` | Нет | {application} | [{r['path']}:{r['line']}](../../{r['path']}) |")

out += ['', '## Внутренние определения Engine', '',
'Определения в Engine/Source и Engine/Runtime не входят в SDK Include. Пользователь не видит их в редакторе и не должен подключать приватные исходники для создания игры.', '',
'| Тип | База | Видимость / создание | Источник |','|---|---|---|---|']
for r in internal:
    out.append(f"| `{r['name']}` | `{r['base']}` | Внутренний; в редакторе недоступен | [{r['path']}:{r['line']}](../../{r['path']}) |")
out += ['', '## Сгенерированные пользовательские классы', '',
'| Класс | Родитель | Что видит пользователь |', '|---|---|---|',
'| ProjectGame | Game | В списке Game; используется в Play и отдельной игре |',
'| ProjectComponent | GameComponent | В палитре; геометрию и логику задаёт автор |',
'| Класс мастера Component | Указанный компонент | В палитре после успешной сборки |',
'| Класс мастера Game | Указанный Game-наследник | В выборе игры после успешной сборки |',
'| Класс мастера Object | Любой допустимый родитель либо без базы | В палитре как [Object]; без геометрии |',
'| Неотмеченный пользовательский класс | Любой допустимый родитель | Только C++ код, в редакторе не отображается |','',
'Список построен из production-исходников скриптом Engine/Tools/Write-TypeInventory.py. Таблица показывает объявления и фактический способ экспонирования; она не обещает, что произвольный служебный класс безопасно превращается в игровой объект без адаптации.','']
target=repo/'Engine/docs/PROJECT-CAPABILITIES.md'
target.write_text('\n'.join(out),encoding='utf-8')
print(f'{target}: {sum(r["kind"]=="class" for r in public)} public class definitions, {sum(r["kind"]=="struct" for r in public)} public struct definitions, {len(games)} game types, {len(internal)} internal definitions')
