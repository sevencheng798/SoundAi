# Creater By Sven
add_library(PortAudioMediaPlayer SHARED
        PaBufferQueue.cpp
        PaWrapper.cpp
        PlaybackConfiguration.cpp)

target_include_directories(PortAudioMediaPlayer PUBLIC
        "${PortAudioMediaPlayer_SOURCE_DIR}/include"
		"${CMAKE_SOURCE_DIR}/AICommon/Utils/include/"
		"${PORTAUDIO_INCLUDE_DIR}"
        "${FFMPEG_INCLUDE_DIR}"
		"${OPENSSL_INCLUDE_DIR}")

target_link_libraries(PortAudioMediaPlayer
        portaudio
	# FFmpeg libraries
        avformat
        avcodec
	avdevice
	avfilter
        avutil
        swresample
	swscale
	ssl
	crypto)
	
asdk_install()

