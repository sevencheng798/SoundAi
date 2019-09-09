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
#include <string>
#include <dirent.h>
#include <unistd.h>
#include <cstdio>

#include "Utils/Threading/Executor.h"
#include "Utils/Attachment/AttachmentReader.h"
#include "Utils/MediaPlayer/MediaPlayerObserverInterface.h"
#include "AudioMediaPlayer/AOWrapper.h"

using namespace aisdk::mediaPlayer::ffmpeg;

static const char *urlTank[] = {
	"http://fs.dg360.kugou.com/201909040951/01d6eebf555231b68634d4b6e2c87e36/G014/M07/1B/13/roYBAFUJsYOAXv4zAE1QgBqXVy4671.mp3",
	"http://fs.dg360.kugou.com/201909040952/8178a59324577cf3a85036c491d2f255/G117/M07/11/07/tQ0DAFot-mqAYrykADwdPNgvKjM064.mp3",
	"http://fs.dg360.kugou.com/201909040953/0a632eb9ec7c4ec6dde5d3f8ce0bd318/G085/M07/0B/10/lQ0DAFujV42AK4xpACkHR2d9qTo587.mp3",
	"http://yp.stormorai.cn/music/mp3/3337.mp3",
	"http://fs.dg360.kugou.com/201909040956/ede3abaf40fcaafc295cfe0c41cb252b/G013/M07/02/03/TQ0DAFUObtOAB-59ADSkhorPd8U162.mp3",
	"http://fs.dg360.kugou.com/201909040958/4f469f7d0a0aad3a4445c725ecaf0cfb/G001/M01/1F/15/QQ0DAFS4AdiAV0OoAEJUI7rcygo322.mp3",
	NULL
};

int flags = 0;
	
namespace util {
int kbhit();
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
class MockAttachmentReader : public aisdk::utils::attachment::AttachmentReader {
public:
	MockAttachmentReader(std::shared_ptr<std::ifstream> stream):m_stream{stream} {

	};

