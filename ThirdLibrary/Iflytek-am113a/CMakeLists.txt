# Creater By Sven
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

LIST(APPEND 3RD_LIBS "${ThirdLibrary_SOURCE_DIR}/Iflytek/lib/libaiui.so")
LIST(APPEND AIUI_CONFIG "${ThirdLibrary_SOURCE_DIR}/Iflytek/AIUI")
LIST(APPEND AIUI_CONFIG "${ThirdLibrary_SOURCE_DIR}/Iflytek/msc")
install(DIRECTORY  ${AIUI_CONFIG} DESTINATION ${CMAKE_INSTALL_PREFIX}/../cfg)
install(PROGRAMS  ${3RD_LIBS} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
