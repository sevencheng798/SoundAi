# Creater By Sven
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(SoundAi LANGUAGES CXX)

include(../../build/BuildDefaults.cmake)

# Define ENABLE_HTTP2 to enable sai sdk network feature
add_definitions(-DENABLE_HTTP2)

# Set find link directory path for sai sdk library
link_directories("${SoundAi_SOURCE_DIR}/lib")

add_subdirectory("src")
add_subdirectory("test")

