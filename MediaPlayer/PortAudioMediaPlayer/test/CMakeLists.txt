cmake_minimum_required(VERSION 3.1)

set(PaWrapperTest_SOURCES)
list(APPEND PaWrapperTest_SOURCES PaWrapperTest.cpp)

add_executable(PaWrapperTest ${PaWrapperTest_SOURCES})
target_include_directories(PaWrapperTest PUBLIC
		"${PaWrapperTest_SOURCES_DIR}/include"
		"${CMAKE_SOURCE_DIR}/AICommon/Utils/include")

target_link_libraries(PaWrapperTest 
		PortAudioMediaPlayer)

install(TARGETS PaWrapperTest
      RUNTIME DESTINATION bin
      BUNDLE  DESTINATION bin
      LIBRARY DESTINATION lib)
