#pragma once
#include <string>

// Opt-in, module-local interface. Does not change GameComponent or historical
// DLL layouts. Values remain owned by SaveProjectData/LoadProjectData.
class EditorPropertyProvider {
public:
    virtual ~EditorPropertyProvider() = default;
    virtual std::string DescribeEditorProperties() const = 0;
};
