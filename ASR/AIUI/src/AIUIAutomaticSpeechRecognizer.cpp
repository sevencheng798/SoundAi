/*
 * Copyright 2019 its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#include <cstring> 
// jsoncpp ver.1.8.3
#include "json/json.h"
#include <Utils/Logging/Logger.h>
// Support read data(TTS) to a attachment.
#include <Utils/Attachment/InProcessAttachment.h>
#include "AIUI/AIUIAutomaticSpeechRecognizer.h"

#include <iostream>
#include <fstream>
#include <time.h>
#include <stdio.h>
/// String to identify log entries originating from this file.
static const std::string TAG{"AIUIAutomaticSpeechRecognizer"};
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

namespace aisdk {
namespace asr {
namespace aiuiEngine {
	
using namespace utils::sharedbuffer;

/// The name of the @c AudioTrackManager channel used by the @c SpeechSynthesizer.
static const std::string CHANNEL_NAME = utils::channel::AudioTrackManagerInterface::DIALOG_CHANNEL_NAME;

// The name of alarm domain.
static const std::string DOMAIN_ALARM{"alarm"};

// The name of schedule domain.
static const std::string DOMAIN_SCHEDULE{"schedule"};

// The name of playcontrol domain.
static const std::string DOMAIN_PLAYCONTROL{"playcontrol"};

// The name of playcontrol domain.
static const std::string DOMAIN_VOLUME{"volume"};

// A name of expectSpeech be used to match directive handle.
static const std::string EXPECT_SPEECH{"ExpectSpeech"};
// Used within generate ExpectSpeech ID().
static const std::string MESSAGE_ID_COMBINING_SUBSTRING = "expectspeech";

static const char *DOMAIN_EXPECT_SPEECH_FORMAT = "{\"code\":200, \"message\":\"OK\", \"query\":\"\", \"domain\":\"ExpectSpeech\"}";

/// The name of channel handler interface
static const std::string CHANNEL_INTERFACE_NAME{"AIUIAutomaticSpeechRecognizer"};

const std::chrono::milliseconds TIMEOUT_FOR_READ_CALLS = std::chrono::milliseconds(200);

/// Set barge-in timeout that wait release audio channel normaly.
const auto BARGEIN_TIMEOUT = std::chrono::milliseconds{500};

/// Set Thinking to IDLE timeout time
const auto THINKING_TIMEOUT = std::chrono::seconds{6};

/**
 * Set Listening to IDLE timeout time.
 * When the listening state is entered, the ASR engine still cannot receive or recognize 
 * the vaild speech. we should take timeout strategy to quit recognizer state.
 */
const auto LISTENG_TIMEOUT = std::chrono::seconds{5};

/// Set check Activing audio timeout time.
const auto ACTIVING_TIMEOUT = std::chrono::seconds{8};

// #define TTS_RECORD
#ifdef TTS_RECORD
const std::string DEF_RECODER{"/tmp/tts-16k.pcm"};
std::ofstream recoder(DEF_RECODER);
#endif
std::shared_ptr<AIUIAutomaticSpeechRecognizer> AIUIAutomaticSpeechRecognizer::create(
	const std::shared_ptr<utils::DeviceInfo>& deviceInfo,
	std::shared_ptr<utils::channel::AudioTrackManagerInterface> trackManager,
	std::shared_ptr<utils::attachment::AttachmentManagerInterface> attachmentDocker,
	std::shared_ptr<dmInterface::MessageConsumerInterface> messageConsumer,
	std::shared_ptr<asr::ASRRefreshConfiguration> asrRefreshConfig,
	const AutomaticSpeechRecognizerConfiguration& config) {
	if(!trackManager) {
		AISDK_ERROR(LX("CreateFailed").d("reason", "trackManagerIsnullptr."));
		return nullptr;
	}

	if(!attachmentDocker) {
		AISDK_ERROR(LX("CreateFailed").d("reason", "attachmentDockerIsnullptr"));
	}
	
	if(!messageConsumer) {
		AISDK_ERROR(LX("CreateFailed").d("reason", "messageConsumerIsnullptr."));
		return nullptr;
	}

	if(!asrRefreshConfig) {
		AISDK_ERROR(LX("CreateFailed").d("reason", "asrRefreshConfigIsnullptr"));
		return nullptr;
	}
	
	auto appid = config.getAiuiAppId();
	auto configFile = config.getAiuiConfigFile();
	auto aiuiDir = config.getAiuiDir();
	auto logDir = config.getAiuiLogDir();
	auto engine = std::shared_ptr<AIUIAutomaticSpeechRecognizer>( new AIUIAutomaticSpeechRecognizer(
			deviceInfo, trackManager, attachmentDocker, messageConsumer, asrRefreshConfig, appid, configFile, aiuiDir, logDir));
	if(!engine->init()) {
		AISDK_ERROR(LX("CreateFailed").d("reason", "initedFailed."));
		return nullptr;
	}

	return engine;
}

bool AIUIAutomaticSpeechRecognizer::start() {
	// no-op
	return true;
}

bool AIUIAutomaticSpeechRecognizer::stop() {
	// no-op
	return true;
}

