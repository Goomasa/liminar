set(exe_file
    main.cpp
    renderer/material.cpp
    renderer/integrator.cpp
    renderer/renderer.cpp
    scene/scene.cpp
    scene/bvh.cpp
    scene/primitive.cpp
    config/lexer.cpp
    config/parser.cpp
)

find_package(OpenMP REQUIRED)

include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.14
)
FetchContent_MakeAvailable(SDL3)

add_executable(liminar ${exe_file})

target_include_directories(liminar PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/externals
)

target_link_libraries(liminar PRIVATE OpenMP::OpenMP_CXX)
target_link_libraries(liminar PRIVATE SDL3::SDL3)
