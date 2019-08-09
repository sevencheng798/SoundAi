# Creater By Sven
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(ASR LANGUAGES CXX)

add_subdirectory("src")

if(GTEST_ENABLE)
#add_subdirectory("AIUI/test")
endif()
