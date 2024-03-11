cmake_minimum_required(VERSION 3.25)
project(FastVoxels)


find_package(Vulkan REQUIRED COMPONENTS dxc)

if(WIN32) # on Windows, we need to copy the vulkan dlls to the output directory, but on Linux, we don't need to do that, because its in the system path
	set(DXC_DLL $ENV{VULKAN_SDK}/Bin/dxcompiler.dll)
endif()

set(VULRAY_BUILD_DENOISERS OFF)
set(GLFW_BUILD_WAYLAND OFF) # Make sure it doesn't build wayland, because half of the screen is black in wayland, same code works fine in X11 and Windows

# Include all the dependencies
add_subdirectory(Deps/glfw)
add_subdirectory(Deps/glm)
add_subdirectory(Deps/tinygltf)
add_subdirectory(Deps/Vulray)


file (GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/Source/*.cpp")

add_executable(FastVoxels ${SOURCES})

set_property(TARGET FastVoxels PROPERTY CXX_STANDARD 20)

target_include_directories(FastVoxels PUBLIC 
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Deps/libnpy/include"
    "${PROJECT_SOURCE_DIR}/Deps/tinygltf"
    "${PROJECT_SOURCE_DIR}/Deps/voxelizer"
    )


# Add custom command to copy Scenes folder to build directory
add_custom_command(TARGET FastVoxels POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Scenes
    $<TARGET_FILE_DIR:FastVoxels>/Scenes
)

target_link_libraries(FastVoxels glfw glm Vulray Vulkan::dxc_lib)

target_precompile_headers(FastVoxels PRIVATE "${PROJECT_SOURCE_DIR}/Source/Base/Common.h")

add_custom_command(
    TARGET FastVoxels POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders $<TARGET_FILE_DIR:FastVoxels>/Shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Assets $<TARGET_FILE_DIR:FastVoxels>/Assets
    COMMENT "Copying shaders for ${tgt}")
if(WIN32)
   add_custom_command(
       TARGET FastVoxels POST_BUILD
       COMMAND ${CMAKE_COMMAND} -E copy ${DXC_DLL} $<TARGET_FILE_DIR:FastVoxels>
       COMMAND ${CMAKE_COMMAND} -E echo "Copying DXC dll")
endif()