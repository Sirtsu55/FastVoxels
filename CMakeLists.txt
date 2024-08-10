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

# Utils for the project
file (GLOB SOURCES "${PROJECT_SOURCE_DIR}/Source/*.cpp")

add_library(VoxelUtils ${SOURCES})

target_include_directories(VoxelUtils PUBLIC 
    "${PROJECT_SOURCE_DIR}/Source"
)

target_link_libraries(VoxelUtils glfw glm DXRay d3d12 dxcompiler dxgi)

target_precompile_headers(VoxelUtils PRIVATE "${PROJECT_SOURCE_DIR}/Source/Common.h")

add_custom_command(
    TARGET VoxelUtils POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders $<TARGET_FILE_DIR:VoxelUtils>/Shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Assets $<TARGET_FILE_DIR:VoxelUtils>/Assets
    COMMENT "Copying shaders for ${tgt}")


# Axis Aligned Intersection
add_executable(AxisAlignedIntersection "${PROJECT_SOURCE_DIR}/Source/AxisAlignedIntersection/AxisAlignedIntersection.cpp")
target_link_libraries(AxisAlignedIntersection VoxelUtils)

# Distance Field Intersection
add_executable(DistanceFieldIntersection "${PROJECT_SOURCE_DIR}/Source/DistanceFieldIntersection/DistanceFieldIntersection.cpp")
target_link_libraries(DistanceFieldIntersection VoxelUtils)