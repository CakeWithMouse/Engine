include(${CMAKE_CURRENT_LIST_DIR}/Sources.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/SharedDependency.cmake)
list(REMOVE_ITEM PAIN_Engine_SOURCES ${PAIN_CORE_SOURCES})
add_library(Engine STATIC ${PAIN_Engine_SOURCES})
add_library(PainEngine::Engine ALIAS Engine)
target_link_libraries(Engine PUBLIC EngineCore d3d11 d3dcompiler dxgi dxguid user32 gdi32
    ole32 shell32 uuid xaudio2 mf mfplat mfreadwrite mfuuid)
target_include_directories(Engine PRIVATE Engine/ThirdParty/stb Engine/ThirdParty/meshoptimizer/src)
if(PAIN_ENABLE_ASSIMP)
    include(${CMAKE_CURRENT_LIST_DIR}/Assimp.cmake)
    target_sources(Engine PRIVATE Engine/Source/Render/SkeletalLoaderAssimp.cpp
        Engine/Source/Components/SpecificComponents/FBXComponent.cpp)
    target_link_libraries(Engine PUBLIC assimp::assimp)
    target_compile_definitions(Engine PUBLIC ENGINE_ENABLE_ASSIMP)
else()
    target_sources(Engine PRIVATE Engine/Source/Render/SkeletalLoaderStub.cpp)
endif()

list(APPEND PAIN_INSTALL_TARGETS Engine)
if(PAIN_ENABLE_JOLT)
    find_package(Jolt CONFIG REQUIRED)
    pain_bundle_dependency(Jolt::Jolt Jolt)
    target_link_libraries(Engine PRIVATE Jolt::Jolt)
    target_compile_definitions(Engine PUBLIC ENGINE_ENABLE_JOLT)
endif()

function(pain_deploy_content target source destination)
    if(NOT TARGET ${target}Content)
        add_custom_target(${target}Content)
        add_dependencies(${target} ${target}Content)
    endif()
    add_custom_command(TARGET ${target}Content POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${source}" "$<TARGET_FILE_DIR:${target}>/${destination}"
        VERBATIM)
endfunction()

