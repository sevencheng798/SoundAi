# QuickStart for compilation SDK

## Compiler introduction:<br>

If you are the first compiler SDK you need to install some thrid-party dependency libraies such as: openssl-1.1-0h/ffmpeg-4.0 and libao-1.2.0.
Then endit and correctly set the compiler environment in the am113a-toolchain.cmake. so that the next time you recompile the same types of BOARDS you won't need to set it up again.

Once the setup is complete, you can execute the following commands for compilation.

## step1, To create a compiler directory

	mkdir ${HOME}/build_dir

## step2, To clone the Soundai sdk to ${HOME}

	git clone ssh://git@119.3.60.230:1622/hardware/Soundai.git

## setp3, To execute the cmake command in the ${HOME}/build_dir directory.

	cmake ../Soundai -DCMAKE_TOOLCHAIN_FILE=../Soundai/am113a-toolchain.cmake \
		-DCMAKE_INSTALL_PREFIX=./_install \
		-DCMAKE_BUILD_TYPE=DEBUG \
		-DFFMPEG_LIB_PATH="${PWD}/../Soundai/ThirdLibrary/Open3rd/lib" \
		-DFFMPEG_INCLUDE_DIR="${PWD}/../Soundai/ThirdLibrary/Open3rd/include" \
		-DOPENSSL=ON \
		-DOPENSSL_INCLUDE_DIR="${PWD}/../Soundai/ThirdLibrary/Open3rd/include" \
		-DOPENSSL_LIB_PATH="${PWD}/../Soundai/ThirdLibrary/Open3rd/lib" \
		-DAO_INCLUDE_DIR="${PWD}/../Soundai/ThirdLibrary/Open3rd/include" \
		-DAO_LIB_PATH="${PWD}/../Soundai/ThirdLibrary/Open3rd/lib" \
		-DPORTAUDIO=ON \
		-DPORTAUDIO_LIB_PATH="${PWD}/../Soundai/ThirdLibrary/Open3rd/lib" \
		-DPORTAUDIO_INCLUDE_DIR="${PWD}/../Soundai/ThirdLibrary/Open3rd/include" \
		-DJSONCPP_LIB_PATH="${PWD}/../Soundai/ThirdLibrary/Open3rd/lib" \
		-DJSONCPP_INCLUDE_DIR="${PWD}/../Soundai/ThirdLibrary/Open3rd/include" \
		-DSQLITE3_INCLUDE_DIR="${PWD}/../Soundai/ThirdLibrary/Open3rd/include" \
		-DSQLITE3_LIB_PATH="${PWD}/../Soundai/ThirdLibrary/Open3rd/lib" \
		-DIFLYTEK_AIUI_ASR=ON \
		-DIFLYTEK_AIUI_ASR_LIB_PATH="${PWD}/../Soundai/ThirdLibrary/Iflytek/lib" \
		-DIFLYTEK_AIUI_ASR_INCLUDE_DIR="${PWD}/../Soundai/ThirdLibrary/Iflytek/include" \
		-DSOUNDAI_KEY_WORD_DETECTOR=on \
		-DSOUNDAI_KEY_WORD_DETECTOR_LIB_PATH="${PWD}/../Soundai/ThirdLibrary/SoundAi/lib" \
		-DSOUNDAI_KEY_WORD_DETECTOR_INCLUDE_DIR="${PWD}/../Soundai/ThirdLibrary/SoundAi/include"

## step4, Start compiling.
	make && make install
	
## step5, Test the AI sample.
	First before testing, you need to copy the relevant 3rd libraries to the target device if the devices are missing them. 
	These third-party libraries are placed in the Soundai/ThirdLibrary/Open3rd/lib directory.

	Then, you need to copy Soundai run library and execute binrary file to the target device.
	These library and execute are placed in the installation directory specified for compilation.
	such as `CMAKE_INSTALL_PREFIX=./_install`.

	when the above is ready, you can execute the following command to test:
	./SampleApp

