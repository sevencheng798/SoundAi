# Creater @Sven
#
#设置编译器
set(CMAKE_C_COMPILER /opt/am113a/linaro-6.3.1-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER /opt/am113a/linaro-6.3.1-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++)

#设置cmake查找库、头文件的根目录
set(CMAKE_FIND_ROOT_PATH /opt/am113a/linaro-6.3.1-arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

include_directories(BEFORE SYSTEM
	${CMAKE_SOURCE_DIR}/ThirdLibrary/Open3rd/include
)

