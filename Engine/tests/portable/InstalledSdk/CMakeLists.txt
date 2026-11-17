cmake_minimum_required(VERSION 3.24)
project(PortableSdkConsumer LANGUAGES CXX)
find_package(PainEngine CONFIG REQUIRED)
add_executable(SdkConsumer Main.cpp)
target_link_libraries(SdkConsumer PRIVATE PainEngine::Core)
enable_testing()
add_test(NAME InstalledCore COMMAND SdkConsumer)
add_test(NAME InstalledGenerator COMMAND ${CMAKE_COMMAND}
    -DGENERATOR=$<TARGET_FILE:PainEngine::ComponentGenerator>
    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/generator-test
    -P ${CMAKE_CURRENT_SOURCE_DIR}/../GeneratorTest.cmake)
