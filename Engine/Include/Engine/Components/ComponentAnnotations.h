#pragma once
// Put ENGINE_COMPONENT() or ENGINE_COMPONENT("Display name") immediately before
// a class definition in a project Source header. See docs/COMPONENT-GENERATION.md.
// The build tool reads this marker; the C++ preprocessor deliberately erases it.
#define ENGINE_COMPONENT(...)
