cmake_minimum_required(VERSION 2.8)

set(SampleApp_SOURCES)
list(APPEND SampleApp_SOURCES 
	Main.cpp SampleApp.cpp UIManager.cpp AIClient.cpp)

link_directories(${SQLITE3_LIB_PATH})

add_executable(SampleApp ${SampleApp_SOURCES})
target_include_directories(SampleApp PUBLIC
		"${SampleApp_SOURCE_DIR}/include"
		"${AudioTrackManager_SOURCE_DIR}/include")

target_link_libraries(SampleApp
		AICommon
		AudioTrackManager
		AudioMediaPlayer
		SoundAi
		NLP
		SpeechSynthesizer
		ResourcesPlayer
		AlarmsPlayer
		ao
		asound
		sqlite3
		z)

install(TARGETS SampleApp
      RUNTIME DESTINATION bin
      BUNDLE  DESTINATION bin
      LIBRARY DESTINATION lib)
