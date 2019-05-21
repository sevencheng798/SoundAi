# Creater By Sven
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(ThirdLibrary LANGUAGES CXX)

if (ENABLE_SOUNDAI_ASR OR SOUNDAI_KEY_WORD_DETECTOR)
	add_subdirectory(SoundAi)
endif()

if (ENABLE_IFLYTEK_AIUI_ASR)
	add_subdirectory(Iflytek)
endif()
