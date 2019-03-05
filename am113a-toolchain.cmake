# Creater @Sven
#
#设置编译器
set(CMAKE_C_COMPILER /opt/am113a/linaro-6.3.1-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER /opt/am113a/linaro-6.3.1-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++)

#设置cmake查找库、头文件的根目录
set(CMAKE_FIND_ROOT_PATH /opt/am113a/linaro-6.3.1-arm-linux-gnueabihf/arm-linux-gnueabihf/lib
	/opt/am113a/linaro-6.3.1-arm-linux-gnueabihf/arm-linux-gnueabihf/include
	/opt/am113a/linaro-6.3.1-arm-linux-gnueabihf/arm-linux-gnueabihf/lib
	/opt/am113a/linaro-6.3.1-arm-linux-gnueabihf/arm-linux-gnueabihf/include
	/home/sven/work/3rd/_install/libbz2/lib
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_NO_INSTALL_PREFIX TRUE)

#添加新的目标：uninstall, 用于执行 make uninstall
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/makefile "include Makefile\nuninstall:\n\txargs rm -rfv < install_manifest.txt")

#设置cmake查找库文件与头文件的路径前缀
list(INSERT  CMAKE_PREFIX_PATH 0 /opt/am113a/linaro-6.3.1-arm-linux-gnueabihf/arm-linux-gnueabihf/include
					/opt/am113a/linaro-6.3.1-arm-linux-gnueabihf/arm-linux-gnueabihf/include
					/home/sven/work/3rd/_install/libbz2/lib
)

#设置编译器查找头文件的路径
include_directories(BEFORE SYSTEM
			 /opt/am113a/linaro-6.3.1-arm-linux-gnueabihf/arm-linux-gnueabihf/include
			 /opt/am113a/linaro-6.3.1-arm-linux-gnueabihf/include
)

