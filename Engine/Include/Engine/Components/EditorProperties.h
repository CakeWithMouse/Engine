#pragma once
#include <string>

// Legacy source compatibility only. New components override the method on
// GameComponent directly. Keep this independent base so old multiple-inheritance
// declarations still compile; do not alias it to GameComponent.
class EditorPropertyProvider {
public:
    virtual ~EditorPropertyProvider() = default;
    virtual std::string DescribeEditorProperties() const = 0;
};
