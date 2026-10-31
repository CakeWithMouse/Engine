PainEngine — примеры туториала, редакция 4, 31.10.2026
SDK: 2026.10.31.2, C++17. Это исходники для C++ проекта лаунчера.

1. Создайте C++ проект. RotatingCube и ProjectGame уже генерируются движком.
   Приложенные файлы показывают минимальный вариант; текущий шаблон также имеет свойства Speed/Enabled. Не создавайте дубликаты.
   Play добавляет один вращающийся куб. Скорость 90 градусов/с при масштабе 1.
2. Скопируйте InputCube.h/.cpp в Source. Выполните Hot Reload в Edit.
   Добавьте один InputCube из «Классы проекта», нажмите Play и щёлкните viewport.
   R переключает вращение, N создаёт RotatingCube рядом. Stop удаляет копию Play.
   ProjectGame не создаёт дополнительный куб, если в сцене есть InputCube.
3. ModelExample.h/.cpp — вспомогательные функции, не добавляемый компонент.
   Импортируйте свою модель, затем вызовите из AfterInitialize после базового метода:
     SpawnModel(*this, "MyModel", "/Game/Models/blend-packed/blend-packed.glb");
   Подставьте реальный путь из вашего Content, подключите ModelExample.h.
   Сохраняйте .import.json, Materials и Textures рядом с моделью.
   Уникальное имя обязательно. Повторный вызов с занятым именем выдаст ошибку.
4. AssignMaterial(object, "/Game/Materials/Wood.mat") назначает готовый материал.
   AssignTexture(object, "/Game/Textures/grid.png") сохраняет текущий материал.
   Пути — примеры: создайте/импортируйте свои ресурсы. Изображения не входят в архив.
   У FBX/BLEND обычно меняйте BaseMap материала, а не отдельную текстуру объекта:
   отдельная текстура дополнительно умножается на результат материала.
5. MyController.h — необязательный вариант для слайда «Своя схема управления».
   В ProjectGame.h добавьте метод:
     std::unique_ptr<CharacterController> CreateCharacterController() override;
   В ProjectGame.cpp подключите MyController.h и добавьте:
     std::unique_ptr<CharacterController> ProjectGame::CreateCharacterController() {
         return std::make_unique<MyController>(*this);
     }
   Этот пример полностью заменяет WASD/ПКМ на I. Персонажа назначьте в настройках Play.

Фрагменты на слайдах сокращены; файлы здесь содержат необходимые includes и функции.
Не заменяйте исходники уже доработанного проекта целиком: переносите нужные части.

PropertyTutorial/ — отдельный пример с пользовательскими свойствами.
Готовый проект на рабочей машине: O:/Engine/Проекты/Туториал-Свойства.
В архиве лежат исходники и сцена, без DLL и runtime. Для нового C++ проекта
скопируйте PropertyTutorial/Source/RotatingCube.h/.cpp вместо базовых файлов,
а Content/Scenes/Main.scene.json — в Content/Scenes. Соберите проект новым SDK.
Подробное упражнение — PropertyTutorial/README.md. Create.ps1 предназначен
для Game/PropertyTutorial внутри репозитория; не запускайте его из архива.
