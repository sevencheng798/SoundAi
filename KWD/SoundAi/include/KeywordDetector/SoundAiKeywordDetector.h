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

#ifndef _SOUNDAI_KEYWORD_DETECTOR_H_
#define _SOUNDAI_KEYWORD_DETECTOR_H_
#include <string>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

// SoundAi open denoise header file.
#include <denoise/denoise.h>

#include "DMInterface/KeyWordObserverInterface.h"
#include "KWD/GenericKeywordDetector.h"

namespace aisdk {
namespace kwd {

/**
*  An KeywordDetector class
*/
class SoundAiKeywordDetector : public GenericKeywordDetector {
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
	static std::unique_ptr<SoundAiKeywordDetector> create(
		std::shared_ptr<utils::sharedbuffer::SharedBuffer> stream,
		std::unordered_set<std::shared_ptr<dmInterface::KeyWordObserverInterface>> keywordObserver,
		std::chrono::milliseconds maxSamplesPerPush = std::chrono::milliseconds(10));

	/// Destructor.
	~SoundAiKeywordDetector() override;
private:
	
	/**
	 * Constructor.
	 * @param stream The stream of audio data. This should be formatted in LPCM encoded with 16 bits per sample
     * with 8channels and have a sample rate of 16 kHz. Additionally, the data should be in little endian format.
	 * @param keyWordObservers The observers to notify of keyword detections.
	 * @param maxSamplesPerPush The amount of data in milliseconds to push to SoundAi denoise at a time.
	 */
	SoundAiKeywordDetector(
		std::shared_ptr<utils::sharedbuffer::SharedBuffer> stream,
		std::unordered_set<std::shared_ptr<dmInterface::KeyWordObserverInterface>> keywordObserver,
		std::chrono::milliseconds maxSamplesPerPush);

	/**
     * Initializes the stream reader, sets up the SoundAi denoise engine, and kicks off a thread to begin processing 
     * data from the stream. This function should only be called once with each new @c SoundAiKeywordDetector.
     *
     * @return @c true if the engine was initialized properly and @c false otherwise.
     */
	bool init();

	/**
	 * Create a stream @c SharedBuffer of the @c m_denoiseStream for denoised audio data.
	 * In order to send the denoise audio stream to 'ASR engine' @c GenricAutomaticSpeechRecognizer.
	 */
	bool establishDenoiseWriter();

	/**
	 * Destory and close the buffer @c SharedBuffer of @c m_denoiseStream and the writer of @c m_denoiseWriter.
	 */
	void destoryDenoiseWriter();

	/**
	 * The callback that Denoise will issue to notify of keyword detections.
	 * 
	 * @param denoiseContext The denoise context callback setting.
	 * @param code The denoise event code.
	 * @param payload The denoise event message instance.
	 * @param userData A pointer to the user data to pass along to the callback.
	 */
	static void handleKeywordDetectedCallback(
		sai_denoise_ctx_t* denoiseContext,
		int32_t code,
		const void* payload,
		void* userData);
	
	/**
	 * The callback that Denoise will issue to notify of vad detections.
	 * 
	 * @param denoiseContext The denoise context callback setting.
	 * @param code The denoise event code.
	 * @param payload The denoise event message instance.
	 * @param userData A pointer to the user data to pass along to the callback.
	 */
	static void handleDenoiseVADCallback(
		sai_denoise_ctx_t* denoiseContext,
		int32_t code,
		const void* payload,
		void* userData);
	
	/**
	 * The callback that Denoise will issue to notify of denoise data.
	 * 
	 * @param denoiseContext The denoise context callback setting.
	 * @param data The samples data of the denoised.
	 * @param size The total bytes of the denoised samples @c data.
	 * @param userData A pointer to the user data to pass along to the callback.
	 */
	static void handleDenoiseStreamCallback(
		sai_denoise_ctx_t* denoiseContext,
		const char* data,
		size_t size,
		void* userData);
	
	/// The main function that reads data and feeds it into the engine.
	void detectionLoop();
	
	/// Indicates whether the internal main loop should keep running.
	std::atomic<bool> m_isShuttingDown;

	/// The stream of audio data from device microphone.
	const std::shared_ptr<utils::sharedbuffer::SharedBuffer> m_stream;

	/// The reader that will be used to read audio data from the stream.
	std::shared_ptr<utils::sharedbuffer::Reader> m_streamReader;

	/// The stream of denoised audio data from SoundAi denoise API.
	std::shared_ptr<utils::sharedbuffer::SharedBuffer> m_denoiseStream;

	/// The writer that will be used to store denoised audio data to @c m_denoiseStream.
	std::shared_ptr<utils::sharedbuffer::Writer> m_denoiseWriter;

	/**
	 * The max number of samples to push into the MSC engine per read data from @c m_streamReader.
	 * This will be determined based on the sampling rate of the audio data passed in.
	 */
	const size_t m_maxSamplesPerPush;	
	
	/// Denoise config structure used to denoise init. 
	sai_denoise_cfg_t *m_denoiseConfig;

	/// Denoise context structure used to remain denoise context resource.
	sai_denoise_ctx_t *m_denoiseContext;

	/// Internal thread that reads audio from the buffer and feeds it to the Sensory engine.
	std::thread m_detectionThread;
};

}	// namespace kwd
}  //namespace aisdk

#endif  // _SOUNDAI_KEYWORD_DETECTOR_H_
