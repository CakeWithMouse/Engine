# Bundle a shared desktop dependency without leaking package-manager paths into the SDK.
function(pain_bundle_dependency dependency name)
    get_target_property(dependency_type ${dependency} TYPE)
    if(NOT dependency_type STREQUAL "SHARED_LIBRARY")
        message(FATAL_ERROR "${dependency} must be shared for SDK packaging (use the x64-windows triplet)")
    endif()
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/PainDependency-${name}-$<CONFIG>.cmake" CONTENT
        "if(NOT TARGET ${dependency})\n  add_library(${dependency} SHARED IMPORTED)\nendif()\nset_property(TARGET ${dependency} APPEND PROPERTY IMPORTED_CONFIGURATIONS $<UPPER_CASE:$<CONFIG>>)\nset_target_properties(${dependency} PROPERTIES\n  IMPORTED_LOCATION_$<UPPER_CASE:$<CONFIG>> \"\${CMAKE_CURRENT_LIST_DIR}/../../../${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:${dependency}>\"\n  IMPORTED_IMPLIB_$<UPPER_CASE:$<CONFIG>> \"\${CMAKE_CURRENT_LIST_DIR}/../../../${CMAKE_INSTALL_LIBDIR}/$<TARGET_LINKER_FILE_NAME:${dependency}>\")\n")
    install(FILES "${CMAKE_BINARY_DIR}/PainDependency-${name}-$<CONFIG>.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PainEngine)
    install(FILES "$<TARGET_LINKER_FILE:${dependency}>" DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY "$<TARGET_FILE_DIR:${dependency}>/" DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILES_MATCHING PATTERN "*.dll")
    # vcpkg keeps dependency notices alongside the package configuration files.
    get_target_property(dependency_includes ${dependency} INTERFACE_INCLUDE_DIRECTORIES)
    foreach(include_dir IN LISTS dependency_includes)
        if(IS_DIRECTORY "${include_dir}/../share")
            file(GLOB notices "${include_dir}/../share/*/copyright")
            foreach(notice IN LISTS notices)
                get_filename_component(package_dir "${notice}" DIRECTORY)
                get_filename_component(package_name "${package_dir}" NAME)
                install(FILES "${notice}" DESTINATION ${CMAKE_INSTALL_DATADIR}/PainEngine/licenses/${package_name})
            endforeach()
        endif()
    endforeach()

    list(APPEND PAIN_SDK_DEPENDENCIES "${name}")
    set(PAIN_SDK_DEPENDENCIES "${PAIN_SDK_DEPENDENCIES}" PARENT_SCOPE)
    set_property(GLOBAL APPEND PROPERTY PAIN_SHARED_RUNTIME_TARGETS ${dependency})
endfunction()

function(pain_deploy_dependencies target)
    get_property(dependencies GLOBAL PROPERTY PAIN_SHARED_RUNTIME_TARGETS)
    foreach(dependency IN LISTS dependencies)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} "-DSOURCE=$<TARGET_FILE_DIR:${dependency}>"
                "-DDESTINATION=$<TARGET_FILE_DIR:${target}>"
                -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CopyDlls.cmake" VERBATIM)
    endforeach()
endfunction()
