#include "ModelImportEditFlow.h"
#include "ImportSettingsDialog.h"
#include "EditorLocale.h"

#include <QDoubleSpinBox>
#include <QLabel>
#include <QVBoxLayout>

namespace ModelImportEditFlow
{

bool PrepareForEdit(ImportSettingsDialog& dialog, const EditorCore::ImportSettings& settings)
{
    dialog.SetEditMode(true);
    const bool legacy = !settings.AppliesGeometryScale();
    if (!legacy)
    {
        dialog.SetSettings(settings);
        return false;
    }

    EditorCore::ImportSettings display = settings;
    display.model.scale = 1.0f;
    dialog.SetSettings(display);

    if (auto* spin = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ImportScaleSpin")))
    {
        spin->setToolTip(Tr("Старый масштаб хранится на объекте на сцене. Здесь задаётся масштаб геометрии.",
                            "The old scale is stored on the scene object. This field sets the geometry scale."));
    }

    auto* notice = new QLabel(Tr("При сохранении настройки перейдут в версию 2: геометрия будет масштабироваться по новому значению, а Transform существующих объектов на сцене не меняется.",
                                 "Saving migrates to version 2: geometry is scaled by the new value, while existing scene objects' Transform stays unchanged."),
                              &dialog);
    notice->setObjectName(QStringLiteral("LegacyScaleNotice"));
    notice->setWordWrap(true);
    if (auto* layout = qobject_cast<QVBoxLayout*>(dialog.layout()))
    {
        layout->insertWidget(0, notice);
    }
    return true;
}

} // namespace ModelImportEditFlow