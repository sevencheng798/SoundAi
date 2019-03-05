# QuickStart for compilation SDK

## Compiler introduction:<br>

If you are the first compiler SDK you need to install some thrid-party dependency libraies such as: libcurl-7.56/openssl-1.1-0h/ffmpeg-4.0 and libao.
Then endit and correctly set the compiler environment in the am113a-toolchain.cmake. so that the next time you recompile the same types of products you won't need to set it up again.

Once the setup is complete, you can execute the following commands for compilation.

## step1, To create a compiler directory

	mkdir ${HOME}/build_dir

## step2, To clone the Soundai sdk to ${HOME}

	git clone ssh://git@119.3.60.230:1622/hardware/Soundai.git

## setp3, To execute the cmake command

	cmake ../Soundai -DCMAKE_TOOLCHAIN_FILE=../Soundai/am113a-toolchain.cmake \
			-DFFMPEG_LIB_PATH="/home/sven/work/3rd/_install/ffmpeg-4.0-am113/lib" \
			-DFFMPEG_INCLUDE_DIR="/home/sven/work/3rd/_install/ffmpeg-4.0-am113/include" \
			-DPORTAUDIO=ON -DPORTAUDIO_LIB_PATH="/home/sven/work/3rd/_install/portaudio-v19-a113/lib" \
			-DPORTAUDIO_INCLUDE_DIR="/home/sven/work/3rd/_install/portaudio-v19-a113/include" \
			-DOPENSSL=ON -DOPENSSL_INCLUDE_DIR=/home/sven/work/3rd/_install/openssl-1.1h-am113/include \
			-DOPENSSL_LIB_PATH="/home/sven/work/3rd/_install/openssl-1.1h-am113/lib" \
			-DCMAKE_INSTALL_PREFIX=./_install \
			-DCMAKE_BUILD_TYPE=DEBUG

## step4, make && make install
	

