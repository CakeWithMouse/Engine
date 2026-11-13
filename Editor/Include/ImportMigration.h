#pragma once
#include <QString>

namespace ImportMigration {

// C++ markers created by "Add C++ to Project": Project.code.json, Source/, Game.vcxproj.
bool IsCodeProject(const QString& projectDir);

// Backs up the current module, copies the runtime template Game.dll to a fresh
// Binaries/Migrated/<stamp>/ path and repoints Project.json (the loaded DLL cannot
// be overwritten). Returns the new relative module path, or empty when migration
// is unnecessary or impossible (error set).
QString MigrateNoCodeModule(const QString& projectDir, const QString& runtimeDir, QString* error);

}