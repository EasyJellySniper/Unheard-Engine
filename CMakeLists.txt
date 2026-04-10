cmake_minimum_required(VERSION 3.10)

project(UnheardEngine)

# source code setup
file(GLOB_RECURSE UHE_SOURCES
    ${CMAKE_SOURCE_DIR}/Runtime/*.cpp
    ${CMAKE_SOURCE_DIR}/Game/*.cpp
)

add_executable(UnheardEngine_Linux
	UnheardEngine.cpp
    ${UHE_SOURCES}
)

set_target_properties(UnheardEngine_Linux PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# GLFW lib
find_package(glfw3 REQUIRED)
target_link_libraries(UnheardEngine_Linux PRIVATE glfw)

# Vulkan lib
find_package(Vulkan REQUIRED)
target_link_libraries(UnheardEngine_Linux PRIVATE Vulkan::Vulkan)
set(CMAKE_PREFIX_PATH $ENV{VULKAN_SDK})

# headers
target_include_directories(UnheardEngine_Linux PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty
)