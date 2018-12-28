/*
 * Copyright 2018 gm its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <memory>
#include <iostream>
#include <fstream>

#include <dirent.h>
#include <unistd.h>
#include <cstdio>
#include <termios.h>
#include <sys/types.h>
#include <sys/time.h>

#include "Utils/MediaPlayer/MediaPlayerObserverInterface.h"
#include "PortAudioMediaPlayer/PaWrapper.h"

using namespace mediaPlayer::ffmpeg;

namespace util {
int kbhit();
}

class AudioPlayerTest : public utils::mediaPlayer::MediaPlayerObserverInterface
		, public std::enable_shared_from_this<AudioPlayerTest>{
public:
	/// Constructor
	AudioPlayerTest(std::unique_ptr<PaWrapper> paWrapper, PaWrapper::SourceId id);

	void init(std::shared_ptr<std::istream> stream);

	bool run();
	
	/// @name MediaPlayerObserverInterface functions
	/// @{
	void onPlaybackStarted(SourceId id) override;

    void onPlaybackFinished(SourceId id) override;

    void onPlaybackError(SourceId id, const utils::mediaPlayer::ErrorType& type, std::string error) override;

    void onPlaybackStopped(SourceId id) override;

	void onPlaybackPaused(SourceId id) override;

	void onPlaybackResumed(SourceId id) override;

	///}
	
private:
	std::unique_ptr<PaWrapper> m_paWrapper;

	PaWrapper::SourceId m_sourceID;

	std::shared_ptr<std::istream> m_stream;
};

AudioPlayerTest::AudioPlayerTest(std::unique_ptr<PaWrapper> paWrapper, PaWrapper::SourceId id)
	:m_sourceID{0}, m_paWrapper{std::move(paWrapper)}{

}

void AudioPlayerTest::init(std::shared_ptr<std::istream> stream){
std::cout << "set init" << std::endl;
	m_paWrapper->setObserver(shared_from_this());
	m_stream = stream;
}

bool AudioPlayerTest::run(){
	std::cout << "set run" << std::endl;
	m_sourceID = m_paWrapper->setSource(m_stream, true);
	if(utils::mediaPlayer::MediaPlayerInterface::ERROR == m_sourceID){
		std::cout << "setSourceFailed" << std::endl;
		return false;
	}

	std::cout << "Playback with portaudio" << std::endl << std::endl;

	std::cout << "Options: " << std::endl;
	std::cout << " p: play sound" << std::endl;
	std::cout << " r: resume sound" << std::endl;
	std::cout << " s: stop sound" << std::endl;
	std::cout << " z: pause sounds" << std::endl << std::endl;

	std::cout << "Press 'Q' to quit" << std::endl;

	int ch; 			
	bool done = false;
	while (!done)
	{
		if(!util::kbhit()) {
			ch = getchar();
			switch (ch) {
			case 'q':
			case 'Q':
					done = true;
					break;
			case 'p':
			case 'P':
					m_paWrapper->play(m_sourceID);
					break;
			case 'r':
			case 'R':
					m_paWrapper->resume(m_sourceID);
					break;
			case 's':
			case 'S':
					m_paWrapper->stop(m_sourceID);
					break;
			case 'z':
			case 'Z':
					m_paWrapper->pause(m_sourceID);
					break;
			default:
					break;
			}
		}
	}
	
	m_paWrapper.reset();
	
	if(!m_paWrapper){
		std::cout << "pa be destory" << std::endl;
	}
	
	return true;
}

void AudioPlayerTest::onPlaybackStarted(SourceId id){
	if(id == m_sourceID){
		std::cout << " Start..." << std::endl;
	}else
		std::cout << " source isnot match currentID: " << m_sourceID << "newId: " << id << std::endl;
}

void AudioPlayerTest::onPlaybackFinished(SourceId id){
	if(id == m_sourceID){
		std::cout << " Finished ..." << std::endl;
	}else
		std::cout << " source isnot match currentID: " << m_sourceID << "newId: " << id << std::endl;

}	

void AudioPlayerTest::onPlaybackError(SourceId id, const utils::mediaPlayer::ErrorType& type, std::string error){
	if(id == m_sourceID){
		std::cout << " Error ..." << std::endl;
		auto errStr = utils::mediaPlayer::errorTypeToString(type);
		std::cout << errStr << std::endl;
	}else
		std::cout << " source isnot match currentID: " << m_sourceID << "newId: " << id << std::endl;

}

void AudioPlayerTest::onPlaybackStopped(SourceId id){
	if(id == m_sourceID){
		std::cout << " stopped ..." << std::endl;
	}else
		std::cout << " source isnot match currentID: " << m_sourceID << "newId: " << id << std::endl;

}

void AudioPlayerTest::onPlaybackPaused(SourceId id){
	if(id == m_sourceID){
		std::cout << " paused ..." << std::endl;
	}else
		std::cout << " source isnot match currentID: " << m_sourceID << "newId: " << id << std::endl;

}

void AudioPlayerTest::onPlaybackResumed(SourceId id){
	if(id == m_sourceID){
		std::cout << " resumed ..." << std::endl;
	}else
		std::cout << " source isnot match currentID: " << m_sourceID << "newId: " << id << std::endl;

}

int util::kbhit()
{
        struct timeval tv;
        fd_set rdfs;

        tv.tv_sec = 0;
        tv.tv_usec = 0;

        FD_ZERO(&rdfs);
        FD_SET(STDIN_FILENO, &rdfs);

        select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);

        return FD_ISSET(STDIN_FILENO, &rdfs);
}


int main(int argc, char *argv[]){
	std::shared_ptr<std::ifstream> input;
	
	input = std::make_shared<std::ifstream>();
	if(argc >= 2){

		input->open(argv[1], std::ifstream::in);
	}else
	input->open("/mnt/hgfs/share/test.m4", std::ifstream::in);
	if(!input->is_open()){
		std::cout << "Open the file is failed\n" << std::endl;
		return -1;
	}

	std::cout << "Start create" << std::endl;
	
	/// Create PaWrapper object
	auto pa = PaWrapper::create();
	if(!pa){
		std::cout << "Creat pa failed\n" << std::endl;
		input->close();
		return -1;
	}
std::cout << "Start create ok" << std::endl;
	auto audioPlayer = std::make_shared<AudioPlayerTest>(std::move(pa), 0);

	audioPlayer->init(input);
	
	audioPlayer->run();
	
	std::cout << "play finished" << std::endl;
	return 0;
}
