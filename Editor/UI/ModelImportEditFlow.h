#pragma once

#include "ImportSettings.h"

class ImportSettingsDialog;

namespace ModelImportEditFlow
{
// Prepares the dialog for "Настройки импорта…". For a legacy sidecar (version < 2) the
// scale is shown as 1.0 because the old multiplier lives on the scene objects' Transform;
// a notice explains that saving migrates to version 2 and scales geometry instead.
// Returns true when the loaded sidecar was legacy (version < 2).
bool PrepareForEdit(ImportSettingsDialog& dialog, const EditorCore::ImportSettings& settings);
}