bool AIUIAutomaticSpeechRecognizer::reset() {
	// no-op
	return true;
}

std::future<bool> AIUIAutomaticSpeechRecognizer::recognize(
	std::shared_ptr<utils::sharedbuffer::SharedBuffer> stream,
    utils::sharedbuffer::SharedBuffer::Index begin,
    utils::sharedbuffer::SharedBuffer::Index keywordEnd) {
	if(stream && INVALID_INDEX == begin) {
		AISDK_DEBUG5(LX("recognize").d("begin", "INVALID_INDEX"));
		// Should to get current reader index.
	#if 0
		auto startWithNewData = true;
		auto reader = stream->createReader(Reader::Policy::NONBLOCKING, startWithNewData);
		begin = reader->tell();
	#endif
	}

	// Clear the @c m_bargeIn flags.	
	m_bargeIn = false;
	
	// If this is a barge-in, verify that it is permitted.
	auto currentState = getState();
	switch(currentState) {
		case ObserverInterface::State::IDLE:
		case ObserverInterface::State::TIMEOUT:
        {
			std::unique_lock<std::mutex> lock(m_bargeMutex);
			// Release channel.
			releaseForegroundTrace();
			m_conditionBarge.wait_for(
					lock, BARGEIN_TIMEOUT, [this]{ 
					return m_trackState == utils::channel::FocusState::NONE;
			});
		}
			break;
		case ObserverInterface::State::EXPECTING_SPEECH:
		case ObserverInterface::State::RECOGNIZING:
		{
			#if 0
			// For barge-in, we should close the previous reader before creating another one.
            if (m_reader) {
                m_reader->close();
                m_reader.reset();
            }
			#endif
			AISDK_WARN(LX("executeRecognizeFailed").d("reason", "Barge-in is not permitted while recognizing"));
			/**
			 * For barge-in, we should close the previous reader before creating another one.
			 * first to clear state to IDLE.
			 * then to set the barge-in flags - @c m_bargeIn.
			 * final to set vad finished flas - notify the @c sendStreamProcessing to terminate the feed data..
			 */
			setState(ObserverInterface::State::IDLE);
			m_bargeIn = true;
			setVaildVad(true);
			// Release channel.
			releaseForegroundTrace();
			std::unique_lock<std::mutex> lock(m_bargeMutex);
			auto cv_status = m_conditionBarge.wait_for(
					lock, BARGEIN_TIMEOUT, [this]{ 
					return m_trackState == utils::channel::FocusState::NONE;
			});

			if(!cv_status) {
				AISDK_ERROR(LX("executeRecognizeFailed").d("reason", "Barge-in is not permitted while recognizing timeout"));
				// Release channel again.
				releaseForegroundTrace();
				std::promise<bool> p;
				std::future<bool> ret = p.get_future();
				return ret;
			}
		}
			break;
			//return false;
		case ObserverInterface::State::BUSY:
			AISDK_ERROR(LX("executeRecognizeFailed").d("reason", "Barge-in is not permitted while busy"));
			std::promise<bool> p;
			std::future<bool> ret = p.get_future();
			return ret;
	}
	
    return m_executor.submit([this, stream, begin, keywordEnd]() {
        return executeRecognize(stream, begin, keywordEnd);
    });
}

std::future<bool> AIUIAutomaticSpeechRecognizer::acquireTextToSpeech(
		const std::string text,
		std::shared_ptr<utils::attachment::AttachmentWriter> writer) {

    return m_executor.submit([this, text, writer]() {
        return executeTextToSpeech(text, writer);
    });
}

bool AIUIAutomaticSpeechRecognizer::cancelTextToSpeech() {
	m_executor.submit([this]() {
		executeCancelTextToSpeech();
	});

	return true;
}

void AIUIAutomaticSpeechRecognizer::handleEventStateReady() {
	// default no-op
	//executeResetState(); //mark
}

void AIUIAutomaticSpeechRecognizer::handleEventStateWorking() {
	m_readerThread = std::thread(&AIUIAutomaticSpeechRecognizer::sendStreamProcessing, this);
}

void AIUIAutomaticSpeechRecognizer::handleEventConnectToSever(std::string uid) {
	if(uid.empty()) {
		AISDK_ERROR(LX("EVENT_CONNECTED_TO_SERVERFailed").d("reason", "uidIsEmpty"));
	} else {
		AISDK_INFO(LX("EVENT_CONNECTED_TO_SERVER").d("uid", uid));
		auto deviceId = m_deviceInfo->getDeviceSerialNumber();
		if(m_asrRefreshConfig)
			m_asrRefreshConfig->notifyAsrRefresh(uid, m_appId, "914a80c33bda12e993559f79ae898205", deviceId);
	}
}

void AIUIAutomaticSpeechRecognizer::handleEventVadBegin() {
	// disable the timer.
	// tryEntryListeningStateOnTimer();
	m_timeoutForActivingAudioTimer.stop();
}

