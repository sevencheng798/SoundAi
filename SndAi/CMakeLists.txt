# Creater By Sven
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
option(SAI_SDK_LIBRARY "Enable SAI SDK Library. defaule be set on" ON)

if (SAI_SDK_LIBRARY)
	# Define ENABLE_HTTP2 to enable sai sdk network feature
	link_directories("${CMAKE_CURRENT_SOURCE_DIR}/SoundAi/lib")
	add_definitions(-DENABLE_HTTP2)
	add_subdirectory("SoundAi")
else()
	message("No SoundAi will be built, you must set SAI_SDI library")
endif()
