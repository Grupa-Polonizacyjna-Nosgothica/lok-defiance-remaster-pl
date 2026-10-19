cmake_minimum_required(VERSION 3.16)

project(LokDefianceTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

add_executable(PolishPatcher
    patcher/PatchBackup.cpp
    patcher/Patcher.cpp
    patcher/PolishPatcher.cpp
    patcher/Preflight.cpp
    patcher/TranslationSource.cpp
)
target_link_libraries(PolishPatcher PRIVATE common)