foreach(game BasicExamples Pong SunGame Battle Katamari)
    if(game STREQUAL "Katamari" AND NOT PAIN_ENABLE_ASSIMP)
        continue()
    endif()
    add_executable(${game} ${PAIN_${game}_SOURCES})
    target_link_libraries(${game} PRIVATE Engine)
    target_include_directories(${game} PRIVATE Game/Common)
    set_target_properties(${game} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/${game}")
    pain_generate_components(${game} ${game} "Game/${game}/Source")
    pain_deploy_content(${game} "${PROJECT_SOURCE_DIR}/Engine/Content" EngineContent)
    pain_deploy_content(${game} "${PROJECT_SOURCE_DIR}/Game/${game}/Content" GameContent)
    add_custom_command(TARGET ${game} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/Game/${game}/RequiredFiles.txt" "$<TARGET_FILE_DIR:${game}>/RequiredFiles.txt")
endforeach()

set(runtime_dir "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Runtime")
add_library(RuntimeExample MODULE Game/RuntimeExample/Source/Module.cpp)
target_link_libraries(RuntimeExample PRIVATE Engine)
set_target_properties(RuntimeExample PROPERTIES PREFIX "" OUTPUT_NAME Game
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>/RuntimeExample"
    RUNTIME_OUTPUT_DIRECTORY "${runtime_dir}/Projects/RuntimeExample"
    LIBRARY_OUTPUT_DIRECTORY "${runtime_dir}/Projects/RuntimeExample")
pain_generate_components(RuntimeExample RuntimeExample Game/RuntimeExample/Source)
pain_deploy_content(RuntimeExample "${PROJECT_SOURCE_DIR}/Game/RuntimeExample/Content" Content)
add_custom_command(TARGET RuntimeExample POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PROJECT_SOURCE_DIR}/Game/RuntimeExample/Project.json" "$<TARGET_FILE_DIR:RuntimeExample>/Project.json")

add_library(SceneProject MODULE Game/SceneProject/Source/Module.cpp)
target_link_libraries(SceneProject PRIVATE Engine)
set_target_properties(SceneProject PROPERTIES PREFIX "" OUTPUT_NAME Game
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>/SceneProject"
    RUNTIME_OUTPUT_DIRECTORY "${runtime_dir}/Templates/SceneProject"
    LIBRARY_OUTPUT_DIRECTORY "${runtime_dir}/Templates/SceneProject")

add_executable(EngineRuntime Engine/Runtime/Main.cpp Engine/Runtime/Project.cpp
    Game/Common/PreviewScenes.cpp Game/Common/OrderedSceneGame.cpp Game/BasicExamples/Source/TrianglesScene.cpp
    Game/Pong/Source/PongGame.cpp Game/Pong/Source/PongScene.cpp Game/Pong/Source/Ball.cpp
    Game/Pong/Source/Rocket.cpp Game/Pong/Source/Wall.cpp Game/SunGame/Source/SunGame.cpp
    Game/Battle/Source/BattleGame.cpp Game/Battle/Source/BattleAi.cpp Game/Battle/Source/BattleBench.cpp)
target_link_libraries(EngineRuntime PRIVATE Engine)
target_include_directories(EngineRuntime PRIVATE Game/Common Game/BasicExamples/Source)
set_target_properties(EngineRuntime PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${runtime_dir}")
add_dependencies(EngineRuntime RuntimeExample SceneProject)
pain_deploy_content(EngineRuntime "${PROJECT_SOURCE_DIR}/Engine/Content" EngineContent)
foreach(game BasicExamples Pong SunGame Battle)
    pain_deploy_content(EngineRuntime "${PROJECT_SOURCE_DIR}/Game/${game}/Content" "Projects/${game}/Content")
    add_custom_command(TARGET EngineRuntime POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/Game/${game}/Project.json" "$<TARGET_FILE_DIR:EngineRuntime>/Projects/${game}/Project.json")
endforeach()
file(GENERATE OUTPUT "${runtime_dir}/EngineConfiguration.txt" CONTENT "$<CONFIG>\n")
add_custom_command(TARGET EngineRuntime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Engine/Version.json" "$<TARGET_FILE_DIR:EngineRuntime>/EngineVersion.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Engine/TemplateCompatibility.json" "$<TARGET_FILE_DIR:EngineRuntime>/TemplateCompatibility.json")

enable_language(RC)
add_executable(EngineLauncher WIN32 Game/Launcher/Source/Main.cpp Engine/Runtime/Project.cpp Game/Launcher/Source/Launcher.rc)
target_link_libraries(EngineLauncher PRIVATE PainBuildOptions gdiplus shell32 ole32 uuid user32 gdi32)
add_dependencies(EngineLauncher EngineRuntime)

if(BUILD_TESTING)
    add_executable(EngineTests ${PAIN_EngineTests_SOURCES})
    target_link_libraries(EngineTests PRIVATE Engine)
    pain_deploy_content(EngineTests "${PROJECT_SOURCE_DIR}/Engine/Content" EngineContent)
    set(cases BadVersion WrongKind MissingExport FailGameStart FailEditorStart RequestExit FailUpdate)
    set(case 0)
    foreach(fixture IN LISTS cases)
        math(EXPR case "${case} + 1")
        add_library(${fixture} MODULE Engine/tests/runtime/Fixture.cpp)
        target_link_libraries(${fixture} PRIVATE PainBuildOptions)
        target_compile_definitions(${fixture} PRIVATE FIXTURE_CASE=${case})
        set_target_properties(${fixture} PROPERTIES PREFIX ""
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/RuntimeFixtures"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/RuntimeFixtures")
        add_dependencies(EngineTests ${fixture})
    endforeach()
    add_test(NAME Engine COMMAND EngineTests)
    set_tests_properties(Engine PROPERTIES WORKING_DIRECTORY "$<TARGET_FILE_DIR:EngineTests>")
endif()

if(PAIN_BUILD_EDITOR)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets Test)
    find_package(Qt6 QUIET COMPONENTS Svg)
    add_library(Editor MODULE ${PAIN_Editor_SOURCES}
        Editor/Core/Logger.h Editor/Core/CommandRegistry.h Editor/Core/FileService.h
        Editor/Include/MainWindow.h Editor/Include/ViewportWidget.h
        Editor/Include/ObjectInspectorPanel.h Editor/Include/SceneObjectsPanel.h
        Editor/Include/MaterialEditorWindow.h Editor/UI/MainToolBar.h)
    set_target_properties(Editor PROPERTIES AUTOMOC ON PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${runtime_dir}" LIBRARY_OUTPUT_DIRECTORY "${runtime_dir}")
    target_include_directories(Editor PRIVATE Editor Editor/Core Editor/Include Editor/UI Editor/Host)
    target_link_libraries(Editor PRIVATE Engine Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
    if(TARGET Qt6::Svg)
        target_link_libraries(Editor PRIVATE Qt6::Svg)
        target_compile_definitions(Editor PRIVATE ENGINE_HAS_QTSVG)
    endif()
    target_precompile_headers(Editor PRIVATE Editor/pch.h)
    add_dependencies(EngineRuntime Editor)
    get_target_property(qt_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(qt_bin "${qt_qmake}" DIRECTORY)
    find_program(PAIN_WINDEPLOYQT windeployqt HINTS "${qt_bin}" REQUIRED)
    add_custom_command(TARGET Editor POST_BUILD COMMAND "${PAIN_WINDEPLOYQT}"
        --no-translations --no-opengl-sw "$<TARGET_FILE:Editor>" VERBATIM)
endif()

if(PAIN_ENABLE_ASSIMP OR PAIN_ENABLE_JOLT)
    foreach(target BasicExamples Pong SunGame Battle RuntimeExample SceneProject EngineRuntime)
        pain_deploy_dependencies(${target})
    endforeach()
    if(TARGET Katamari)
        pain_deploy_dependencies(Katamari)
    endif()
    if(BUILD_TESTING)
        pain_deploy_dependencies(EngineTests)
    endif()
    if(PAIN_BUILD_EDITOR)
        pain_deploy_dependencies(Editor)
    endif()
endif()
