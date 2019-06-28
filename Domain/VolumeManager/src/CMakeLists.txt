#
# Creator by Sven
#
add_library(VolumeManager SHARED
        VolumeManager.cpp)

target_include_directories(VolumeManager PUBLIC
        "${VolumeManager_SOURCE_DIR}/include")

target_link_libraries(VolumeManager AICommon NLP)

# install target
asdk_install()

