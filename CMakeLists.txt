cmake_minimum_required(VERSION 3.25)
project(FastVoxels)


# Include all the dependencies
add_subdirectory(Deps/glfw)
add_subdirectory(Deps/glm)
add_subdirectory(Deps/Vulray)


file (GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/Source/*.cpp")

add_executable(FastVoxels ${SOURCES})

target_include_directories(FastVoxels PUBLIC 
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Deps/libnpy/include")


# Add custom command to copy Scenes folder to build directory
add_custom_command(TARGET FastVoxels POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Scenes
    $<TARGET_FILE_DIR:FastVoxels>/Scenes
)

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