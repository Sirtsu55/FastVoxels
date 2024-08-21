cmake_minimum_required(VERSION 3.25)
project(VoxelUtils)


# Global binary directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


set(CMAKE_CXX_STANDARD 20)

# Options
set(DXRAY_USE_AGILITY_SDK ON)
set(OPTICK_USE_D3D12 ON)

# Include all the dependencies
add_subdirectory(Deps/glfw)
add_subdirectory(Deps/glm)
add_subdirectory(Deps/DXRay)
add_subdirectory(Deps/tomlplusplus)

# Utils for the project
file (GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/Source/*.cpp")

add_executable(VoxelApp ${SOURCES})

target_include_directories(VoxelApp PUBLIC 
    "${PROJECT_SOURCE_DIR}/Source"
)

target_link_libraries(VoxelApp glfw glm DXRay d3d12 dxcompiler dxgi tomlplusplus::tomlplusplus)

target_precompile_headers(VoxelApp PRIVATE "${PROJECT_SOURCE_DIR}/Source/Common.h")

add_custom_command(
    TARGET VoxelApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders $<TARGET_FILE_DIR:VoxelApp>/Shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Data $<TARGET_FILE_DIR:VoxelApp>/Data
)

