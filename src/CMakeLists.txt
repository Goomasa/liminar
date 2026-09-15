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
