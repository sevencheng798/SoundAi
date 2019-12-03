/*
 * Copyright 2019 gm its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 *
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifndef _ECHO_CANCELLATION_IMPLEMENTATION_H_
#define _ECHO_CANCELLATION_IMPLEMENTATION_H_
#include <string>
#include <mutex>
#include <unordered_set>

#include "echo_cancellation.h"
#include "ring_buffer.h"
#include "audio_buffer.h"

#include "KeywordDetector/EchoCancellationObserverInterface.h"

namespace aisdk {
namespace kwd {
using namespace webrtc;

typedef void (*AecStreamHandler)(const int16_t *frame, size_t size, void *user_data);

/**
*  An AEC class
*/
class EchoCancellationImplemention {
public:
	
	/**
	 * Creates a @c SoundAiKeywordDetector.
	 *
	 * @param stream The stream of audio data. This should be formatted in LPCM encoded with 16 bits per sample
     * with 8channels and have a sample rate of 16 kHz. Additionally, the data should be in little endian format.
	 * @param keyWordObservers The observers to notify of keyword detections.
	 * @param maxSamplesPerPush The amount of data in milliseconds to push to SoundAi denoise at a time.
	 * @Return A new @c SoundAiKeywordDetector, or @c nullptr if the operation failed.
	 */
	static std::unique_ptr<EchoCancellationImplemention> create(
		int streamDelayMillisecond = 20,
		int channels = 8,
		int sampleRateHz = 16000);

  	// Processes a 10 ms |frame| of the primary audio stream. On the client-side,
  	// this is the near-end (or captured) audio.
  	int processStream(const char *frame, size_t size);
	int processStream(const int16_t *frame, size_t size);
	int setAecStreamHandlerCallback(AecStreamHandler handler, void *userData);
	
	/// Destructor.
	~EchoCancellationImplemention();
private:
	static constexpr int frameStep = 10;
	class Canceller;
	
	/**
	 * Constructor.
	 * @param stream The stream of audio data. This should be formatted in LPCM encoded with 16 bits per sample
     * with 8channels and have a sample rate of 16 kHz. Additionally, the data should be in little endian format.
	 * @param keyWordObservers The observers to notify of keyword detections.
	 * @param maxSamplesPerPush The amount of data in milliseconds to push to SoundAi denoise at a time.
	 */
	EchoCancellationImplemention(
		int streamDelayMillisecond, int channels, int sampleRateHz);

	/**
     * Initializes the stream reader, sets up the SoundAi denoise engine, and kicks off a thread to begin processing 
     * data from the stream. This function should only be called once with each new @c SoundAiKeywordDetector.
     *
     * @return @c true if the engine was initialized properly and @c false otherwise.
     */
	bool init();

	void setSteamDelayMs(int delay);
	/**
	 * Calculate stream delay number.
	 */
	void calculateStreamDelayNumber();

	bool createStreamBuffer();

	void freeStreamBuffer();

	/// stream delay process on reference frame.
	int m_streamDelayMillisecond;

	/// stream delay number on reference frame.
	/// in order to can match mic frame.
	int m_streamDelayNumber;

	int m_streamDelayCount;
	
	/// Input audio sample rate.
	int m_sampleRateHz;

	int m_channels;
	
	size_t m_samplesPreChannel;

	AudioBuffer *m_micAudioBuffer;
	
	RingBuffer *m_micBuffer;

	/// A instance of Canceller
	std::unique_ptr<Canceller> m_canceller;
	
    /**
     * The handler to notify on AEC stream.
     */
    AecStreamHandler m_aecObservers;
	// To point aecHandler.
	void *m_opdata;

};

}	// namespace kwd
}  //namespace aisdk

#endif  // _ECHO_CANCELLATION_IMPLEMENTATION_H_
