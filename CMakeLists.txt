cmake_minimum_required(VERSION 3.21)

project(ArbitraryFastTravel VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(CommonLibSSE
    GIT_REPOSITORY https://github.com/alandtse/CommonLibSSE-NG.git
    GIT_TAG 8b032fa992750d654d6d38a33731714d8b86be1f
)
FetchContent_MakeAvailable(CommonLibSSE)
include("${commonlibsse_SOURCE_DIR}/cmake/CommonLibSSE.cmake")

# Only the research baseline is advertised until travel hooks are validated.
add_commonlibsse_plugin(${PROJECT_NAME}
    AUTHOR "Apocrypher00"
    COMPATIBLE_RUNTIMES 1.6.1170.0
    SOURCES src/plugin.cpp
)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_compile_options(${PROJECT_NAME} PRIVATE /Zc:preprocessor /utf-8)

include(cmake/Packaging.cmake)
