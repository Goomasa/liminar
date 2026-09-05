cmake_minimum_required(VERSION 3.24)
project(liminar LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

add_library(tinygltf_v3 STATIC
    ${PROJECT_SOURCE_DIR}/externals/tinygltf/tiny_gltf_v3.c
)

target_include_directories(tinygltf_v3 PUBLIC
    ${PROJECT_SOURCE_DIR}/externals/tinygltf
)

set_target_properties(tinygltf_v3 PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
)

target_compile_definitions(tinygltf_v3 PRIVATE
    TINYGLTF3_ENABLE_FS
)

add_subdirectory(src)
