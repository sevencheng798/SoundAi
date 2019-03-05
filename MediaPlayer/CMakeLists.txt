#
# Creater By Sven
#
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

if (PORTAUDIO)
	add_subdirectory("PortAudioMediaPlayer")
else()
	add_subdirectory("AudioMediaPlayer")
endif()
	
