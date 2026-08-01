set(exe_file
    main.cpp
    renderer/material.cpp
    renderer/integrator.cpp
    scene/scene.cpp
    scene/bvh.cpp
    config/lexer.cpp
    config/parser.cpp
)

add_executable(liminar ${exe_file})

target_include_directories(liminar PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/externals
)