	/// @name AttachmentReader method;
	/// @{
	std::size_t read(
        void* buf,
        std::size_t numBytes,
        ReadStatus* readStatus,
        std::chrono::milliseconds timeoutMs = std::chrono::milliseconds(0)) override;
	bool seek(uint64_t offset) override { return true; };
	uint64_t getNumUnreadBytes() override { return 0; };
	void close(ClosePoint closePoint = ClosePoint::AFTER_DRAINING_CURRENT_BUFFER) override;
	/// @}
	~MockAttachmentReader() {
		std::cout << "~MockAttachmentReader destory" << std::endl;
		close();
	}
private:
	std::shared_ptr<std::ifstream> m_stream;
};

std::size_t MockAttachmentReader::read(
	void* buf,
	std::size_t numBytes,
	ReadStatus* readStatus,
	std::chrono::milliseconds timeoutMs) {
	(*readStatus) = aisdk::utils::attachment::AttachmentReader::ReadStatus::OK;
    m_stream->read(reinterpret_cast<char*>(buf), numBytes);
    auto bytesRead = m_stream->gcount();
    if (!bytesRead) {
		(*readStatus) = aisdk::utils::attachment::AttachmentReader::ReadStatus::CLOSED;
        if (m_stream->bad()) {
			std::cout << "readFailed::occrous." << std::endl;
            return 0;
        }
		std::cout << "readFailed::bytes: " << bytesRead << std::endl;
        return 0;
    }
	
    return bytesRead;
}

void MockAttachmentReader::close(ClosePoint closePoint) {
	m_stream.reset();
}


class AudioPlayerTest 
	: public aisdk::utils::mediaPlayer::MediaPlayerObserverInterface
	, public std::enable_shared_from_this<AudioPlayerTest> {
public:
	/// Create a object @c AudioPlayerTest
	static std::shared_ptr<AudioPlayerTest> createNew();

	/// Destructs.
	~AudioPlayerTest(){ 
		std::cout << "AudioPlayerTest destructor..." << std::endl;
		if(m_playWrapper) 
			m_playWrapper.reset();
	}

	/// Set Observer
	bool init();

	bool run(std::shared_ptr<aisdk::utils::attachment::AttachmentReader> attachmentReader);

	bool run(std::shared_ptr<std::istream> stream);

	bool run(std::string &url, std::chrono::milliseconds &offset);

	/// Master process tasks
	bool play();
	
	/// @name MediaPlayerObserverInterface functions
	/// @{
	void onPlaybackStarted(SourceId id) override;

    void onPlaybackFinished(SourceId id) override;

    void onPlaybackError(SourceId id, const aisdk::utils::mediaPlayer::ErrorType& type, std::string error) override;

    void onPlaybackStopped(SourceId id) override;

	void onPlaybackPaused(SourceId id) override;

	void onPlaybackResumed(SourceId id) override;

	///@}

	void executePlaybackFinished();
	
private:
	std::shared_ptr<AOWrapper> m_playWrapper;

	AOWrapper::SourceId m_sourceID;

	std::shared_ptr<std::istream> m_stream;

	int m_index;

	/// An internal thread pool which queues up operations from asynchronous API calls
	aisdk::utils::threading::Executor m_executor;
};

std::shared_ptr<AudioPlayerTest> AudioPlayerTest::createNew() {
	std::cout << "CreateNew entry" << std::endl;
	auto instance = std::shared_ptr<AudioPlayerTest>(new AudioPlayerTest);
	if(instance->init()) {
		std::cout << "created:reason=initializedSuccess" << std::endl;
		return instance;
	}

	return nullptr;
}

bool AudioPlayerTest::init(){
	/// Create object for the @c AOEngine.
	auto engine = AOEngine::create();
	if(!engine) {
		std::cout << "createFailed:reason=createEngineFailed" << std::endl;
		return false;
	}
	
	/// Create PaWrapper object
	m_playWrapper = AOWrapper::create(engine);
	if(!m_playWrapper){
		std::cout << "CreateFailed:reason=createWrapperFailed" << std::endl;
		return false;
	}
	
	m_playWrapper->setObserver(shared_from_this());

	// Url tank index.
	m_index = 0;
	
	return true;
}


bool AudioPlayerTest::run(std::shared_ptr<aisdk::utils::attachment::AttachmentReader> attachmentReader) {
#if 1
	aisdk::utils::AudioFormat format{.encoding = aisdk::utils::AudioFormat::Encoding::LPCM,
                       .endianness = aisdk::utils::AudioFormat::Endianness::LITTLE,
                       .sampleRateHz = 16000,
                       .sampleSizeInBits = 16,
                       .numChannels = 1,
                       .dataSigned = true};
	#endif
	//aisdk::utils::AudioFormat format;
	m_sourceID = m_playWrapper->setSource(attachmentReader, &format);
	if(aisdk::utils::mediaPlayer::MediaPlayerInterface::ERROR == m_sourceID){
		std::cout << "run:reason=setSourceStreamFailed" << std::endl;
		return false;
	}
	
	std::cout << "newSourceId= " << m_sourceID << std::endl;

	this->play();

	return true;

}

bool AudioPlayerTest::run(std::shared_ptr<std::istream> stream){
	
	m_sourceID = m_playWrapper->setSource(stream, true);
	if(aisdk::utils::mediaPlayer::MediaPlayerInterface::ERROR == m_sourceID){
		std::cout << "run:reason=setSourceStreamFailed" << std::endl;
		return false;
	}
	
	std::cout << "newSourceId= " << m_sourceID << std::endl;

	this->play();

	return true;
}

bool AudioPlayerTest::run(std::string &url, std::chrono::milliseconds &offset){
	m_sourceID = m_playWrapper->setSource(url, offset);
	if(aisdk::utils::mediaPlayer::MediaPlayerInterface::ERROR == m_sourceID){
		std::cout << "run:reason=setSourceUrlFailed" << std::endl;
		return false;
	}
	std::cout << "newSourceId= " << m_sourceID << std::endl;
	
	this->play();
	
	return true;
}

bool AudioPlayerTest::play(){

	std::cout << "Playback with libao:" << std::endl << std::endl;

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
					m_playWrapper->play(m_sourceID);
					break;
			case 'r':
			case 'R':
					m_playWrapper->resume(m_sourceID);
					break;
			case 's':
			case 'S':
					m_playWrapper->stop(m_sourceID);
					break;
			case 'z':
			case 'Z':
					m_playWrapper->pause(m_sourceID);
					break;
			default:
					break;
			}
		}
	}
	
	m_playWrapper->shutdown();

	if(!m_playWrapper){
		std::cout << "aoWrapperIsDestory" << std::endl;
	}
	
	return true;
}

void AudioPlayerTest::onPlaybackStarted(SourceId id){
	if(id == m_sourceID){
		std::cout << " PlaybackStarted:SourceId: " << m_sourceID << std::endl;
	}else
		std::cout << " PlaybackFinished:SourceIsNotMatch:CurrentID:  " << m_sourceID << " newId: " << id << std::endl;
}

void AudioPlayerTest::onPlaybackFinished(SourceId id){
	if(id == m_sourceID){
		std::cout << " PlaybackFinished:SourceId: " << m_sourceID << std::endl;
		m_executor.submit([this]() {executePlaybackFinished();});
	}else
		std::cout << " PlaybackFinished:SourceIsNotMatch:CurrentID: " << m_sourceID << " newId: " << id << std::endl;

}	

void AudioPlayerTest::onPlaybackError(SourceId id, const aisdk::utils::mediaPlayer::ErrorType& type, std::string error){
	if(id == m_sourceID){
		std::cout << " PlaybackError:SourceId: " << m_sourceID << std::endl;
		auto errStr = aisdk::utils::mediaPlayer::errorTypeToString(type);
		std::cout << errStr << std::endl;
	}else
		std::cout << " onPlaybackError:SourceIsNotMatch:CurrentID: " << m_sourceID << " newId: " << id << std::endl;

}

