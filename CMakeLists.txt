cmake_minimum_required(VERSION 3.10)

project(UnheardEngine)

# source code setup
add_executable(uhe_linux
    UnheardEngine.cpp
	Runtime/Application.cpp
	Runtime/Platform/PlatformLinux.cpp
)

# GLFW lib
find_package(glfw3 REQUIRED)
target_link_libraries(uhe_linux PRIVATE glfw)

# headers
target_include_directories(uhe_linux PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)