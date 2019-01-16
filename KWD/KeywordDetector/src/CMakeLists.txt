# Creater By Sven
add_library(KeywordDetector SHARED
			KeywordDetector.cpp
			)

target_include_directories(KeywordDetector PUBLIC
        "${KeywordDetector_SOURCE_DIR}/include"
		"${SoundAi_SOURCE_DIR}/include"
		"${CMAKE_SOURCE_DIR}/AICommon/Utils/include/")

#target_link_libraries(KeywordDetector
#		AICommon)

# Install sai sdk library
#file(GLOB SAI_SDK_LIBRARIES ${SoundAi_SOURCE_DIR}/lib/*)
#install(PROGRAMS ${SAI_SDK_LIBRARIES} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
	
asdk_install()

