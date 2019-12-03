##
# Create By Sven
#

if (IFLYTEK_KEY_WORD_DETECTOR)
aux_source_directory(${KWD_SOURCE_DIR}/IflyTek/src/ Keyword_SOURCES)
endif ()

if (SOUNDAI_KEY_WORD_DETECTOR)
# aux_source_directory(${KWD_SOURCE_DIR}/SoundAi/src/ Keyword_SOURCES)
set(Keyword_SOURCES ${KWD_SOURCE_DIR}/SoundAi/src/SoundAiKeywordDetector.cpp)
if (GM_AEC)
list(APPEND Keyword_SOURCES ${KWD_SOURCE_DIR}/SoundAi/src/EchoCancellationImplementation.cpp)
link_directories("${AEC_LIB_PATH}")
endif ()
endif ()

add_library(KWD SHARED
    GenericKeywordDetector.cpp
    KeywordDetectorRegister.cpp
	${Keyword_SOURCES})

include_directories(KWD 
	"${KWD_SOURCE_DIR}/include")

if (IFLYTEK_KEY_WORD_DETECTOR)
include_directories(KWD 
	"${KWD_SOURCE_DIR}/IflyTek/include"
	"${IFLYTEK_KEY_WORD_DETECTOR_INCLUDE_DIR}")
endif ()

if(SOUNDAI_KEY_WORD_DETECTOR)
include_directories(KWD 
	"${KWD_SOURCE_DIR}/SoundAi/include"
	"${SOUNDAI_KEY_WORD_DETECTOR_INCLUDE_DIR}")
if (GM_AEC)
message("---> include : ${AEC_INCLUDE_DIR}, lib: ${AEC_LIB_PATH}")
include_directories(KWD "${AEC_INCLUDE_DIR}")
target_link_libraries(KWD ${AEC_LIB_PATH}/libaec.a)
endif ()
endif()

target_link_libraries(KWD AICommon)

# install target
asdk_install()