void AIUIAutomaticSpeechRecognizer::handleEventVadEnd() {
	setVaildVad(true);
	//m_timeoutForActivingAudioTimer.stop();
	if(!m_timeoutForThinkingTimer.isActive()) {
		if(!m_timeoutForThinkingTimer.start(
			THINKING_TIMEOUT,
			std::bind(&AIUIAutomaticSpeechRecognizer::transitionFromThinkingTimedOut, this))
			.valid()) {
			AISDK_ERROR(LX("handleEventVadBegin").d("reason", "failedToStartTransitionFromThinkingTimedOut"));
		}
	}
}

void AIUIAutomaticSpeechRecognizer::handleEventResultNLP(const std::string unparsedIntent) {
	// AISDK_DEBUG5(LX("handleEventResultNLP").d("reason", "entry").d("unparsedIntent", unparsedIntent));
	// The state should is RECOGNIZING or EXPECT SPEECH.
	if(ObserverInterface::State::RECOGNIZING != getState() && \
	 ObserverInterface::State::EXPECTING_SPEECH != getState())
		return;
	
	m_executor.submit([this, unparsedIntent]() {
		return executeNLPResult(unparsedIntent);
	});
}

void AIUIAutomaticSpeechRecognizer::handleEventResultTPP(const std::string unparsedIntent) {
	//AISDK_DEBUG5(LX("handleEventResultTPP").d("reason", "entry").d("intentLength", unparsedIntent.length()));
	if(unparsedIntent.empty()) {
		AISDK_ERROR(LX("handleEventResultTPP").d("reason", "unparsedIntentIsnullptr"));
		return;
	}
	
	m_executor.submit([this, unparsedIntent]() {
		return executeTPPResult(unparsedIntent);
	});
}

void AIUIAutomaticSpeechRecognizer::handleEventResultTTS(const std::string info, const std::string data) {
	/// AISDK_DEBUG5(LX("handleEventResultTTS").d("reason", "entry"));
	m_executor.submit([this, info, data]() {
		executeTTSResult(info, data);
	});
}

void AIUIAutomaticSpeechRecognizer::handleEventResultIAT(const std::string unparsedIntent) {
	AISDK_DEBUG5(LX("handleEventResultIAT")
		.d("reason", "entry")
		.d("intentLength", unparsedIntent.length())
		.d("intent", unparsedIntent));
	// no-op

}

void AIUIAutomaticSpeechRecognizer::onTrackChanged(utils::channel::FocusState newTrace) {
	AISDK_DEBUG5(LX("onTrackChanged").d("newTrace", newTrace));
	m_executor.submit([this, newTrace]() {
		return executeOnTrackChanged(newTrace);
	});
}

void AIUIAutomaticSpeechRecognizer::onDeregistered() {
	// Maybe should use thread.
	executeResetState();
}

void AIUIAutomaticSpeechRecognizer::preHandleDirective(std::shared_ptr<DirectiveInfo> info) {
	// default no-op
}

void AIUIAutomaticSpeechRecognizer::handleDirective(std::shared_ptr<DirectiveInfo> info) {
    if (!info) {
        AISDK_ERROR(LX("handleDirectiveFailed").d("reason", "nullDirectiveInfo"));
        return;
    }

	if (info->directive->getDomain() == EXPECT_SPEECH) {
		m_executor.submit([this, info]() { executeExpectSpeech(info); });
    } else {
        if (info->result) {
            info->result->setFailed("unknownDomainDirective");
        }
        AISDK_ERROR(LX("handleDirectiveFailed")
                        .d("reason", "unknownDomainDirective")
                        .d("domain", info->directive->getDomain()));
    }

}

void AIUIAutomaticSpeechRecognizer::cancelDirective(std::shared_ptr<DirectiveInfo> info) {
	removeDirective(info);
}

void AIUIAutomaticSpeechRecognizer::removeDirective(std::shared_ptr<DirectiveInfo> info) {
	if(info->directive && info->result)
		nlp::DomainProxy::removeDirective(info->directive->getMessageId());
}

void AIUIAutomaticSpeechRecognizer::terminate() {
	m_executor.shutdown();
	executeResetState();
	m_trackManager.reset();
	m_attachmentDocker.reset();
	m_messageConsumer.reset();
	
	if(m_aiuiAgent != NULL) {
		m_aiuiAgent->destroy();
		m_aiuiAgent = NULL;
	}
}

AIUIAutomaticSpeechRecognizer::~AIUIAutomaticSpeechRecognizer() {
	if(m_aiuiAgent != NULL) {
		m_aiuiAgent->destroy();
		m_aiuiAgent = NULL;
	}
	
	if(m_gainTune)
		m_gainTune.reset();
}

AIUIAutomaticSpeechRecognizer::AIUIAutomaticSpeechRecognizer(
	std::shared_ptr<utils::DeviceInfo> deviceInfo,
	std::shared_ptr<utils::channel::AudioTrackManagerInterface> trackManager,
	std::shared_ptr<utils::attachment::AttachmentManagerInterface> attachmentDocker,
	std::shared_ptr<dmInterface::MessageConsumerInterface> messageConsumer,
	std::shared_ptr<asr::ASRRefreshConfiguration> asrRefreshConfig,
	const std::string &appId,
	const std::string &aiuiConfigFile,
	const std::string &aiuiDir,
	const std::string &aiuiLogDir):
	m_deviceInfo{deviceInfo},
	m_trackManager{trackManager},
	m_trackState{utils::channel::FocusState::NONE},
	m_attachmentDocker{attachmentDocker},
	m_messageConsumer{messageConsumer},
	m_asrRefreshConfig{asrRefreshConfig},
	m_appId{appId},
	m_aiuiConfigFile{aiuiConfigFile}, 
	m_aiuiDir{aiuiDir}, 
	m_aiuiLogDir{aiuiLogDir},
	m_running{false},
	m_bargeIn{false},
	m_attachmentWriter{nullptr},
	m_gainTune{nullptr},
	m_utteranceSave{false} {

}
	
