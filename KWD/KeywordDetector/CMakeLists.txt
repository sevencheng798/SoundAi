# Creater By Sven
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(KeywordDetector LANGUAGES CXX)

include(../../build/BuildDefaults.cmake)

# Set find link directory path for sai sdk library
#link_directories("${KeywordDetector_SOURCE_DIR}/lib")

add_subdirectory("src")
#add_subdirectory("test")
