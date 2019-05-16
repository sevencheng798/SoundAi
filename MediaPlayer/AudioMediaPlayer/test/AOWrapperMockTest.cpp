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

#include "Utils/Attachment/AttachmentReader.h"
#include "Utils/MediaPlayer/MediaPlayerObserverInterface.h"
#include "AudioMediaPlayer/AOWrapper.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace aisdk {
namespace mediaPlayer {
namespace ffmpeg {
namespace test {
	
using namespace utils::attachment;
using namespace utils::mediaPlayer;

using namespace ::testing;

static const std::string RAW_STREAM_FILENAME("/tmp/Happy.wav");
static const std::string MP3_STREAM_FILENAME("/tmp/yxdcb.mp3");

class MockAttachmentReader : public AttachmentReader {
public:
	MockAttachmentReader(
		std::shared_ptr<std::ifstream> stream,
		ssize_t timeoutIteration = -1):
		m_stream{stream},
		m_iteration{-1},
		m_timeoutIteration{timeoutIteration} {

	};
		
	/// @name AttachmentReader method;
	/// @{
	MOCK_METHOD1(seek, bool(uint64_t));
	MOCK_METHOD0(getNumUnreadBytes, uint64_t());
	MOCK_METHOD1(close, void(ClosePoint closePoint));
	std::size_t read(
        void* buf,
        std::size_t numBytes,
        ReadStatus* readStatus,
        std::chrono::milliseconds timeoutMs = std::chrono::milliseconds(0)) override;
	/// @}
	~MockAttachmentReader() {
		std::cout << "~MockAttachmentReader destory" << std::endl;
		m_stream.reset();
	}
private:
	std::shared_ptr<std::ifstream> m_stream;
	/// Read iterations counter.
    ssize_t m_iteration;

    /// This can be used to trigger a timeout response when the iteration reaches the timeout iteration.
    ssize_t m_timeoutIteration;
};

std::size_t MockAttachmentReader::read(
	void* buf,
	std::size_t numBytes,
	ReadStatus* readStatus,
	std::chrono::milliseconds timeoutMs) {
	
	m_iteration++;
	if (m_iteration == m_timeoutIteration) {
        (*readStatus) = AttachmentReader::ReadStatus::OK_WOULDBLOCK;
        return AVERROR(EAGAIN);
    }

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
	
	(*readStatus) = aisdk::utils::attachment::AttachmentReader::ReadStatus::OK;
    return bytesRead;
}

/// Mocks the media player observer.
class MockObserver : public MediaPlayerObserverInterface {
public:
    MOCK_METHOD1(onPlaybackStarted, void(SourceId));
    MOCK_METHOD1(onPlaybackFinished, void(SourceId));
    MOCK_METHOD1(onPlaybackStopped, void(SourceId));
    MOCK_METHOD1(onPlaybackPaused, void(SourceId));
    MOCK_METHOD1(onPlaybackResumed, void(SourceId));
    MOCK_METHOD3(onPlaybackError, void(SourceId, const ErrorType&, std::string));

    MOCK_METHOD1(onBufferRefilled, void(SourceId));
    MOCK_METHOD1(onBufferUnderrun, void(SourceId));
};

/// Class that can be used to wait for an event.
class WaitEvent {
public:
    /// Wake up a thread that is waiting for this event.
    void wakeUp();

    /// The default timeout for an expected event.
    static const std::chrono::seconds DEFAULT_TIMEOUT;

    /**
     * Wait for wake up event.
     *
     * @param timeout The maximum amount of time to wait for the event.
     */
    std::cv_status wait(const std::chrono::milliseconds& timeout = DEFAULT_TIMEOUT);

private:
    /// The condition variable used to wake up the thread that is waiting.
    std::condition_variable m_condition;
};

/// Test class for @c AOWrapper.
class AOWrapperPlayerTest : public Test {
protected:
	void SetUp() override {
		/// Create object for the @c AOEngine.
		m_aoEngine = AOEngine::create();
		/// Create object for the @c AOWrapper.
		m_player = AOWrapper::create(m_aoEngine);
		m_observer = std::make_shared<NiceMock<MockObserver>>();
		m_player->setObserver(m_observer);
	}
	
    std::shared_ptr<std::ifstream> createStream(const std::string &filename) {
        auto stream = std::make_shared<std::ifstream>(filename);
		if(stream->is_open()) 
			return stream;
		else
			return nullptr;
    }

	void TearDown() override {
		m_reader.reset();
	}