void AIUIAutomaticSpeechRecognizer::closeActiveAttachmentWriter() {
	m_attachmentWriter.reset();
}

bool AIUIAutomaticSpeechRecognizer::init() {
#if 1
	m_gainTune = std::make_shared<ASRGainTune>();
	if(!m_gainTune) {
		AISDK_ERROR(LX("initFailed").d("reason", "createdGainTuneFailed"));
		return false;
	}
#endif	
	if(m_aiuiDir.empty()) {
		AISDK_ERROR(LX("initFailed").d("reason", "aiuiDirIsEmpty"));
		return false;
	}

	if(m_aiuiLogDir.empty()) {
		AISDK_ERROR(LX("initFailed").d("reason", "aiuiLogDirIsEmpty"));
		return false;		
	}

	if(m_aiuiConfigFile.empty()) {
		AISDK_ERROR(LX("initFailed").d("reason", "aiuiConfigFileIsEmpty"));
		return false;
	}
	
	aiui::AIUISetting::setAIUIDir(m_aiuiDir.c_str());
	aiui::AIUISetting::initLogger(m_aiuiLogDir.c_str());
	// Read the default aiui.cfg file.
	std::string defaultConfig = FileUtil::readFileAsString(m_aiuiConfigFile);
	if(defaultConfig.empty()) {
		AISDK_ERROR(LX("initFailed").d("reason", "readAIUIConfigFileError"));
		return false;
	}
	/**
	 * This is a old JSON API already be deprecated.
	 */
	//Json::Reader reader;
	Json::CharReaderBuilder readerBuilder;
	JSONCPP_STRING errs;
	Json::Value root;
	std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());
	if(!reader->parse(defaultConfig.c_str(), defaultConfig.c_str()+defaultConfig.length(), &root, &errs)) {
		AISDK_ERROR(LX("initFailed").d("reason", "parseAIUIJsonFailed."));
		return false;
	}
	
	Json::Value appidJson;
	appidJson["appid"] = m_appId;
	root["login"] = appidJson;

	Json::Value userParamsJson;
	userParamsJson["deviceId"] = m_deviceInfo->getDeviceSerialNumber();
	root["userparams"] = userParamsJson;

	// Set interactive scene - main or test
	if(!m_deviceInfo->getDeviceScene()) {
		Json::Value sceneJson;
		sceneJson["scene"] = std::string("test");
		root["global"] = sceneJson;
		AISDK_DEBUG5(LX("init").d("scene", "test"));
	}
	
	/**
	 * This is a old JSON API already be deprecated.
	 */
	Json::StreamWriterBuilder writerBuilder;
	std::unique_ptr<Json::StreamWriter> writer(writerBuilder.newStreamWriter());
	std::ostringstream os;
	writer->write(root, &os);

	//  注：该方法总是返回非空对象，非空并不代表创建过程中无错误发生。- from Iflytek aiui
	m_aiuiAgent = aiui::IAIUIAgent::createAgent(os.str().c_str(), this);

	// Check utterance save flags.
	m_utteranceSave = m_deviceInfo->getUtteranceSave();

	setObserver(shared_from_this());
	m_running = true;
	return true;
}

void AIUIAutomaticSpeechRecognizer::executeExpectSpeech(std::shared_ptr<DirectiveInfo> info) {
    if (info && info->isCancelled) {
        AISDK_DEBUG(LX("expectSpeechIgnored").d("reason", "isCancelled"));
        return ;
    }
	auto state = getState();
    if (state != ObserverInterface::State::IDLE && \
        state != ObserverInterface::State::TIMEOUT && \
		state != ObserverInterface::State::BUSY) {
        static const char* errorMessage = "ExpectSpeech only allowed in IDLE or BUSY state.";
        if (info->result) {
            info->result->setFailed(errorMessage);
        }
        removeDirective(info);
        AISDK_ERROR(LX("executeExpectSpeechFailed")
                        .d("reason", "invalidState")
                        .d("expectedState", "IDLE/BUSY")
                        .d("state", state));
        return;
    }

	if (info->result) {
		info->result->setCompleted();
	}
	removeDirective(info);

	if(m_audioProvider) {
		auto startWithNewData = true;
		auto reader = m_audioProvider->createReader(Reader::Policy::NONBLOCKING, startWithNewData);
		auto begin = reader->tell();
		executeRecognize(m_audioProvider, begin, INVALID_INDEX, ObserverInterface::State::EXPECTING_SPEECH);
	}
	
}

