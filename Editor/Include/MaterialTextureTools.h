#pragma once

#include <QDialog>
#include <QFrame>
#include <QImage>
#include <QString>
#include <functional>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QMimeData;
class QTimer;

// Текстуры материала: правка HLSL поверхности и виджеты выбора. Слот существует, только если
// HLSL читает текстуру (компилятор выбрасывает неиспользуемые), поэтому AddSlot всегда пишет и чтение.
namespace MaterialTextures
{
    enum class Role { Color = 0, Opacity, Specular, Emissive };

    QString RoleLabel(Role role);
    // Свободное имя для слота: BaseMap, BaseMap2...
    QString DefaultName(const QString& source, Role role);
    // Почему имя не подходит; пустая строка — подходит.
    QString NameProblem(const QString& source, const QString& name);

    bool IsTextureFile(const QString& path);
    QImage Thumbnail(const QString& file, int size);

    // Объявляет Texture2D на свободном регистре и добавляет чтение в конец Surface.
    bool AddSlot(QString& source, const QString& name, Role role, QString& error);
    // Убирает объявление и строки, которые читают текстуру или её локальную переменную.
    bool RemoveSlot(QString& source, const QString& name, QString& error);
}

// Строка слота: миниатюра, путь, кнопки; на неё можно бросить файл из Files или проводника.
class MaterialTextureSlot final : public QFrame
{
public:
    std::function<void()> choose, clear, remove;
    std::function<void(const QString& file)> dropped;

    // path: /Game путь или пустая строка; file: тот же файл на диске.
    MaterialTextureSlot(const QString& name, const QString& path, const QString& file, QWidget* parent);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void Highlight(bool on);
};

// Выбор текстуры проекта: сетка миниатюр с поиском, «Нет текстуры» и импорт файла извне.
class MaterialTexturePicker final : public QDialog
{
public:
    // current: файл на диске, он будет выделен.
    MaterialTexturePicker(const QString& contentRoot, const QString& current, const QString& slot, QWidget* parent);

    // После accept: файл на диске, в том числе вне Content; пустая строка — «Нет текстуры».
    const QString& Selected() const { return selected_; }

private:
    void Import();
    void LoadNextThumbnail();

    QListWidget* list_ = nullptr;
    QTimer* thumbnails_ = nullptr;
    int nextThumbnail_ = 1; // первая строка — «Нет текстуры»
    QString selected_;
};

// Назначение и имя нового слота.
class MaterialAddTextureDialog final : public QDialog
{
public:
    MaterialAddTextureDialog(const QString& source, QWidget* parent);

    MaterialTextures::Role Role() const;
    QString Name() const;

private:
    void Validate();

    QString source_;
    QComboBox* role_ = nullptr;
    QLineEdit* name_ = nullptr;
    QLabel* problem_ = nullptr;
    QDialogButtonBox* buttons_ = nullptr;
    bool nameEdited_ = false;
};
