# Project code portability finding (before fix)

Before the r7 relocation fix, `Source/ProjectGame.cpp` derived SaveData, audio logs, and frame dump output from `std::filesystem::path(__FILE__)`. The absolute source checkout was embedded in the game DLL, so a relocated project mounted `/Game` from its new Content folder but wrote gameplay output back into the original CalmCityUpgraded tree. The project-local fix now resolves `/Game` through `EngineModuleVfs::Resolve` and writes to the parent project SaveData folder.

Evidence: relocated save/load logs before the fix are in the scratch run output, and the fixed source/build and relocated second-process save/load result are included with `portable-project` evidence.
