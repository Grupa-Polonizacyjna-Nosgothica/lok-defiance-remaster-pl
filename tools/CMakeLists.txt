cmake_minimum_required(VERSION 3.16)

project(LocaleExtractor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(LocaleExtractor
    MappedFile.cpp
    LocaleExtractor.cpp
)

target_include_directories(BigfileUtil PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
