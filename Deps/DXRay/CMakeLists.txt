
# Configure the project
project("DXRay")

# Options for DXRay
option(DXRAY_USE_AGILITY_SDK "Use the Agility SDK for GPU Upload Heap feature" ON)

# Fetch the DirectX Agility SDK 
set(AGILITY_SDK_URL "https://globalcdn.nuget.org/packages/microsoft.direct3d.d3d12.1.711.3-preview.nupkg")
set(AGILITY_SDK_DIR "${PROJECT_SOURCE_DIR}/Deps/AgilitySDK")

# Download the Agility SDK if it doesn't exist, If user wants to replace the sdk, then they can extract the zip file manually
# to the AgilitySDK directory
if(NOT EXISTS "${AGILITY_SDK_DIR}" AND DXRAY_USE_AGILITY_SDK)
	file(DOWNLOAD ${AGILITY_SDK_URL} "${PROJECT_SOURCE_DIR}/Deps/AgilitySDK.zip" SHOW_PROGRESS)
	file(MAKE_DIRECTORY ${AGILITY_SDK_DIR})
	file(ARCHIVE_EXTRACT INPUT "${PROJECT_SOURCE_DIR}/Deps/AgilitySDK.zip" DESTINATION "${PROJECT_SOURCE_DIR}/Deps/AgilitySDK")
	file(REMOVE "${PROJECT_SOURCE_DIR}/Deps/AgilitySDK.zip")
else()
	message(STATUS "DXRay: Agility SDK not downloaded. If you want to use the Agility SDK, then set DXRAY_USE_AGILITY_SDK to ON")
endif()

# Set D3D12MA Agility SDK Variables. For some reason, you have to run CMake twice to get the variables to work
if( DXRAY_USE_AGILITY_SDK )
	set(D3D12MA_AGILITY_SDK_DIRECTORY  ${AGILITY_SDK_DIR})
	set(D3D12MA_AGILITY_SDK_PREVIEW ON CACHE BOOL "Use Agility SDK Preview")
	set(D3D12MA_OPTIONS16_SUPPORTED ON CACHE BOOL "Use Agility SDK for GPU Upload Heaps")
endif()

# Add Dependencies
add_subdirectory("${PROJECT_SOURCE_DIR}/Deps/D3D12MemoryAllocator")
add_subdirectory("${PROJECT_SOURCE_DIR}/Deps/DirectX-Headers")

# Add the Library
file(GLOB_RECURSE DXRaySOURCES "${PROJECT_SOURCE_DIR}/Source/*.cpp")

# Create the Library
add_library(DXRay STATIC ${DXRaySOURCES})

# Link the Libraries
target_link_libraries(DXRay PUBLIC D3D12MemoryAllocator DirectX-Headers)

# Set C++ Standard
set_target_properties(DXRay PROPERTIES CXX_STANDARD 20)

# Precompiled Headers
target_precompile_headers(DXRay PRIVATE "${PROJECT_SOURCE_DIR}/Include/DXRay/DXRay.h")

# Set Compile Definitions
if( DXRAY_USE_AGILITY_SDK )
	target_compile_definitions(DXRay PUBLIC DXRAY_AGILITY_SDK_VERSION=711 DXRAY_AGILITY_SDK_PATH="AgilitySDK\\\\")
endif()

# Add the Include Directory
target_include_directories(DXRay PUBLIC 
	"${PROJECT_SOURCE_DIR}/Include"
	"${PROJECT_SOURCE_DIR}/Deps/DirectX-Headers/include"
	"${PROJECT_SOURCE_DIR}/Deps/D3D12MemoryAllocator/include"
)

# If Agility SDK doesn't exist in binary directory, then copy the binaries from source directory
if(NOT EXISTS "${CMAKE_BINARY_DIR}/Deps/AgilitySDK" AND DXRAY_USE_AGILITY_SDK)
	add_custom_command(TARGET DXRay POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${PROJECT_SOURCE_DIR}/Deps/AgilitySDK/build/native/bin/x64" 
	"${CMAKE_BINARY_DIR}/AgilitySDK")
endif()