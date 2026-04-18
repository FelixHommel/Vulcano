cmake_minimum_required(VERSION 3.25)
project(
    Vulcano
    DESCRIPTION "Modern Vulkan renderer"
    LANGUAGES C CXX
    VERSION 0.0.1
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_definitions(PROJ_ROOT="${CMAKE_SOURCE_DIR}/")

find_package(Vulkan REQUIRED)
include(cmake/VcpkgDependencies.cmake)

include(cmake/DefaultProjectSettings.cmake)
include(cmake/CodeCoverage.cmake)

add_library(project_warnings INTERFACE)
include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_warnings)

add_subdirectory(src)