bool AIUIAutomaticSpeechRecognizer::executeRecognize(
	std::shared_ptr<utils::sharedbuffer::SharedBuffer> stream,
    utils::sharedbuffer::SharedBuffer::Index begin,
    utils::sharedbuffer::SharedBuffer::Index keywordEnd,
    ObserverInterface::State state) {

	// Check network state.
	if(!m_deviceInfo->isConnected()) {
		AISDK_WARN(LX("executeRecognizeFailed").d("reason", "networkIsDisconnected"));
		executeResetState();
		return false;
	}
	
	// Acquire channel prority.
	if (!m_trackManager->acquireChannel(CHANNEL_NAME, shared_from_this(), CHANNEL_INTERFACE_NAME)) {
		AISDK_ERROR(LX("executeRecognizeFailed").d("reason", "Unable to acquire channel"));
		executeResetState();
		return false;
    }

	// Terminate tts convert.
	executeCancelTextToSpeech();
	
	// Should ensure writer already close.
	closeActiveAttachmentWriter();

	if(m_readerThread.joinable()) {
		m_readerThread.join();
	}
	
	// Formally update state now.
	m_bargeIn = false;
	setVaildVad(false);
	setState(state);

	// Creating new @c Reader.
	m_reader = stream->createReader(Reader::Policy::BLOCKING);
	
    // Record provider as the last-used Audio Provider so it can be used in the event of an ExpectSpeech domain.
	m_audioProvider = stream;

	/**
	 * It must be ensured that there is a timeout mechanism when there is no response for 
	 * a long time during the transition from recognition to thinking.
	 */
	tryEntryIdleStateOnTimer();
	
	// Formally, Start sending wakeup event to AIUI engine.
	aiui::IAIUIMessage * wakeupMsg = aiui::IAIUIMessage::create(aiui::AIUIConstant::CMD_WAKEUP);
	m_aiuiAgent->sendMessage(wakeupMsg);
	wakeupMsg->destroy();
	
	return true;
}

bool AIUIAutomaticSpeechRecognizer::executeNLPResult(const std::string unparsedIntent) {
	Json::CharReaderBuilder readerBuilder;
	JSONCPP_STRING errs;
	Json::Value root;
	std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());
	if (!reader->parse(unparsedIntent.c_str(), unparsedIntent.c_str()+unparsedIntent.length(), &root, &errs)) {
		AISDK_ERROR(LX("executeNLPResultFailed").d("reason", "parseInfoError"));
		return false;
	}
#if 0
	if(!m_timeoutForThinkingTimer.isActive()) {
		if(!m_timeoutForThinkingTimer.start(
			THINKING_TIMEOUT,
			std::bind(&AIUIAutomaticSpeechRecognizer::transitionFromThinkingTimedOut, this))
			.valid()) {
			AISDK_ERROR(LX("executeNLPResult").d("reason", "failedToStartTransitionFromThinkingTimedOut"));
		}
	}
#endif
	Json::Value intent = root["intent"];
	std::string  sid = intent["sid"].asString();
	if(sid.empty()) {
		AISDK_ERROR(LX("executeNLPResultFailed").d("reason", "notFoundSID."));
		return false;
	}

	m_sessionId = sid;
	AISDK_INFO(LX("executeNLPResult").d("newSID", sid));

	setState(ObserverInterface::State::BUSY);

	return true;
}

void AIUIAutomaticSpeechRecognizer::intentRepackingConsumeMessage(const std::string &intent) {
	Json::CharReaderBuilder readerBuilder;
	JSONCPP_STRING errs;
	Json::Value root;
	std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());	
	// Start parsing.
	if (!reader->parse(intent.c_str(), intent.c_str()+intent.length(), &root, &errs)) {
		AISDK_ERROR(LX("intentRepackingConsumeMessageFailed").d("reason", "parseIntentError").d("intent", intent));
		return;
	}

	// Parsing domain node.
	std::string domain = root["domain"].asString();
	if(domain.empty()) {
		AISDK_ERROR(LX("executeTPPResultFailed").d("reason", "notFoundDomainNode."));
		 return;
	}

	bool repacking = false;
	// Compare domain whether is equal.
	if(domain == DOMAIN_ALARM || domain == DOMAIN_SCHEDULE || \
		domain == DOMAIN_PLAYCONTROL || domain == DOMAIN_VOLUME) {
		repacking = true;
	} else {
		Json::Value data = root["data"];
		auto resource = data["resource"].asBool();
		if(resource) {
			repacking = true;
		}
	}

	// Should repacking to consume message.
	if(repacking) {
		Json::StreamWriterBuilder writerBuilder;
		std::unique_ptr<Json::StreamWriter> writer(writerBuilder.newStreamWriter());
		std::ostringstream os;
		root["domain"] = "chat";
		writer->write(root, &os);
		// AISDK_DEBUG5(LX("intentRepackingConsumeMessage").d("newIntent", os.str()));
		m_messageConsumer->consumeMessage(m_sessionId, os.str());
	}

}

