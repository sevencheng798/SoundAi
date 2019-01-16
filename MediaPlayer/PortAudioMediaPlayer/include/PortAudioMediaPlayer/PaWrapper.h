/*
 * Copyright 2018 gm its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#ifndef __PAWRAPPER__H_
#define __PAWRAPPER__H_

#include <memory>
#include <mutex>
#include <stdbool.h>

/// Portaudio APIs header
#include <portaudio.h>

#include <Utils/MediaPlayer/MediaPlayerInterface.h>
#include "FFmpegInputControllerInterface.h"
#include "PaBufferQueue.h"

namespace aisdk {
namespace mediaPlayer{
namespace ffmpeg{

/**
 * This class implements an media player.
 *
 * The implementation uses portaudio APIs to play the audio and FFmpeg to decode and resample the media input.
 */	
class PaWrapper : public utils::mediaPlayer::MediaPlayerInterface {
public:
    /**
     * Creates an portaudio stream object.
     *
     * @param sampleFormat A pointer to the sampleformat type. It shall include at @PaSampleFormat.
     * @param channelCount represents the stream whether is stereo or mono.
     * @return A pointer to the @c PaWrapper if succeed; @c nullptr otherwise.
     */
	static std::unique_ptr<PaWrapper> create(	
	int sampleFormat = paInt16,	
	PaBufferQueue::Channels channelCount = PaBufferQueue::Channels::STEREO,
	const PlaybackConfiguration& config = PlaybackConfiguration());

    /// @name MediaPlayerInterface methods.
    ///@{
    SourceId setSource(const std::string& url, std::chrono::milliseconds offset) override;
	SourceId setSource(std::shared_ptr<std::istream> stream, bool repeat) override;
    bool play(SourceId id) override;
    bool stop(SourceId id) override;
	bool pause(SourceId id) override;
	bool resume(SourceId id) override;
	void setObserver(
		std::shared_ptr<utils::mediaPlayer::MediaPlayerObserverInterface> playerObserver) override;
	///@}
	
    bool shutdown();

    PaStream *getPaStream();

	/// Destructor
    ~PaWrapper();
private:
	/**
     * Constructor
     */	
    PaWrapper(int sampleFormat, PaBufferQueue::Channels channelCount, const PlaybackConfiguration& config);
		
	/// initialize Portaudio
	bool initialize();

	/**
     * Internal method used to create a new media queue and increment the request id.
     */
	int configureNewRequest(
			std::unique_ptr<FFmpegInputControllerInterface> inputController,
			std::chrono::milliseconds offset = std::chrono::milliseconds(0));

	/// Internal method implements the stop media player logic. This method should be called after acquring @c m_mutex
    bool stopLocked();

    /* This routine will be called by the PortAudio engine when audio is needed.
    ** It may called at interrupt level on some machines so don't do anything
    ** that could mess up the system like calling malloc() or free().
    */
    static int portAudioCallback( const void *inputBuffer, void *outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData );

    /*
     * This routine is called by portaudio when playback is done.
     */
    static void paStreamFinished(void* userData);
	
    PaStream *m_stream;

    /// The current source id.
    SourceId m_sourceId;
	
    /// Save the initial media offset to compute total offset.
    std::chrono::milliseconds m_initialOffset;

	/// The buffer media queue.
    std::shared_ptr<PaBufferQueue> m_mediaQueue;

    // The android media player configuration.
    PlaybackConfiguration m_config;

	std::shared_ptr<utils::mediaPlayer::MediaPlayerObserverInterface> m_observer;

	/// A type used to specify one or more sample formats.
	int m_sampleFormat;

	/// The number of channels of sound to be delivered to the stream data
	PaBufferQueue::Channels m_channelCount;

    /// Mutex used to synchronize media player operations.
    std::mutex m_operationMutex;
	
	/// Mutex used to synchronize @c request creation.
	std::mutex m_requestMutex;
		
};

}//namespace ffmpeg
} //namespace mediaPlayer
} //namespace aisdk
#endif //__PAWRAPPER__H_