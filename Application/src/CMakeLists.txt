cmake_minimum_required(VERSION 2.8)

set(SampleApp_SOURCES)
list(APPEND SampleApp_SOURCES 
	Main.cpp SampleApp.cpp UIManager.cpp AIClient.cpp)

add_definitions(-DENABLE_HTTP2)
link_directories("${SoundAi_SOURCE_DIR}/lib")
link_directories("/home/sven/work/3rd/_install/libbz2/lib")

add_executable(SampleApp ${SampleApp_SOURCES})
target_include_directories(SampleApp PUBLIC
		"${SampleApp_SOURCE_DIR}/include"
		"${AudioTrackManager_SOURCE_DIR}/include")

target_link_libraries(SampleApp
		AICommon
		SoundAi
		NLP
		AudioTrackManager
		PortAudioMediaPlayer
		SpeechSynthesizer
		asound
		z)

#		bz2
#		mp3lame
#		vorbis
#		vorbisenc
#		fontconfig
#		freetype)

install(TARGETS SampleApp
      RUNTIME DESTINATION bin
      BUNDLE  DESTINATION bin
      LIBRARY DESTINATION lib)
