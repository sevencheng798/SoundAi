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
	bool seek(uint64_t offset) override { return fileSeek(); };
	uint64_t getNumUnreadBytes() override { return 0; };
	void close(ClosePoint closePoint = ClosePoint::AFTER_DRAINING_CURRENT_BUFFER) override;
	/// @}

	bool fileSeek() {
		std::cout << "==>file position: " << m_stream->tellg() << std::endl;
		m_stream->clear();
		m_stream->seekg(0, m_stream->beg);
		std::cout << "-->file position: " << m_stream->tellg() << std::endl;
		return true;
	};
	
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
	m_stream->close();
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

	void setAttachmentReader(std::shared_ptr<aisdk::utils::attachment::AttachmentReader> reader);

	std::shared_ptr<aisdk::utils::attachment::AttachmentReader> getAttachmentReader();
private:
	std::shared_ptr<AOWrapper> m_playWrapper;

	AOWrapper::SourceId m_sourceID;

	std::shared_ptr<std::istream> m_stream;

	int m_index;

	std::shared_ptr<aisdk::utils::attachment::AttachmentReader> m_attachmentReader;

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
                       .sampleRateHz = 48000,
                       .sampleSizeInBits = 16,
                       .numChannels = 2,
                       .dataSigned = true};
	#endif
	setAttachmentReader(attachmentReader);
	//aisdk::utils::AudioFormat format;
	m_sourceID = m_playWrapper->setSource(getAttachmentReader(), &format);
	if(aisdk::utils::mediaPlayer::MediaPlayerInterface::ERROR == m_sourceID){
		std::cout << "run:reason=setSourceStreamFailed" << std::endl;
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
	aisdk::utils::AudioFormat format{.encoding = aisdk::utils::AudioFormat::Encoding::LPCM,
                       .endianness = aisdk::utils::AudioFormat::Endianness::LITTLE,
                       .sampleRateHz = 48000,
                       .sampleSizeInBits = 16,
                       .numChannels = 2,
                       .dataSigned = true};

	std::cout << "nextItem: " << m_index++ << " ";
	auto attachmentReader = getAttachmentReader();
	attachmentReader->seek(0);
	m_sourceID = m_playWrapper->setSource(attachmentReader, &format);
	if(aisdk::utils::mediaPlayer::MediaPlayerInterface::ERROR == m_sourceID){
		std::cout << "run:reason=setSourceUrlFailed" << std::endl;
		return;
	}
	std::cout << "newSourceId= " << m_sourceID << std::endl;
	if(!m_playWrapper->play(m_sourceID)) {
		std::cout << "playFailed." << std::endl;
	}
}


void AudioPlayerTest::setAttachmentReader(
	std::shared_ptr<aisdk::utils::attachment::AttachmentReader> reader) {
	m_attachmentReader = reader;
}

std::shared_ptr<aisdk::utils::attachment::AttachmentReader> AudioPlayerTest::getAttachmentReader() {
	return m_attachmentReader;
}

void PaHelp(){
	printf("Options: PaWrapperTest [options]\n" \
		"\t -u set play a url resource.\n" \
		"\t -f Set play a filename stream resource.\n" \
		"\t -p set url stream start position[uint: sec]\n" \
		"\t -r Set play a filename as attachment resource.\n" \
		"\t\t the resource format must be: s16_le,16000,mono\n");
}

int work(std::string &filename){
	auto audioPlayer = AudioPlayerTest::createNew();
	std::cout << "audioPlayer shared_ptr:user_count= " << audioPlayer.use_count() << std::endl;

	if (!filename.empty()) {
		std::shared_ptr<std::ifstream> input = std::make_shared<std::ifstream>();
		input->open(filename, std::ifstream::in);
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
	while((opt = getopt(argc, argv, "hf:")) != -1) {
	switch (opt) {
		case 'u':
			url = optarg;
			break;
		case 'f':
			attachmentName = optarg;
			break;		
		case 'h':
			PaHelp();
			exit(EXIT_FAILURE);
		default:
			printf("optopt = %c\n", (char)optopt);
			printf("opterr = %d\n", opterr);
			fprintf(stderr, "usage: \n");
			PaHelp();
		   exit(EXIT_FAILURE);
	}
	}

	work(attachmentName);

	getchar();
	std::cout << "play finished exit==========" << std::endl;
}

