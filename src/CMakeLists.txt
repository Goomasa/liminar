set(exe_file
    main.cpp
    renderer/material.cpp
    renderer/renderer.cpp
    integrator/pathtracing.cpp
    integrator/bidirection.cpp
    integrator/volbpt.cpp
    scene/scene.cpp
    scene/bvh.cpp
    scene/primitive.cpp
    scene/lobby.cpp
    config/lexer.cpp
    config/parser.cpp
    util/image.cpp
    util/tg3helper.cpp
)

find_package(OpenMP REQUIRED)

add_executable(liminar ${exe_file})

target_include_directories(liminar PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/externals
)

target_link_libraries(liminar PRIVATE OpenMP::OpenMP_CXX)
target_link_libraries(liminar PRIVATE tinygltf_v3)
