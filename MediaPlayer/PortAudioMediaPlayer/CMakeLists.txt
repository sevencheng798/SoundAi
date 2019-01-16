# Creater By Sven
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(PortAudioMediaPlayer LANGUAGES CXX)

include(../../build/BuildDefaults.cmake)

# This is a temporary settings for am113-board
link_directories("/home/sven/work/3rd/_install/libbz2/lib")

add_subdirectory("src")
add_subdirectory("test")

