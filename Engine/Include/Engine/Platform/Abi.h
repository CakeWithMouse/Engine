#pragma once

// Calling convention and symbol visibility belong to the target platform.
#ifndef ENGINE_MODULE_CALL
#if defined(_WIN32)
#define ENGINE_MODULE_CALL __cdecl
#else
#define ENGINE_MODULE_CALL
#endif
#endif

#if defined(_WIN32)
#define ENGINE_MODULE_EXPORT __declspec(dllexport)
#else
#define ENGINE_MODULE_EXPORT __attribute__((visibility("default")))
#endif