bool AIUIAutomaticSpeechRecognizer::executeTPPResult(
	const std::string intent,
	std::shared_ptr<utils::attachment::AttachmentWriter> writer) {
	Json::CharReaderBuilder readerBuilder;
	JSONCPP_STRING errs;
	Json::Value root;
	std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());	
	AISDK_DEBUG(LX("executeTPPResult").d("reason", "entry").d("intent", intent));
	// Start parsing.
	if (!reader->parse(intent.c_str(), intent.c_str()+intent.length(), &root, &errs)) {
		AISDK_ERROR(LX("handleEventResultTPPFailed").d("reason", "parseIntentError").d("intent", intent));
		return false;
	}

	Json::Value data = root["data"];
	Json::Value empty;
	Json::Value answer = data.get("answer", empty);
	if(answer.empty()) {
		AISDK_ERROR(LX("executeTPPResultFailed").d("reason", "notFoundAnswerNode."));
		 return false;
	}

	// The BUSY state will be allowed to continue working.
	if(ObserverInterface::State::BUSY != getState())
		return false;

	auto expectSpeech = data["session"].asBool();

	// We should close the last writer before starting a new write.
	closeActiveAttachmentWriter();
	if(m_attachmentDocker && !m_attachmentWriter) {
		// Default Policy is NONBLOCKING.
		m_attachmentWriter = m_attachmentDocker->createWriter(m_sessionId);
		if (!m_attachmentWriter) {
            AISDK_ERROR(LX("executeTPPResultFailed")
                            .d("reason", "createWriterFailed")
                            .d("attachmentId", m_sessionId));
        }
	}

	m_timeoutForThinkingTimer.stop();
	std::string text(answer.asString());
	AISDK_INFO(LX("executeTPPResult").d("text", text));
	executeTextToSpeech(text);

	intentRepackingConsumeMessage(intent);
	
	m_messageConsumer->consumeMessage(m_sessionId, intent);
	if(expectSpeech) {
		AISDK_DEBUG0(LX("executeTPPResult").d("expectSpeech", expectSpeech?"true":"false"));
		std::stringstream expectSpeech;
		expectSpeech << DOMAIN_EXPECT_SPEECH_FORMAT;
		AISDK_DEBUG5(LX("executeTPPResult").d("ExpectSpeech", expectSpeech.str()));
		m_messageConsumer->consumeMessage(MESSAGE_ID_COMBINING_SUBSTRING+m_sessionId, expectSpeech.str());
	}
	
	return true;
}

AIUIAutomaticSpeechRecognizer::TTSDataWriteStatus 
AIUIAutomaticSpeechRecognizer::writeDataToAttachment(const char* buffer, size_t size) {
	if(!m_attachmentWriter) {
		AISDK_ERROR(LX("writeDataToAttachmentFailed").d("reason", "nullAttachmentWriter"));
		return AIUIAutomaticSpeechRecognizer::TTSDataWriteStatus::ERROR;
	}

	auto writeStatus = utils::attachment::AttachmentWriter::WriteStatus::OK;
    auto numWritten = m_attachmentWriter->write(const_cast<char*>(buffer), size, &writeStatus);

    // The underlying memory was closed elsewhere.
    if (utils::attachment::AttachmentWriter::WriteStatus::CLOSED == writeStatus) {
        AISDK_WARN(LX("writeDataToAttachmentFailed").d("reason", "attachmentWriterIsClosed"));
        return AIUIAutomaticSpeechRecognizer::TTSDataWriteStatus::ERROR;
    }

    // A low-level error with the Attachment occurred.
    if (utils::attachment::AttachmentWriter::WriteStatus::ERROR_BYTES_LESS_THAN_WORD_SIZE == writeStatus ||
        utils::attachment::AttachmentWriter::WriteStatus::ERROR_INTERNAL == writeStatus) {
        AISDK_ERROR(LX("writeDataToAttachmentFailed").d("reason", "attachmentWriterInternalError"));
        return AIUIAutomaticSpeechRecognizer::TTSDataWriteStatus::ERROR;
    }

    // We should are blocked on a slow reader. 
    // TODO:

    // A final sanity check to ensure we wrote the data we intended to.
    if (utils::attachment::AttachmentWriter::WriteStatus::OK == writeStatus && numWritten != size) {
        AISDK_ERROR(LX("writeDataToAttachmentFailed").d("reason", "writeTruncated"));
        return AIUIAutomaticSpeechRecognizer::TTSDataWriteStatus::ERROR;
    }

    return AIUIAutomaticSpeechRecognizer::TTSDataWriteStatus::OK;
}

