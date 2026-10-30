#pragma once
#include <atomic>

class IEditorHost;

void RunEditorMainWindow(IEditorHost* host, std::atomic<bool>* ready);
