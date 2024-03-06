cmake_minimum_required(VERSION 3.25)
project(FastVoxels)


file (GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/Source/FastVoxels/*.cpp")

add_executable(FastVoxels ${SOURCES})

target_include_directories(FastVoxels PUBLIC "${PROJECT_SOURCE_DIR}/Include")

# Add custom command to copy Scenes folder to build directory
add_custom_command(TARGET FastVoxels POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Scenes
    $<TARGET_FILE_DIR:FastVoxels>/Scenes
)
