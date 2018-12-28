project(AICommon)

add_subdirectory("Utils")

#add_library(AICommon SHARED Utils/src/TestS.cpp)

#target_include_directories(AICommon PUBLIC "${AICommon_SOURCE_DIR}/Utils/include")

LIST(APPEND PATHS "${PROJECT_SOURCE_DIR}/Utils/include")

#asdk_install_multiple("${PATHS}")

