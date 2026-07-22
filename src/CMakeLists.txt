add_executable(liminar main.cpp)

target_include_directories(liminar PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/externals
)
