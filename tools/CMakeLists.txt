cmake_minimum_required(VERSION 3.16)

project(LokDefianceTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # UTF-8 sources, no dependency on the Visual C++ redistributable for the shipped exe
    add_compile_options(/utf-8 /W3)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Archive index, locals.bin, TextEntry and language block handling shared by the tools
add_library(common STATIC
    common/BigfileIndex.cpp
    common/CrBlocks.cpp
    common/LocalesBin.cpp
    common/MappedFile.cpp
    common/TextEntry.cpp
)
target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)

add_executable(LocaleExtractor
    extractor/CinematicExporter.cpp
    extractor/LocaleExtractor.cpp
    extractor/LocaleHandler.cpp
)
target_link_libraries(LocaleExtractor PRIVATE common)

# The patcher's logic is a library so that the tests can use it too
add_library(patcher_core STATIC
    patcher/PatchBackup.cpp
    patcher/Patcher.cpp
    patcher/Preflight.cpp
    patcher/TranslationSource.cpp
)
target_include_directories(patcher_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/patcher)
target_link_libraries(patcher_core PUBLIC common)

add_executable(PolishPatcher patcher/PolishPatcher.cpp)
target_link_libraries(PolishPatcher PRIVATE patcher_core)

# Tests on a synthetic archive (no game data needed)
enable_testing()
add_executable(PatcherTests tests/PatcherTests.cpp)
target_link_libraries(PatcherTests PRIVATE patcher_core)
add_test(NAME patcher COMMAND PatcherTests)