void AudioPlayerTest::onPlaybackStopped(SourceId id){
	if(id == m_sourceID){
		std::cout << " PlaybackStopped:SourceId: " << m_sourceID << std::endl;
	}else
		std::cout << " PlaybackStopped:SourceIsNotMatch:CurrentID: " << m_sourceID << " newId: " << id << std::endl;

}

void AudioPlayerTest::onPlaybackPaused(SourceId id){
	if(id == m_sourceID){
		std::cout << " PlaybackPaused:SourceId: " << m_sourceID << std::endl;
	}else
		std::cout << " PlaybackPaused:SourceIsNotMatch:CurrentID: " << m_sourceID << " newId: " << id << std::endl;

}

void AudioPlayerTest::onPlaybackResumed(SourceId id){
	if(id == m_sourceID){
		std::cout << " PlaybackResumed:SourceId: " << m_sourceID << std::endl;
	}else
		std::cout << " PlaybackResumed:SourceIsNotMatch:CurrentID: " << m_sourceID << " newId: " << id << std::endl;

}

void AudioPlayerTest::executePlaybackFinished() {
	std::chrono::milliseconds offset = std::chrono::milliseconds::zero();
	std::string url;
	if(urlTank[m_index] != NULL) {
		url = std::string(urlTank[m_index]);
	} else {
		m_index = 0;
		url = std::string(urlTank[m_index]);
	}
	m_index++;
	std::cout << "nextItem: " << m_index-1 << " ";
	m_sourceID = m_playWrapper->setSource(url, offset);
	if(aisdk::utils::mediaPlayer::MediaPlayerInterface::ERROR == m_sourceID){
		std::cout << "run:reason=setSourceUrlFailed" << std::endl;
		return;
	}
	std::cout << "newSourceId= " << m_sourceID << std::endl;
	if(!m_playWrapper->play(m_sourceID)) {
		std::cout << "playFailed." << std::endl;
	}
}

void PaHelp(){
	printf("Options: PaWrapperTest [options]\n" \
		"\t -u set play a url resource.\n" \
		"\t -f Set play a filename stream resource.\n" \
		"\t -p set url stream start position[uint: sec]\n" \
		"\t -r Set play a filename as attachment resource.\n" \
		"\t\t the resource format must be: s16_le,16000,mono\n");
}

int work(std::string &url, std::string &filename, std::string &attachment, std::chrono::milliseconds offset){
	auto audioPlayer = AudioPlayerTest::createNew();
	std::cout << "audioPlayer shared_ptr:user_count= " << audioPlayer.use_count() << std::endl;

	if(!url.empty()){
		std::cout << "offset: " << offset.count() << std::endl;
		audioPlayer->run(url, offset);
	}else if(!filename.empty()){
		std::shared_ptr<std::ifstream> input = std::make_shared<std::ifstream>();
		input->open(filename, std::ifstream::in);
		if(!input->is_open()){
			std::cout << "Open the file is failed\n" << std::endl;
			return -1;
		}
		audioPlayer->run(input);
	}else if (!attachment.empty()) {
		std::shared_ptr<std::ifstream> input = std::make_shared<std::ifstream>();
		input->open(attachment, std::ifstream::in);
		if(!input->is_open()){
			std::cout << "Open the file is failed\n" << std::endl;
			return -1;
		}
		auto attachmentReader = std::make_shared<MockAttachmentReader>(input);
		audioPlayer->run(attachmentReader);
	} else{
		audioPlayer.reset();
	}
	
	std::cout << "audioPlayer shared_ptr:user_count= " << audioPlayer.use_count() << std::endl;
	audioPlayer.reset();
	
	std::cout << "play finished" << std::endl;

	return 0;
}

int main(int argc, char *argv[]){
	//std::shared_ptr<std::ifstream> input;
	std::string url;
	std::string filename;
	std::string attachmentName;	
	std::chrono::milliseconds offset = std::chrono::milliseconds::zero();
		
	int opt;
	int i=0;
	for(i=0; urlTank[i] != NULL; i++) {
		printf("index: %d, url: %s\n", i , urlTank[i]);
	}
	
	while((opt = getopt(argc, argv, "yhu:r:p:f:")) != -1) {
	switch (opt) {
		case 'u':
			url = optarg;
			break;
		case 'p':
			offset = std::chrono::milliseconds(atoi(optarg)*1000);
			break;
		case 'f':
			filename = optarg;
			break;
		case 'r':
			attachmentName = optarg;
			break;			
		case 'h':
			PaHelp();
			exit(EXIT_FAILURE);
		case 'y':
			flags = 1;
			break;
		default:
			printf("optopt = %c\n", (char)optopt);
			printf("opterr = %d\n", opterr);
			fprintf(stderr, "usage: \n");
			PaHelp();
		   exit(EXIT_FAILURE);
	}
	}

	work(url, filename, attachmentName, offset);

	getchar();
	std::cout << "play finished exit==========" << std::endl;
}

