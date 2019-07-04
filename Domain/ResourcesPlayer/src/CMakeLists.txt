#
# Creator by Sven
#
add_library(ResourcesPlayer SHARED
	ResourcesPlayer.cpp http.cpp md5.cpp Md5Compute.cpp cJSON.cpp)

target_include_directories(ResourcesPlayer PUBLIC
        "${ResourcesPlayer_SOURCE_DIR}/include")

target_link_libraries(ResourcesPlayer AICommon AudioMediaPlayer NLP)

# install target
asdk_install()

