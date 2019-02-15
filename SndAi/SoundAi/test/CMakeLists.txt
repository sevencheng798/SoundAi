cmake_minimum_required(VERSION 2.8)

set(SoundAiEngineTest_SOURCES)
list(APPEND SoundAiEngineTest_SOURCES SoundAiEngineTest.cpp)

add_executable(SoundAiEngineTest ${SoundAiEngineTest_SOURCES})
target_include_directories(SoundAiEngineTest PUBLIC
		"${SoundAi_SOURCE_DIR}/include"
		"${NLP_SOURCE_DIR}/include"
		"${AudioTrackManager_SOURCE_DIR}/include"
		"${CMAKE_SOURCE_DIR}/AICommon/Utils/include")

target_link_libraries(SoundAiEngineTest
		SoundAi
		NLP
		AudioTrackManager
		asound
		z)

#		bz2
#		mp3lame
#		vorbis
#		vorbisenc
#		fontconfig
#		freetype)

install(TARGETS SoundAiEngineTest
      RUNTIME DESTINATION bin
      BUNDLE  DESTINATION bin
      LIBRARY DESTINATION lib)
