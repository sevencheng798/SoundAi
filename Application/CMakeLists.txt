#
# Creator by Sven
#
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(SampleApp LANGUAGES CXX)

include(../build/BuildDefaults.cmake)

if (SAI_SDK_LIBRARY)
    add_definitions(-DENABLE_HTTP2)
    link_directories("${SoundAi_SOURCE_DIR}/lib")	
endif()

add_subdirectory("src")
