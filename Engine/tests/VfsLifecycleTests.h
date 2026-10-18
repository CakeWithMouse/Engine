#pragma once

// VFS Lifecycle Tests (Scenarios A through E)
// Used by EngineTests to verify global VFS state across BaseEngine lifecycle.

void VfsLifecycle_ScenarioA();
void VfsLifecycle_ScenarioB();
void VfsLifecycle_ScenarioC();
void VfsLifecycle_ScenarioD();
void VfsLifecycle_ScenarioE();
void VfsLifecycle_PreserveMountSet();
void VfsLifecycle_ContentRootReverted();
void VfsLifecycle_PreMountedGameReverted();