bool AIUIAutomaticSpeechRecognizer::executeTTSResult(const std::string info, const std::string data) {
	if(info.empty()) {
		AISDK_ERROR(LX("executeTTSResultFailed").d("reason", "infoIsEmpty"));
		return false;
	}

	if(data.empty()) {
		AISDK_ERROR(LX("executeTTSResultFailed").d("reason", "dataIsEmpty"));
		return false;
	}
	
	Json::CharReaderBuilder readerBuilder;
	JSONCPP_STRING errs;
	Json::Value root;
	std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());
	if (!reader->parse(info.c_str(), info.c_str()+info.length(), &root, &errs)) {
		AISDK_ERROR(LX("executeTTSResultFailed").d("reason", "parseInfoError"));
		return false;
	}
	
	Json::Value dataNode = root["data"][0];
	Json::Value content = (dataNode["content"])[0];
	int dts = content["dts"].asInt();
	//AISDK_DEBUG0(LX("executeTTSResult").d("dts", dts));
	std::string errorinfo = content["error"].asString();
	if(dts == 2 && errorinfo == "AIUI DATA NULL") {
		AISDK_DEBUG5(LX("executeTTSResult").d("reason", errorinfo));
	} else if (3 == dts) {
		AISDK_INFO(LX("executeTTSResult").d("dts", dts).d("length", data.length()).d("expected", "Reserve"));
	#ifdef TTS_RECORD
		if(!recoder.is_open()) {
			recoder.clear();
			recoder.open(DEF_RECODER);
		}
		recoder.write((const char*) data.c_str(), data.length());
		recoder.close();
	#endif
		// Start writing tts data to a specail attachment docker.
		writeDataToAttachment(data.c_str(), data.length());
		closeActiveAttachmentWriter();
	} else {
		if (0 == dts) {
			AISDK_INFO(LX("executeTTSResult").d("dts", "Started0"));
		#ifdef TTS_RECORD
			if(!recoder.is_open()) {
				recoder.clear();
				recoder.open(DEF_RECODER);
			}
		#endif	
		}
	#ifdef TTS_RECORD
		recoder.write((const char*) data.c_str(), data.length());
	#endif
		// Start writing tts data to a specail attachment docker.
		writeDataToAttachment(data.c_str(), data.length());
	
		if (2 == dts) {
			AISDK_INFO(LX("executeTTSResult").d("dts", "Finished2"));
		#ifdef TTS_RECORD	
			recoder.close();
		#endif
			closeActiveAttachmentWriter();
		}
	}
	
	return true;
}

