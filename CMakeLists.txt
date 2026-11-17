cmake_minimum_required(VERSION 3.24)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Version.json" pain_version_json)
string(JSON pain_version GET "${pain_version_json}" version)
project(PainEngine VERSION ${pain_version} LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")

option(PAIN_BUILD_DESKTOP "Build the Windows/D3D11 runtime and games" ${WIN32})
option(PAIN_BUILD_EDITOR "Build the Qt editor (requires Qt 6)" OFF)
option(PAIN_ENABLE_ASSIMP "Enable Assimp model import" OFF)
option(PAIN_ENABLE_JOLT "Enable Jolt rigid-body physics" OFF)
set(PAIN_HOST_COMPONENT_GENERATOR "" CACHE FILEPATH "Native ComponentGenerator executable when cross compiling")
if(PAIN_BUILD_DESKTOP AND NOT WIN32)
    message(FATAL_ERROR "The desktop renderer still requires Windows/D3D11. Use PAIN_BUILD_DESKTOP=OFF for EngineCore and native tools.")
endif()
if(PAIN_BUILD_DESKTOP AND (NOT MSVC OR NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64"))
    message(FATAL_ERROR "Desktop and its project SDK currently require x64 MSVC or clang-cl. Use PAIN_BUILD_DESKTOP=OFF for other targets.")
endif()
if(PAIN_BUILD_EDITOR AND NOT PAIN_BUILD_DESKTOP)
    message(FATAL_ERROR "PAIN_BUILD_EDITOR requires PAIN_BUILD_DESKTOP")
endif()

add_library(PainBuildOptions INTERFACE)
target_compile_features(PainBuildOptions INTERFACE cxx_std_17)
target_include_directories(PainBuildOptions INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/GLM-master>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(PainBuildOptions INTERFACE
    $<$<PLATFORM_ID:Windows>:UNICODE;_UNICODE;NOMINMAX;WIN32_LEAN_AND_MEAN>)
if(MSVC)
    target_compile_options(PainBuildOptions INTERFACE /utf-8 /permissive- /EHsc)
endif()

set(PAIN_CORE_SOURCES
    Engine/Source/Resources/Paths.cpp
    Engine/Source/Resources/VirtualFileSystem.cpp
    Engine/Source/Resources/VfsRead.cpp
    Engine/Source/Resources/LocalVfs.cpp
    Engine/Source/Resources/HostVfsProxy.cpp
    Engine/Source/Scene/Trace.cpp
    Engine/Source/Script/ScriptCatalog.cpp
    Engine/Source/Script/ScriptProgram.cpp)
add_library(EngineCore STATIC ${PAIN_CORE_SOURCES})
add_library(PainEngine::Core ALIAS EngineCore)
set_target_properties(EngineCore PROPERTIES EXPORT_NAME Core)
target_link_libraries(EngineCore PUBLIC PainBuildOptions Threads::Threads)

add_executable(ComponentGenerator Engine/Tools/ComponentGenerator/ComponentGenerator.cpp)
target_link_libraries(ComponentGenerator PRIVATE PainBuildOptions)
include(cmake/Components.cmake)

if(BUILD_TESTING)
    add_executable(PortableCoreTests Engine/tests/portable/CoreTests.cpp)
    target_link_libraries(PortableCoreTests PRIVATE EngineCore)
    add_test(NAME PortableCore COMMAND PortableCoreTests)
    if(NOT CMAKE_CROSSCOMPILING)
        add_test(NAME ComponentGeneratorPortable COMMAND ${CMAKE_COMMAND}
            -DGENERATOR=$<TARGET_FILE:ComponentGenerator>
            -DWORK=${CMAKE_BINARY_DIR}/generator-test
            -P ${CMAKE_CURRENT_SOURCE_DIR}/Engine/tests/portable/GeneratorTest.cmake)
    endif()
endif()

set(PAIN_INSTALL_TARGETS PainBuildOptions EngineCore ComponentGenerator)
if(PAIN_BUILD_DESKTOP)
    include(cmake/Desktop.cmake)
endif()
install(TARGETS ${PAIN_INSTALL_TARGETS} EXPORT PainEngineTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY Engine/Include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY Engine/ThirdParty/GLM-master/glm Engine/ThirdParty/nlohmann
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")
install(FILES Engine/ThirdParty/GLM-master/copying.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/PainEngine/licenses/glm)
install(FILES Engine/ThirdParty/nlohmann/LICENSE.MIT DESTINATION ${CMAKE_INSTALL_DATADIR}/PainEngine/licenses/nlohmann)
if(PAIN_BUILD_DESKTOP)
    install(FILES Engine/ThirdParty/meshoptimizer/LICENSE.md DESTINATION ${CMAKE_INSTALL_DATADIR}/PainEngine/licenses/meshoptimizer)
endif()
install(EXPORT PainEngineTargets NAMESPACE PainEngine:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PainEngine)
configure_package_config_file(cmake/PainEngineConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/PainEngineConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PainEngine)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/PainEngineConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/PainEngineConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/PainEngineConfigVersion.cmake cmake/Components.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PainEngine)
if(PAIN_BUILD_DESKTOP)
    install(DIRECTORY cmake/ProjectTemplate/ DESTINATION ${CMAKE_INSTALL_DATADIR}/PainEngine/ProjectTemplate)
    install(FILES Engine/BuildSupport/ProjectTools.ps1 Engine/BuildSupport/Toolchain.ps1
        Engine/BuildSupport/CMakeProject.ps1 DESTINATION .)
    install(FILES cmake/ProjectTemplate/CMakeLists.txt DESTINATION Build RENAME Game.cmake)
    string(REPLACE "\\" "/" pain_sdk_compiler "${CMAKE_CXX_COMPILER}")
    string(REPLACE "\\" "\\\\" pain_sdk_flags "${CMAKE_CXX_FLAGS}")
    string(REPLACE "\"" "\\\"" pain_sdk_flags "${pain_sdk_flags}")
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/Sdk-$<CONFIG>.json" CONTENT
        "{\n  \"format\": 3, \"version\": \"${PROJECT_VERSION}\", \"configuration\": \"$<CONFIG>\",\n  \"platform\": \"x64\", \"buildSystem\": \"cmake\", \"toolset\": \"cmake\",\n  \"cmake\": {\"compiler\": \"${pain_sdk_compiler}\", \"compilerId\": \"${CMAKE_CXX_COMPILER_ID}\", \"compilerFlags\": \"${pain_sdk_flags}\"}\n}\n")
    install(FILES "${CMAKE_BINARY_DIR}/Sdk-$<CONFIG>.json" DESTINATION . RENAME Sdk.json)
    add_custom_target(ProjectSdk
        COMMAND ${CMAKE_COMMAND} "-DBINARY=${CMAKE_BINARY_DIR}"
            "-DDESTINATION=${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Runtime/SDK" "-DCONFIGURATION=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PublishSdk.cmake"
        DEPENDS Engine ComponentGenerator VERBATIM)
    add_dependencies(EngineRuntime ProjectSdk)
endif()
