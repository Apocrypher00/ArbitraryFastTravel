cmake_minimum_required(VERSION 3.21)

project(ArbitraryFastTravel VERSION 0.1.0 LANGUAGES CXX)

# Keep the package manifest aligned with the version used for plugin metadata
# and release archive names.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" vcpkgManifest)
string(JSON vcpkgVersion GET "${vcpkgManifest}" "version-string")
if(NOT "${vcpkgVersion}" STREQUAL "${PROJECT_VERSION}")
    message(FATAL_ERROR
        "Project version ${PROJECT_VERSION} does not match vcpkg manifest version ${vcpkgVersion}.")
endif()

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

# CommonLib generates the SKSE plugin metadata from this runtime list.
# Advertise only 1.6.1170, where the native callback layout and hooks were verified.
add_commonlibsse_plugin(${PROJECT_NAME}
    AUTHOR "Apocrypher00"
    COMPATIBLE_RUNTIMES 1.6.1170.0
    SOURCES src/plugin.cpp src/Hooks.cpp src/MarkerSelectionOverride.cpp src/TravelDialog.cpp src/GameText.cpp src/Config.cpp
)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_compile_options(${PROJECT_NAME} PRIVATE /Zc:preprocessor /utf-8)

find_package(minhook CONFIG REQUIRED)
find_path(SIMPLEINI_INCLUDE_DIR SimpleIni.h REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE "${SIMPLEINI_INCLUDE_DIR}")
target_link_libraries(${PROJECT_NAME} PRIVATE minhook::minhook)

# Keep the default INI beside development builds, like WMS.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/ArbitraryFastTravel.ini"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/ArbitraryFastTravel.ini"
)

include(cmake/Packaging.cmake)
