/*
 * Copyright 2019 gm its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <iostream>

#include "SpeechSynthesizer/SpeechSynthesizer.h"

namespace aisdk {
namespace domain {
namespace speechSynthesizer {

using namespace utils::mediaPlayer;
using namespace utils::channel;
using namespace dmInterface;

/// The name of the @c AudioTrackManager channel used by the @c SpeechSynthesizer.
static const std::string CHANNEL_NAME = AudioTrackManagerInterface::DIALOG_CHANNEL_NAME;

/// The name of DomainProxy and SpeechChat handler interface
//static const std::string SPEECHCHAT{"SpeechChat"};

/// The name of the @c SafeShutdown
static const std::string SPEECHNAME{"SpeechSynthesizer"};

/// The duration to wait for a state change in @c onTrackChanged before failing.
static const std::chrono::seconds STATE_CHANGE_TIMEOUT{5};

/// The duration to start playing offset position.
static const std::chrono::milliseconds DEFAULT_OFFSET{0};

std::shared_ptr<SpeechSynthesizer> SpeechSynthesizer::create(
	std::shared_ptr<MediaPlayerInterface> mediaPlayer,
	std::shared_ptr<AudioTrackManagerInterface> trackManager,
	std::shared_ptr<utils::dialogRelay::DialogUXStateRelay> dialogUXStateRelay){
	if(!mediaPlayer){
		std::cout << "SpeechCreationFailed:reason:mediaPlayerNull" << std::endl;
		return nullptr;
	}

	if(!trackManager){
		std::cout << "SpeechCreationFailed:reason::trackManagerNull" << std::endl;
		return nullptr;
	}

	if(!dialogUXStateRelay){
		std::cout << "SpeechCreationFailed:reason:dialogUXStateRelayNull" << std::endl;
		return nullptr;
	}

	auto instance = std::shared_ptr<SpeechSynthesizer>(new SpeechSynthesizer(mediaPlayer, trackManager));
	if(!instance){
		std::cout << "SpeechCreationFailed:reason:NewSpeechSynthesizerFailed." << std::endl;
		return nullptr;
	}
	instance->init();

	dialogUXStateRelay->addObserver(instance);

	return instance;
}

void SpeechSynthesizer::onDeregistered() {
	// default no-op
}

void SpeechSynthesizer::preHandleDirective(std::shared_ptr<DirectiveInfo> info) {
	std::cout << "preHandleDirective: messageId: " << info->directive->getMessageId() << std::endl;
    m_executor.submit([this, info]() { executePreHandle(info); });
}

void SpeechSynthesizer::handleDirective(std::shared_ptr<DirectiveInfo> info) {
	std::cout << "handleDirective: messageId: " << info->directive->getMessageId() << std::endl;
    m_executor.submit([this, info]() { executeHandle(info); });
}

void SpeechSynthesizer::cancelDirective(std::shared_ptr<DirectiveInfo> info) {
	std::cout << "handleDirective: messageId: " << info->directive->getMessageId() << std::endl;
    m_executor.submit([this, info]() { executeCancel(info); });
}

void SpeechSynthesizer::onTrackChanged(FocusState newTrace) {
	std::cout << "onTrackChanged: newTrace: " << newTrace << std::endl;
    std::unique_lock<std::mutex> lock(m_mutex);
    m_currentFocus = newTrace;
    setDesiredStateLocked(newTrace);
    if (m_currentState == m_desiredState) {
        return;
    }
	
    // Set intermediate state to avoid being considered idle
    switch (newTrace) {
        case FocusState::FOREGROUND:
            setCurrentStateLocked(SpeechSynthesizerObserverInterface::SpeechSynthesizerState::GAINING_FOCUS);
            break;
        case FocusState::BACKGROUND:
            setCurrentStateLocked(SpeechSynthesizerObserverInterface::SpeechSynthesizerState::LOSING_FOCUS);
            break;
        case FocusState::NONE:
            // We do not care of other track focus states yet
            break;
    }

	auto messageId = (m_currentInfo && m_currentInfo->directive) ? m_currentInfo->directive->getMessageId() : "";
    m_executor.submit([this]() { executeStateChange(); });

	// Block until we achieve the desired state.
    if (m_waitOnStateChange.wait_for(
            lock, STATE_CHANGE_TIMEOUT, [this]() { return m_currentState == m_desiredState; })) {
		std::cout << "onTrackChangedSuccess" << std::endl;
    } else {
		std::cout << "onFocusChangeFailed:reason:stateChangeTimeout:messageId: " << messageId << std::endl;
        if (m_currentInfo) {
            lock.unlock();
			reportExceptionFailed(m_currentInfo, "stateChangeTimeout");
        }
    }
}

void SpeechSynthesizer::onPlaybackStarted(SourceId id) {
	std::cout << "onPlaybackStarted:callbackSourceId: " << id << std::endl;
	
    if (id != m_mediaSourceId) {
		std::cout << "ERR: queueingExecutePlaybackStartedFailed:reason:mismatchSourceId:callbackSourceId: " << id << ":sourceId: "<< m_mediaSourceId << std::endl;
		
        m_executor.submit([this] {
            executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "executePlaybackStartedFailed");
        });
    } else {
        m_executor.submit([this]() { executePlaybackStarted(); });
    }

}

void SpeechSynthesizer::onPlaybackFinished(SourceId id) {
	std::cout << "onPlaybackFinished:callbackSourceId: " << id << std::endl;

    if (id != m_mediaSourceId) {
		std::cout << "ERR: queueingExecutePlaybackFinishedFailed:reason:mismatchSourceId:callbackSourceId: " << id << ":sourceId: "<< m_mediaSourceId << std::endl;
        m_executor.submit([this] {
            executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "executePlaybackFinishedFailed");
        });
    } else {
        m_executor.submit([this]() { executePlaybackFinished(); });
    }
}

void SpeechSynthesizer::onPlaybackError(
	SourceId id,
	const utils::mediaPlayer::ErrorType& type,
	std::string error) {
	std::cout << "onPlaybackError:callbackSourceId: " << id << std::endl;
	
    m_executor.submit([this, type, error]() { executePlaybackError(type, error); });
}

void SpeechSynthesizer::onPlaybackStopped(SourceId id) {
	std::cout << "onPlaybackStopped:callbackSourceId: " << id << std::endl;
    onPlaybackFinished(id);
}

void SpeechSynthesizer::addObserver(std::shared_ptr<SpeechSynthesizerObserverInterface> observer) {
	std::cout << __func__ << ":addObserver:observer: " << observer.get() << std::endl;
    m_executor.submit([this, observer]() { m_observers.insert(observer); });
}

void SpeechSynthesizer::removeObserver(std::shared_ptr<SpeechSynthesizerObserverInterface> observer) {
	std::cout << __func__ << ":removeObserver:observer: " << observer.get() << std::endl;	
    m_executor.submit([this, observer]() { m_observers.erase(observer); }).wait();
}

std::string SpeechSynthesizer::getHandlerName() const {
	return m_handlerName;
}

void SpeechSynthesizer::doShutdown() {
	std::cout << "doShutdown" << std::endl;
	m_speechPlayer->setObserver(nullptr);
	{
        std::unique_lock<std::mutex> lock(m_mutex);
        if (SpeechSynthesizerObserverInterface::SpeechSynthesizerState::PLAYING == m_currentState ||
            SpeechSynthesizerObserverInterface::SpeechSynthesizerState::PLAYING == m_desiredState) {
            m_desiredState = SpeechSynthesizerObserverInterface::SpeechSynthesizerState::FINISHED;
            stopPlaying();
            m_currentState = SpeechSynthesizerObserverInterface::SpeechSynthesizerState::FINISHED;
            lock.unlock();
            releaseForegroundTrace();
        }
    }
	
    {
        std::lock_guard<std::mutex> lock(m_chatInfoQueueMutex);
        for (auto& info : m_chatInfoQueue) {
            if (info.get()->result) {
                info.get()->result->setFailed("SpeechSynthesizerShuttingDown");
            }
            removeChatDirectiveInfo(info.get()->directive->getMessageId());
            removeDirective(info.get()->directive->getMessageId());
        }
    }
	
    m_executor.shutdown();
    m_speechPlayer.reset();
    m_waitOnStateChange.notify_one();
    m_trackManager.reset();
    m_observers.clear();

}

SpeechSynthesizer::ChatDirectiveInfo::ChatDirectiveInfo(
	std::shared_ptr<nlp::DomainProxy::DirectiveInfo> directiveInfo) :
	directive{directiveInfo->directive},
	result{directiveInfo->result},
	sendCompletedMessage{false} {
}
	
void SpeechSynthesizer::ChatDirectiveInfo::clear() {
    sendCompletedMessage = false;
}

SpeechSynthesizer::SpeechSynthesizer(
	std::shared_ptr<MediaPlayerInterface> mediaPlayer,
	std::shared_ptr<AudioTrackManagerInterface> trackManager) :
	DomainProxy{SPEECHNAME},
	SafeShutdown{SPEECHNAME},
	m_handlerName{SPEECHNAME},
	m_speechPlayer{mediaPlayer},
	m_trackManager{trackManager},
	m_mediaSourceId{MediaPlayerInterface::ERROR},
	m_currentState{SpeechSynthesizerObserverInterface::SpeechSynthesizerState::FINISHED},
	m_desiredState{SpeechSynthesizerObserverInterface::SpeechSynthesizerState::FINISHED},
	m_currentFocus{FocusState::NONE},
	m_isAlreadyStopping{false} {
}

void SpeechSynthesizer::init() {
    m_speechPlayer->setObserver(shared_from_this());
}

void SpeechSynthesizer::executePreHandleAfterValidation(std::shared_ptr<ChatDirectiveInfo> info) {
	/// To-Do parse tts url and insert chatInfo map
	/// ...
	/// ...
	
	// If everything checks out, add the chatInfo to the map.
    if (!setChatDirectiveInfo(info->directive->getMessageId(), info)) {
		std::cout << "executePreHandleFailed:reason:prehandleCalledTwiceOnSameDirective:messageId: " << info->directive->getMessageId() << std::endl;
    }
}

void SpeechSynthesizer::executeHandleAfterValidation(std::shared_ptr<ChatDirectiveInfo> info) {
    m_currentInfo = info;
    if (!m_trackManager->acquireChannel(CHANNEL_NAME, shared_from_this(), SPEECHNAME)) {
        static const std::string message = std::string("Could not acquire ") + CHANNEL_NAME + " for " + SPEECHNAME;
		std::cout << "executeHandleFailed:reason:CouldNotAcquireChannel:messageId: " << m_currentInfo->directive->getMessageId() << std::endl;
        reportExceptionFailed(info, message);
    }
}

void SpeechSynthesizer::executePreHandle(std::shared_ptr<DirectiveInfo> info) {
	std::cout << "executePreHandle:messageId" << info->directive->getMessageId() << std::endl;
    auto chatInfo = validateInfo("executePreHandle", info);
    if (!chatInfo) {
		std::cout << "executePreHandleFailed:reason:invalidDirectiveInfo" << std::endl;
        return;
    }
    executePreHandleAfterValidation(chatInfo);
}

void SpeechSynthesizer::executeHandle(std::shared_ptr<DirectiveInfo> info) {
	std::cout << "executeHandle:messageId" << info->directive->getMessageId() << std::endl;
    auto chatInfo = validateInfo("executeHandle", info);
    if (!chatInfo) {
		std::cout << "executeHandleFailed:reason:invalidDirectiveInfo" << std::endl;
        return;
    }
    addToDirectiveQueue(chatInfo);
}

void SpeechSynthesizer::executeCancel(std::shared_ptr<DirectiveInfo> info) {
	std::cout << "executeCancel:messageId" << info->directive->getMessageId() << std::endl;
    auto chatInfo = validateInfo("executeCancel", info);
    if (!chatInfo) {
		std::cout << "executeCancelFailed:reason:invalidDirectiveInfo" << std::endl;
        return;
    }
    if (chatInfo != m_currentInfo) {
        chatInfo->clear();
        removeChatDirectiveInfo(chatInfo->directive->getMessageId());
        {
            std::lock_guard<std::mutex> lock(m_chatInfoQueueMutex);
            for (auto it = m_chatInfoQueue.begin(); it != m_chatInfoQueue.end(); it++) {
                if (chatInfo->directive->getMessageId() == it->get()->directive->getMessageId()) {
                    it = m_chatInfoQueue.erase(it);
                    break;
                }
            }
        }
        removeDirective(chatInfo->directive->getMessageId());
        return;
    }
	
    std::unique_lock<std::mutex> lock(m_mutex);
    if (SpeechSynthesizerObserverInterface::SpeechSynthesizerState::FINISHED != m_desiredState) {
        m_desiredState = SpeechSynthesizerObserverInterface::SpeechSynthesizerState::FINISHED;
        if (SpeechSynthesizerObserverInterface::SpeechSynthesizerState::PLAYING == m_currentState ||
            SpeechSynthesizerObserverInterface::SpeechSynthesizerState::GAINING_FOCUS == m_currentState) {
            lock.unlock();
            if (m_currentInfo) {
                m_currentInfo->sendCompletedMessage = false;
            }
            stopPlaying();
        }
    }

}

void SpeechSynthesizer::executeStateChange() {
    SpeechSynthesizerObserverInterface::SpeechSynthesizerState newState;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        newState = m_desiredState;
    }

	std::cout << "executeStateChange:newState: " << newState << std::endl;
    switch (newState) {
        case SpeechSynthesizerObserverInterface::SpeechSynthesizerState::PLAYING:
            if (m_currentInfo) {
                m_currentInfo->sendCompletedMessage = true;
            }
			// Trigger play
            startPlaying();
            break;
        case SpeechSynthesizerObserverInterface::SpeechSynthesizerState::FINISHED:
			// Trigger stop
            stopPlaying();
            break;
        case SpeechSynthesizerObserverInterface::SpeechSynthesizerState::GAINING_FOCUS:
        case SpeechSynthesizerObserverInterface::SpeechSynthesizerState::LOSING_FOCUS:
            // Nothing to do
            break;
    }
}

void SpeechSynthesizer::executePlaybackStarted() {
	std::cout << "executePlaybackStarted." << std::endl;
	
    if (!m_currentInfo) {
		std::cout << "executePlaybackStartedIgnored:reason:nullptrDirectiveInfo" << std::endl;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
		/// Set current state @c m_currentState to PLAYING to specify device alreay start to playback.
        setCurrentStateLocked(SpeechSynthesizerObserverInterface::SpeechSynthesizerState::PLAYING);
    }
	
	/// wakeup condition wait
    m_waitOnStateChange.notify_one();
}

void SpeechSynthesizer::executePlaybackFinished() {
	std::cout << "executePlaybackFinished." << std::endl;

    if (!m_currentInfo) {
		std::cout << "executePlaybackFinishedIgnored:reason:nullptrDirectiveInfo" << std::endl;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        setCurrentStateLocked(SpeechSynthesizerObserverInterface::SpeechSynthesizerState::FINISHED);
    }
    m_waitOnStateChange.notify_one();

    if (m_currentInfo->sendCompletedMessage) {
        setHandlingCompleted();
    }
	
    resetCurrentInfo();
	
    {
        std::lock_guard<std::mutex> lock_guard(m_chatInfoQueueMutex);
        m_chatInfoQueue.pop_front();
        if (!m_chatInfoQueue.empty()) {
            executeHandleAfterValidation(m_chatInfoQueue.front());
        }
    }
	
    resetMediaSourceId();

}

void SpeechSynthesizer::executePlaybackError(const utils::mediaPlayer::ErrorType& type, std::string error) {
	std::cout << "executePlaybackError: type: " << type << " error: " << error << std::endl;
    if (!m_currentInfo) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        setCurrentStateLocked(SpeechSynthesizerObserverInterface::SpeechSynthesizerState::FINISHED);
    }
    m_waitOnStateChange.notify_one();
    releaseForegroundTrace();
    resetCurrentInfo();
    resetMediaSourceId();
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_chatInfoQueue.empty()) {
			auto charInfo = m_chatInfoQueue.front();
            m_chatInfoQueue.pop_front();
			lock.unlock();
			reportExceptionFailed(charInfo, error);
			lock.lock();
        }
    }

}

void SpeechSynthesizer::startPlaying() {
	std::cout << "startPlaying" << std::endl;
    m_mediaSourceId = m_speechPlayer->setSource(m_currentInfo->url, DEFAULT_OFFSET);
    if (MediaPlayerInterface::ERROR == m_mediaSourceId) {
		std::cout << "startPlayingFailed:reason:setSourceFailed." << std::endl;
        executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
    } else if (!m_speechPlayer->play(m_mediaSourceId)) {
        executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
    } else {
        // Execution of play is successful.
        m_isAlreadyStopping = false;
    }
}

void SpeechSynthesizer::stopPlaying() {
	std::cout << "stopPlaying" << std::endl;
    if (MediaPlayerInterface::ERROR == m_mediaSourceId) {
		std::cout << "stopPlayingFailed:reason:invalidMediaSourceId:mediaSourceId: " << m_mediaSourceId << std::endl;
    } else if (m_isAlreadyStopping) {
		std::cout << "stopPlayingIgnored:reason:isAlreadyStopping." << std::endl;
    } else if (!m_speechPlayer->stop(m_mediaSourceId)) {
        executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "stopFailed");
    } else {
        // Execution of stop is successful.
        m_isAlreadyStopping = true;
    }
}

void SpeechSynthesizer::setCurrentStateLocked(
	SpeechSynthesizerObserverInterface::SpeechSynthesizerState newState) {
	std::cout << "setCurrentStateLocked:state: " << newState << std::endl;
    m_currentState = newState;

    for (auto observer : m_observers) {
        observer->onStateChanged(m_currentState);
    }
}

void SpeechSynthesizer::setDesiredStateLocked(FocusState newTrace) {
    switch (newTrace) {
        case FocusState::FOREGROUND:
            m_desiredState = SpeechSynthesizerObserverInterface::SpeechSynthesizerState::PLAYING;
            break;
        case FocusState::BACKGROUND:
        case FocusState::NONE:
            m_desiredState = SpeechSynthesizerObserverInterface::SpeechSynthesizerState::FINISHED;
            break;
    }
}

void SpeechSynthesizer::resetCurrentInfo(std::shared_ptr<ChatDirectiveInfo> chatInfo) {
    if (m_currentInfo != chatInfo) {
        if (m_currentInfo) {
            removeChatDirectiveInfo(m_currentInfo->directive->getMessageId());
			// Removing map of @c DomainProxy's @c m_directiveInfoMap
            removeDirective(m_currentInfo->directive->getMessageId());
            m_currentInfo->clear();
        }
        m_currentInfo = chatInfo;
    }
}

void SpeechSynthesizer::setHandlingCompleted() {
	std::cout << "setHandlingCompleted" << std::endl;
    if (m_currentInfo && m_currentInfo->result) {
        m_currentInfo->result->setCompleted();
    }
}

void SpeechSynthesizer::reportExceptionFailed(
	std::shared_ptr<ChatDirectiveInfo> info,
	const std::string& message) {
    if (info && info->result) {
        info->result->setFailed(message);
    }
    info->clear();
    removeDirective(info->directive->getMessageId());
    std::unique_lock<std::mutex> lock(m_mutex);
    if (SpeechSynthesizerObserverInterface::SpeechSynthesizerState::PLAYING == m_currentState ||
        SpeechSynthesizerObserverInterface::SpeechSynthesizerState::GAINING_FOCUS == m_currentState) {
        lock.unlock();
        stopPlaying();
    }
}

void SpeechSynthesizer::releaseForegroundTrace() {
	std::cout << "releaseForegroundTrace" << std::endl;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentFocus = FocusState::NONE;
    }
    m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
}

void SpeechSynthesizer::resetMediaSourceId() {
    m_mediaSourceId = MediaPlayerInterface::ERROR;
}

std::shared_ptr<SpeechSynthesizer::ChatDirectiveInfo> SpeechSynthesizer::validateInfo(
	const std::string& caller,
	std::shared_ptr<DirectiveInfo> info,
	bool checkResult) {
    if (!info) {
		std::cout << caller << "Failed:reason:nullptrInfo" <<std::endl;
        return nullptr;
    }
    if (!info->directive) {
		std::cout << caller << "Failed:reason:nullptrDirective" <<std::endl;
        return nullptr;
    }
    if (checkResult && !info->result) {
		std::cout << caller << "Failed:reason:nullptrResult" <<std::endl;
        return nullptr;
    }

    auto chatDirInfo = getChatDirectiveInfo(info->directive->getMessageId());
    if (chatDirInfo) {
        return chatDirInfo;
    }

    chatDirInfo = std::make_shared<ChatDirectiveInfo>(info);

    return chatDirInfo;

}

std::shared_ptr<SpeechSynthesizer::ChatDirectiveInfo> SpeechSynthesizer::getChatDirectiveInfo(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    auto it = m_chatDirectiveInfoMap.find(messageId);
    if (it != m_chatDirectiveInfoMap.end()) {
        return it->second;
    }
    return nullptr;
}

bool SpeechSynthesizer::setChatDirectiveInfo(
	const std::string& messageId,
	std::shared_ptr<SpeechSynthesizer::ChatDirectiveInfo> info) {
	std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    auto it = m_chatDirectiveInfoMap.find(messageId);
    if (it != m_chatDirectiveInfoMap.end()) {
        return false;
    }
    m_chatDirectiveInfoMap[messageId] = info;
    return true;
}

void SpeechSynthesizer::addToDirectiveQueue(std::shared_ptr<ChatDirectiveInfo> info) {
    std::lock_guard<std::mutex> lock(m_chatInfoQueueMutex);
    if (m_chatInfoQueue.empty()) {
        m_chatInfoQueue.push_back(info);
        executeHandleAfterValidation(info);
    } else {
		std::cout << "addToDirectiveQueue:queueSize: " << m_chatInfoQueue.size() << std::endl;
        m_chatInfoQueue.push_back(info);
    }
}

void SpeechSynthesizer::removeChatDirectiveInfo(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    m_chatDirectiveInfoMap.erase(messageId);
}

void SpeechSynthesizer::onDialogUXStateChanged(
	utils::dialogRelay::DialogUXStateObserverInterface::DialogUXState newState) {
	std::cout << "onDialogUXStateChanged" << std::endl;
	m_executor.submit([this, newState]() { executeOnDialogUXStateChanged(newState); });
}

void SpeechSynthesizer::executeOnDialogUXStateChanged(
    utils::dialogRelay::DialogUXStateObserverInterface::DialogUXState newState) {
	std::cout << "executeOnDialogUXStateChanged" << std::endl;
    if (newState != utils::dialogRelay::DialogUXStateObserverInterface::DialogUXState::IDLE) {
        return;
    }
    if (m_currentFocus != FocusState::NONE &&
        m_currentState != SpeechSynthesizerObserverInterface::SpeechSynthesizerState::GAINING_FOCUS) {
        m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
        m_currentFocus = FocusState::NONE;
    }
}

}	// namespace speechSynthesizer
}	// namespace domain
}	// namespace aisdk
