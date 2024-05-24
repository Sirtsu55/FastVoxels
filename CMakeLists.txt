cmake_minimum_required(VERSION 3.25)
project(FastVoxels)


# Global binary directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Options
set(DXRAY_USE_AGILITY_SDK ON)
set(OPTICK_USE_D3D12 ON)

# Include all the dependencies
add_subdirectory(Deps/glfw)
add_subdirectory(Deps/glm)
add_subdirectory(Deps/DXRay)


file (GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/Source/*.cpp")

add_executable(FastVoxels ${SOURCES})

set_property(TARGET FastVoxels PROPERTY CXX_STANDARD 20)

target_include_directories(FastVoxels PUBLIC 
    "${PROJECT_SOURCE_DIR}/Source"
)

target_link_libraries(FastVoxels glfw glm DXRay d3d12 dxcompiler dxgi)

target_precompile_headers(FastVoxels PRIVATE "${PROJECT_SOURCE_DIR}/Source/Base/Common.h")

add_custom_command(
    TARGET FastVoxels POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders $<TARGET_FILE_DIR:FastVoxels>/Shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Assets $<TARGET_FILE_DIR:FastVoxels>/Assets
    COMMENT "Copying shaders for ${tgt}")