	std::shared_ptr<AOEngine> m_aoEngine;
	std::shared_ptr<MockObserver> m_observer;
	std::shared_ptr<AOWrapper> m_player;
	std::shared_ptr<MockAttachmentReader> m_reader;
};

TEST_F(AOWrapperPlayerTest, testCreateNullEngine) {
	auto player = AOWrapper::create(nullptr);
	EXPECT_EQ(player, nullptr);
}

TEST_F(AOWrapperPlayerTest, testFirstReadTimeout) {
	static const ssize_t firstIteration = 1;
	auto stream = createStream(MP3_STREAM_FILENAME);
	EXPECT_NE(stream, nullptr);
	m_reader = std::make_shared<MockAttachmentReader>(stream, firstIteration);
	auto id = m_player->setSource(m_reader, nullptr);

	WaitEvent finishedEvent;
	EXPECT_CALL(*m_observer, onPlaybackStarted(id)).Times(1);
	EXPECT_CALL(*m_observer, onPlaybackFinished(id)).WillOnce(InvokeWithoutArgs([&finishedEvent]() {
		finishedEvent.wakeUp();	
	}));

	EXPECT_TRUE(m_player->play(id));
	EXPECT_EQ(finishedEvent.wait(std::chrono::seconds(5)), std::cv_status::no_timeout);
}

TEST_F(AOWrapperPlayerTest, testRawStreamFromFiles) {
	auto stream = createStream(RAW_STREAM_FILENAME);
	EXPECT_NE(stream, nullptr);
	utils::AudioFormat format {
	   .encoding = aisdk::utils::AudioFormat::Encoding::LPCM,
       .endianness = aisdk::utils::AudioFormat::Endianness::LITTLE,
       .sampleRateHz = 48000,
       .sampleSizeInBits = 16,
       .numChannels = 2,
       .dataSigned = true
	};

	WaitEvent finishedEvent;
	auto m_reader = std::make_shared<MockAttachmentReader>(stream);
	auto id = m_player->setSource(m_reader, &format);
#if 0	
	EXPECT_CALL(*m_observer, onPlaybackStarted(id)).Times(1);
	EXPECT_CALL(*m_observer, onPlaybackFinished(id)).WillOnce(InvokeWithoutArgs([&finishedEvent]() {
		finishedEvent.wakeUp();
	}));
#else	
	//EXPECT_CALL(*m_observer, onPlaybackStarted(id)).Times(1);
	EXPECT_CALL(*m_observer, onPlaybackStarted(id)).WillOnce(InvokeWithoutArgs([&finishedEvent]() {
		finishedEvent.wakeUp();
	}));
#endif	
	EXPECT_TRUE(m_player->play(id));
	//EXPECT_EQ(finishedEvent.wait(), std::cv_status::no_timeout);
	EXPECT_NE(finishedEvent.wait(), std::cv_status::no_timeout);
}

TEST_F(AOWrapperPlayerTest, testMp3StreamFromFile) {
	auto stream = createStream(MP3_STREAM_FILENAME);
	EXPECT_NE(stream, nullptr);

	WaitEvent finishedEvent;
	auto id = m_player->setSource(stream, false);

	EXPECT_CALL(*m_observer, onPlaybackStarted).Times(1);
	EXPECT_CALL(*m_observer, onPlaybackFinished(id)).WillOnce(InvokeWithoutArgs([&finishedEvent]() {
		finishedEvent.wakeUp();
	}));

	EXPECT_TRUE(m_player->play(id));
	EXPECT_EQ(finishedEvent.wait(std::chrono::seconds(250)), std::cv_status::no_timeout);	
}

TEST_F(AOWrapperPlayerTest, testUrlStream) {
	std::string url = std::string("http://audio.xmcdn.com/group10/M03/7F/D6/wKgDaVYWRUKwIdG0AJq6YV7lpjE326.m4a");
	WaitEvent finishedEvent;
	auto id = m_player->setSource(url, std::chrono::milliseconds(30000));

	EXPECT_CALL(*m_observer, onPlaybackStarted(id)).WillOnce(InvokeWithoutArgs([&finishedEvent]() {
		finishedEvent.wakeUp();
	}));

	EXPECT_TRUE(m_player->play(id));
	EXPECT_NE(finishedEvent.wait(), std::cv_status::no_timeout);
}


/// The default timeout for an expected event.
const std::chrono::seconds WaitEvent::DEFAULT_TIMEOUT{30};

/// Wake up a thread that is waiting for this event.
void WaitEvent::wakeUp() {
	m_condition.notify_one();
}


/**
 * Wait for wake up event.
 *
 * @param timeout The maximum amount of time to wait for the event.
 */
std::cv_status WaitEvent::wait(const std::chrono::milliseconds& timeout) {
	std::mutex mutex;
	std::unique_lock<std::mutex> lock(mutex);
	return m_condition.wait_for(lock, timeout);
}

} // namespace test
} // namespace ffmpeg
}// namespace mediaPlayer
} // namespace aisdk
