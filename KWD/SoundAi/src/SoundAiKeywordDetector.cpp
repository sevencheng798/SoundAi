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
#include <fstream>
#include <Utils/Logging/Logger.h>
#include "KeywordDetector/SoundAiKeywordDetector.h"

 /// String to identify log entries originating from this file.
static const std::string TAG("SoundAiKeywordDetector");
 
/**
 * Create a LogEntry using this file's TAG and the specified event string.
 *
 * @param The event string for this @c LogEntry.
 */
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

namespace aisdk {
namespace kwd {

using namespace utils::sharedbuffer;

/// The string of paraments default configure path.
static const std::string DEFAULT_CONFIG("/data/soundai/sai_config");

/// Default size of underlying Sharedbuffer when created internally.
/// This buffer is used to send denoised data to ASR Engine @c GenricAutomaticSpeechRecognizer.
static const int DENOISE_BUFFER_DEFAULT_SIZE_IN_BYTES = 0x100000;

/// The number of hertz per kilohertz.
static const size_t HERTZ_PER_KILOHERTZ = 1000;

/// The SoundAi denoise compatible AIUI sample rate of 16 kHz.
static const unsigned int SOUNDAI_DENOISE_COMPATIBLE_SAMPLE_RATE = 16000;

/// The number of channel before denoise.
static const unsigned int SOUNDAI_DENOISE_COMPATIBLE_CHANNELS = 8;

/// The timeout to use for read calls to the SharedDataStream.
const std::chrono::milliseconds TIMEOUT_FOR_READ_CALLS = std::chrono::milliseconds(1000);

std::unique_ptr<SoundAiKeywordDetector> SoundAiKeywordDetector::create(
	std::shared_ptr<utils::sharedbuffer::SharedBuffer> stream,
	std::unordered_set<std::shared_ptr<dmInterface::KeyWordObserverInterface>> keywordObserver,
	std::chrono::milliseconds maxSamplesPerPush) {
	if(!stream) {
		AISDK_ERROR(LX("CreateFiled").d("reason", "nullStream"));
		return nullptr;
	}
	
	auto detector = std::unique_ptr<SoundAiKeywordDetector>(
		new SoundAiKeywordDetector(stream, keywordObserver, maxSamplesPerPush));
	if(!detector->init()) {
		AISDK_ERROR(LX("CreateFailed").d("reason", "initDetectorFailed"));
		return nullptr;
	}

	return detector;
}

SoundAiKeywordDetector::~SoundAiKeywordDetector() {
	AISDK_INFO(LX("~SoundAiKeywordDetector Destructor."));
	m_isShuttingDown = true;
	if (m_detectionThread.joinable()) {
		m_detectionThread.join();
	}
	// Cleanup denoise stream writer.
	destoryDenoiseWriter();
	
	if(m_denoiseContext)
		sai_denoise_release(m_denoiseContext);
	m_denoiseContext = nullptr;
}

SoundAiKeywordDetector::SoundAiKeywordDetector(
	std::shared_ptr<utils::sharedbuffer::SharedBuffer> stream,
	std::unordered_set<std::shared_ptr<dmInterface::KeyWordObserverInterface>> keywordObserver,
	std::chrono::milliseconds maxSamplesPerPush):GenericKeywordDetector(keywordObserver),
		m_isShuttingDown{false},
		m_stream{stream},
		m_streamReader{nullptr},
		m_denoiseStream{nullptr},
		m_denoiseWriter{nullptr},
		m_maxSamplesPerPush(
			(SOUNDAI_DENOISE_COMPATIBLE_SAMPLE_RATE/HERTZ_PER_KILOHERTZ) * 
			(SOUNDAI_DENOISE_COMPATIBLE_CHANNELS) *maxSamplesPerPush.count()),
		m_denoiseConfig{nullptr},
		m_denoiseContext{nullptr} {

}

bool SoundAiKeywordDetector::init() {
	int errCode = sai_denoise_cfg_init(DEFAULT_CONFIG.c_str(), &m_denoiseConfig);
	if(SAI_ASP_ERROR_SUCCESS != errCode) {
	    AISDK_ERROR(LX("initFailed").d("reason", "sai_denoise_cfg_init"));
		return false;
	}

	errCode = sai_denoise_cfg_add_event_handler(
			m_denoiseConfig,
			DENOISE_EVENT_TYPE_WAKE,
			handleKeywordDetectedCallback,
			reinterpret_cast<void*>(this));
	if(SAI_ASP_ERROR_SUCCESS != errCode) {
	    AISDK_ERROR(LX("initFailed").d("reason", "add_data_handlerWAKECB"));
		return false;
	}
	
	errCode = sai_denoise_cfg_add_event_handler(
			m_denoiseConfig,
			DENOISE_EVENT_TYPE_VAD,
			handleDenoiseVADCallback, 
			nullptr);
	if(SAI_ASP_ERROR_SUCCESS != errCode) {
	    AISDK_ERROR(LX("initFailed").d("reason", "add_data_handlerVAD"));
		return false;
	}

	errCode = sai_denoise_cfg_add_data_handler(
			m_denoiseConfig, 
			DENOISE_DATA_TYPE_ASR,
			handleDenoiseStreamCallback,
			reinterpret_cast<void*>(this));
	if(SAI_ASP_ERROR_SUCCESS != errCode) {
	    AISDK_ERROR(LX("initFailed").d("reason", "add_data_handlerASR"));
		return false;
	}

	errCode = sai_denoise_init(m_denoiseConfig, &m_denoiseContext); 
	if(SAI_ASP_ERROR_SUCCESS != errCode) {
	    AISDK_ERROR(LX("initFailed").d("reason", "sai_denoise_init"));
		return false;
	}

	// Create a @c Reader's to read data from the @c SharedBufferStream.
    m_streamReader = m_stream->createReader(Reader::Policy::BLOCKING);
    if (!m_streamReader) {
        AISDK_ERROR(LX("initFailed").d("reason", "createStreamReaderFailed"));
        return false;
    }

	establishDenoiseWriter();

	m_isShuttingDown = false;
	m_detectionThread = std::thread(&SoundAiKeywordDetector::detectionLoop, this);
	
	//AISDK_INFO(LX("initSuccessed").d("newSessionId", m_sessionId));
	return true;
}

bool SoundAiKeywordDetector::establishDenoiseWriter() {
	/*
     * Creating the buffer (Shared Buffer Stream) that will hold denoise audio data.
     */
    if(!m_denoiseStream) {
		size_t bufferSize = utils::sharedbuffer::SharedBuffer::calculateBufferSize(
			DENOISE_BUFFER_DEFAULT_SIZE_IN_BYTES);
		AISDK_INFO(LX("establishDenoiseWriter").d("bufferSize", bufferSize));
		auto buffer = std::make_shared<utils::sharedbuffer::SharedBuffer::Buffer>(bufferSize);
		m_denoiseStream = utils::sharedbuffer::SharedBuffer::create(buffer);
		if(!m_denoiseStream) {
			AISDK_ERROR(LX("establishDenoiseWriterFailed").d("reason", "createDenoiseStreamFailed"));
			return false;
		}

		m_denoiseWriter = m_denoiseStream->createWriter(utils::sharedbuffer::Writer::Policy::NONBLOCKABLE);
		if(!m_denoiseWriter) {
			AISDK_ERROR(LX("establishDenoiseWriterFailed").d("reason", "createDenoiseWriterFailed"));
			return false;
		}
    }

	return true;
}

void SoundAiKeywordDetector::destoryDenoiseWriter() {
	m_denoiseWriter->close();
	m_denoiseWriter.reset();
	m_denoiseStream.reset();
}

void SoundAiKeywordDetector::handleKeywordDetectedCallback(
	sai_denoise_ctx_t* denoiseContext,
	int32_t code,
	const void* payload,
	void* userData) {
	if (!userData) {
		AISDK_ERROR(LX("handleDenoiseStreamCallbackFailed").d("reason", "nullDetector."));
		return;
	}

	if (!payload) {
		AISDK_ERROR(LX("handleDenoiseStreamCallbackFailed").d("reason", "nullPayload."));
		return;  
	}
	
	SoundAiKeywordDetector *detector = static_cast<SoundAiKeywordDetector*>(userData);
	sai_denoise_wake_t* wkload = (sai_denoise_wake_t*)payload;
	AISDK_INFO(LX("handleKeywordDetectedCallback")
				.d("keyword", wkload->word)
				.d("angle", wkload->angle));
	
	// Notify keyword observer if detecte keyword wakeup event successfuly. defaule set beginIndex and endIndex: 0.
	detector->notifyKeyWordObservers(detector->m_denoiseStream, "xiaokang", 0, 0);
}

void SoundAiKeywordDetector::handleDenoiseVADCallback(
	sai_denoise_ctx_t* denoiseContext,
	int32_t code,
	const void* payload,
	void* userData) {
	if (!payload) {
		return;
	}
	int vad = *(int*)payload;
	switch (vad) {
		case 0:
			AISDK_INFO(LX("handleDenoiseVADCallback").d("vad", "begin"));
		break;
		case 1:
			AISDK_INFO(LX("handleDenoiseVADCallback").d("vad", "end"));
			sai_denoise_stop_beam(denoiseContext);
			break;
		default:
			AISDK_INFO(LX("handleDenoiseVADCallback").d("vad", vad));
			break;
	}
}

enum Sai_Debug_data_Type { 
	Sai_Debug_Raw,
	Sai_Debug_ASR1,
	Sai_Debug_IVW,
	Sai_Debug_ASR2
};

static void writeToFile(int32_t id, Sai_Debug_data_Type type, const std::string& data) {
	std::stringstream ss;
#ifdef __ANDROID__  
	ss << "/sdcard/";
#else
	ss << "/tmp/";
#endif

	std::string datatype("unknown");
	switch (type) {
		case Sai_Debug_Raw:
			datatype = "RAW";
		break;
		case Sai_Debug_ASR1:
			datatype = "ASR1";
		break;
		case Sai_Debug_IVW:
			datatype = "IVW"; 
		break;
		case Sai_Debug_ASR2:
			datatype = "ASR2";
		break;
	}
	
	ss << id << "_" << datatype << ".pcm";
	std::fstream fs(ss.str(), std::fstream::out | std::fstream::app);
	if (fs.good()) {
		fs.write(data.data(), data.size());
	}
	fs.close();
}

void SoundAiKeywordDetector::handleDenoiseStreamCallback(
	sai_denoise_ctx_t* denoiseContext,
	const char* data,
	size_t size,
	void* userData) {
	SoundAiKeywordDetector *detector = static_cast<SoundAiKeywordDetector*>(userData);
	if (!detector) {
		AISDK_ERROR(LX("handleDenoiseStreamCallback").d("reason", "nullDetector."));
		return;
	}

	if (data && size > 0) {
		//AISDK_INFO(LX("handleDenoiseStreamCallback").d("size", size));
		writeToFile(1100, Sai_Debug_ASR1, std::string(data, size));
		detector->m_denoiseWriter->write(data, size);
	}
}

void SoundAiKeywordDetector::detectionLoop() {
	std::vector<int16_t> audioDataToPush(m_maxSamplesPerPush); //(16*8*10)*2 = (1280)*2 = 2560 = 20ms 
	int errCode = SAI_ASP_ERROR_SUCCESS;
	ssize_t wordsRead;
	//std::ofstream output("/tmp/Backup.pcm");

	while(!m_isShuttingDown) {

		bool didErrorOccur = false;
		// Start read data.
		wordsRead = readFromStream(
				m_streamReader,
				m_stream,
				audioDataToPush.data(),
				audioDataToPush.size(),
				TIMEOUT_FOR_READ_CALLS,
				&didErrorOccur);
		// Error occurrence maybe reader close.
		if(didErrorOccur) {
			break;
		} else if (wordsRead == Reader::Error::OVERRUN) {
			
		} else if(wordsRead > 0) {
			void *pbuf8 = audioDataToPush.data();
			//output.write(static_cast<char *>(pbuf8), wordsRead*sizeof(*audioDataToPush.data()));

			errCode = sai_denoise_feed(m_denoiseContext, static_cast<char *>(pbuf8), wordsRead*sizeof(*audioDataToPush.data()));
			if (SAI_ASP_ERROR_SUCCESS != errCode){
				AISDK_ERROR(LX("detectionLoopFailed").d("reason", "sai_denoise_feed err"));
			}
		}
		
	}
	// Close the @c Reader which read stream occurence.
	m_streamReader->close();

}

}	// namespace kwd
}  //namespace aisdk
