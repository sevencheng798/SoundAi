#
# Creator by Sven
#
add_library(AlarmsPlayer SHARED
        AlarmsPlayer.cpp)

target_include_directories(AlarmsPlayer PUBLIC
        "${AlarmsPlayer_SOURCE_DIR}/include"
 	"${SQLITE3_INCLUDE_DIR}")

target_link_libraries(AlarmsPlayer AICommon AudioMediaPlayer NLP)

# install target
asdk_install()

