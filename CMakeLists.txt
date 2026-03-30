cmake_minimum_required(VERSION 3.10)

project(UnheardEngine)

add_executable(uhe_linux
    UnheardEngine.cpp
	Runtime/Application.cpp
	Runtime/Platform/PlatformLinux.cpp
)