void AIUIAutomaticSpeechRecognizer::sendStreamProcessing() {
	std::vector<int16_t> audioDataToPush(640); // 640*2 = 1280 = 80ms
	ssize_t wordsRead;
	std::fstream fs;

	if(m_utteranceSave) {
		fs.open("/tmp/utterance.pcm", std::fstream::out | std::fstream::app);
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	// Seek keyword begin position.
	m_reader->seek(0, Reader::Reference::BEFORE_WRITER);

	do {
		bool didErrorOccur = false;
		// Start read data.
		wordsRead = readFromStream(
				m_reader,
				audioDataToPush.data(),
				audioDataToPush.size(),
				TIMEOUT_FOR_READ_CALLS,
				&didErrorOccur);
		// Error occurrence maybe reader close.
		if(didErrorOccur) {
			break;
		} else if (wordsRead == Reader::Error::OVERRUN) {
			// no-op
			
		} else if(wordsRead == Reader::Error::TIMEDOUT) {
			break;
		} else if(wordsRead > 0) {
			/**
			 * 申请的内存会在sdk内部释放
			 */
			// Calculate the actual length(in bytes).
			auto length = wordsRead*sizeof(*audioDataToPush.data());
			aiui::Buffer* buffer = aiui::Buffer::alloc(length);
			void *pbuf8 = audioDataToPush.data();
			if(m_gainTune)
				m_gainTune->adjustGain(audioDataToPush.data(), length, 0.5);
			
			memcpy(buffer->data(), pbuf8, length);
		
			if (m_utteranceSave && fs.good()) {
				fs.write(static_cast<char *>(pbuf8), length);
			}

			// Start writing data to AIUI Cloud.
			aiui::IAIUIMessage * writeMsg = 
			aiui::IAIUIMessage::create(aiui::AIUIConstant::CMD_WRITE,
								0, 
								0,
								"data_type=audio,sample_rate=16000",
								buffer);
			m_aiuiAgent->sendMessage(writeMsg);
			writeMsg->destroy();
			//usleep(10 * 1000);
		}
	} while(!isVaildVad());

	//if(m_utteranceSave)
		fs.close();

	if(!m_bargeIn) {
	// Notify AIUI Cloud to terminate data writing.
	aiui::IAIUIMessage * stopWrite = 
	aiui::IAIUIMessage::create(aiui::AIUIConstant::CMD_STOP_WRITE,
								0,
								0,
								"data_type=audio,sample_rate=16000");
	

	m_aiuiAgent->sendMessage(stopWrite);
	stopWrite->destroy();
	}else {
		// CMD_RESET_WAKEUP
		//IAIUIMessage * resetMsg = aiui::IAIUIMessage::create(aiui::AIUIConstant::CMD_RESET);
		//m_aiuiAgent->sendMessage(resetMsg);
		//resetMsg->destroy();
	}
	
	if(m_reader) {
		m_reader->close();
		m_reader.reset();
	}

	AISDK_INFO(LX("sendStreamProcessing").d("reason", "Exit"));
}

bool AIUIAutomaticSpeechRecognizer::executeTextToSpeech(
	const std::string text,
	std::shared_ptr<utils::attachment::AttachmentWriter> writer) {

	if(!writer && !m_attachmentWriter) {
		AISDK_ERROR(LX("executeTextToSpeechFailed").d("reason", "writerIsnullptr"));
		return false;
	}
	
	// update lastest writer @c AttachmentWriter.
	if(writer) {
		m_attachmentWriter = writer;
	}

	aiui::Buffer* textData = aiui::Buffer::alloc(text.length());
	text.copy((char*) textData->data(), text.length());

	/*
	 arg1取值说明：
		START	开始合成	合成发音人，语速语调等
		CANCEL	取消合成	无
		PAUSE	暂停播放	无
		RESUME	恢复播放	无
	*/

	/*
	合成参数示例：
	String params = "vcn=xiaoyan,speed=50,pitch=50,volume=50"

	参数字段说明：
		vcn	发音人，如xiaoyan
		speed	速度，0-100
		pitch	语调，0-100
		volume	音量，0-100
		ent	引擎，默认aisound，如果需要较好的效果，可设置成xtts
	*/
	std::string paramStr = "vcn=x_chongchong";
	paramStr += ",speed=50";
	paramStr += ",pitch=50";
	paramStr += ",volume=100";
	paramStr += ",ent=xtts";
	
	aiui::IAIUIMessage * ttsMsg = aiui::IAIUIMessage::create(aiui::AIUIConstant::CMD_TTS,
		aiui::AIUIConstant::START, 0, paramStr.c_str(), textData);

	m_aiuiAgent->sendMessage(ttsMsg);

	ttsMsg->destroy();
	
	return true;
}

bool AIUIAutomaticSpeechRecognizer::executeCancelTextToSpeech() {
	aiui::IAIUIMessage * ttsMsg = aiui::IAIUIMessage::create(aiui::AIUIConstant::CMD_TTS,
		aiui::AIUIConstant::CANCEL, 0, "", NULL);

	m_aiuiAgent->sendMessage(ttsMsg);

	ttsMsg->destroy();

	return true;
}

void AIUIAutomaticSpeechRecognizer::executeOnTrackChanged(utils::channel::FocusState newTrace) {
	// Note new focus state.
	{
		std::lock_guard<std::mutex> lock(m_bargeMutex);
	    m_trackState = newTrace;
	}
	
    // If we're losing focus, stop using the channel.
	if(newTrace != utils::channel::FocusState::FOREGROUND) {
		AISDK_DEBUG(LX("executeOnTrackChanged").d("reason", "Lost focus"));
		executeResetState();
		return;
	}
}

void AIUIAutomaticSpeechRecognizer::releaseForegroundTrace() {
	if(m_trackState != utils::channel::FocusState::NONE)
    	m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
}

void AIUIAutomaticSpeechRecognizer::executeResetState() {
	if(m_reader)
		m_reader->close();

	if(m_trackState != utils::channel::FocusState::NONE) {
		AISDK_DEBUG5(LX("executeResetState").d("reason", "releaseChannel"));
		m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
		m_trackState = utils::channel::FocusState::NONE;
	}

	m_sessionId.clear();

	m_timeoutForActivingAudioTimer.stop();
	m_timeoutForListeningTimer.stop();
	m_timeoutForThinkingTimer.stop();
	
	if(ObserverInterface::State::IDLE != getState())
		setState(ObserverInterface::State::IDLE);

	if(m_bargeIn) {
		AISDK_DEBUG5(LX("executeResetState").d("reason", "notifyBargeIn"));
		m_conditionBarge.notify_one();
	}
}

void AIUIAutomaticSpeechRecognizer::transitionFromThinkingTimedOut() {
	m_executor.submit([this]() {
		AISDK_DEBUG0(LX("transitionFromThinkingTimedOut"));
		setVaildVad(true);
		setState(ObserverInterface::State::TIMEOUT);
		executeResetState();
	});
}

void AIUIAutomaticSpeechRecognizer::transitionFromListeningTimedOut() {
	m_executor.submit([this]() {
		AISDK_DEBUG0(LX("transitionFromListeningTimedOut"));
		setVaildVad(true);
	});
}

void AIUIAutomaticSpeechRecognizer::transitionFromActivingTimedOut() {
	m_executor.submit([this]() {
		AISDK_DEBUG0(LX("transitionFromActivingTimedOut"));
		setVaildVad(true);
		setState(ObserverInterface::State::TIMEOUT);
		executeResetState();
	});
}

void AIUIAutomaticSpeechRecognizer::tryEntryListeningStateOnTimer() {
	m_timeoutForActivingAudioTimer.stop();
	if(!m_timeoutForListeningTimer.start(
		LISTENG_TIMEOUT,
		std::bind(&AIUIAutomaticSpeechRecognizer::transitionFromListeningTimedOut, this))
		.valid()) {
		AISDK_ERROR(LX("tryEntryListeningStateOnTimer").d("reason", "failedToStartTryEntryListeningStateOnTimer"));
	}
}

void AIUIAutomaticSpeechRecognizer::tryEntryIdleStateOnTimer() {
	if(!m_timeoutForActivingAudioTimer.start(
		ACTIVING_TIMEOUT,
		std::bind(&AIUIAutomaticSpeechRecognizer::transitionFromActivingTimedOut, this))
		.valid()) {
		AISDK_ERROR(LX("tryEntryIdleStateOnTimer").d("reason", "failedToStartTryEntryIdleStateOnTimer"));
	}
}

} // namespace aiuiEngine
} // namespace asr
} // namespace aisdk
