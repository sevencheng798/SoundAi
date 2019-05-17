# Creater By Sven
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

if(SOUNDAI_ASR)
LIST(APPEND 3RD_LIBS "${ThirdLibrary_SOURCE_DIR}/SoundAi/lib/libsai_sdk.so")
endif()

if(SOUNDAI_KEY_WORD_DETECTOR)
LIST(APPEND 3RD_LIBS "${ThirdLibrary_SOURCE_DIR}/SoundAi/lib/libopen_denoise.so")
endif()
install(PROGRAMS  ${3RD_LIBS} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
