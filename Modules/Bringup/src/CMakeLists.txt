#
# Creator by Sven
#
add_library(Bringup SHARED
        Bringup.cpp)

target_include_directories(Bringup PUBLIC
        "${Bringup_SOURCE_DIR}/include"
		"${ASR_SOURCE_DIR}/include")

target_link_libraries(Bringup AICommon AudioMediaPlayer AudioTrackManager ASR)

# install target
asdk_install()

