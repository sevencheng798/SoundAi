#
# Creator by Sven
#
add_library(ResourcesPlayer SHARED
        ResourcesPlayer.cpp)

target_include_directories(ResourcesPlayer PUBLIC
        "${ResourcesPlayer_SOURCE_DIR}/include")

target_link_libraries(ResourcesPlayer AICommon AudioMediaPlayer NLP)

# install target
asdk_install()

