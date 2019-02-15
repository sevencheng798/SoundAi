#
# Creator by Sven
#
add_library(SpeechSynthesizer SHARED
        SpeechSynthesizer.cpp)

target_include_directories(SpeechSynthesizer PUBLIC
        "${SpeechSynthesizer_SOURCE_DIR}/include")

target_link_libraries(SpeechSynthesizer AICommon PortAudioMediaPlayer NLP)

# install target
asdk_install()

