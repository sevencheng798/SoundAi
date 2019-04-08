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

#include "ResourcesPlayer/ResourcesPlayer.h"
#include <Utils/cJSON.h>
#include "string.h"

#include<deque>  

//using namespace std;
namespace aisdk {
namespace domain {
namespace resourcesPlayer {

using namespace utils::mediaPlayer;
using namespace utils::channel;
using namespace dmInterface;

/// The name of the @c AudioTrackManager channel used by the @c SpeechSynthesizer.
static const std::string CHANNEL_NAME = AudioTrackManagerInterface::DIALOG_CHANNEL_NAME;

/// The name of DomainProxy and SpeechChat handler interface
//static const std::string SPEECHCHAT{"SpeechChat"};

/// The name of the @c SafeShutdown
static const std::string SPEECHNAME{"ResourcesPlayer"};

/// The duration to wait for a state change in @c onTrackChanged before failing.
static const std::chrono::seconds STATE_CHANGE_TIMEOUT{5};

/// The duration to start playing offset position.
static const std::chrono::milliseconds DEFAULT_OFFSET{0};

//add deque for store audio_list;
std::deque<std::string> AUDIO_URL_LIST; 


std::shared_ptr<ResourcesPlayer> ResourcesPlayer::create(
	std::shared_ptr<MediaPlayerInterface> mediaPlayer,
	std::shared_ptr<AudioTrackManagerInterface> trackManager,
	std::shared_ptr<utils::dialogRelay::DialogUXStateRelay> dialogUXStateRelay){
	if(!mediaPlayer){
		std::cout << "ResourcesPlayerCreationFailed:reason:mediaPlayerNull" << std::endl;
		return nullptr;
	}

	if(!trackManager){
		std::cout << "ResourcesPlayerCreationFailed:reason::trackManagerNull" << std::endl;
		return nullptr;
	}

	if(!dialogUXStateRelay){
		std::cout << "ResourcesPlayerCreationFailed:reason:dialogUXStateRelayNull" << std::endl;
		return nullptr;
	}

	auto instance = std::shared_ptr<ResourcesPlayer>(new ResourcesPlayer(mediaPlayer, trackManager));
	if(!instance){
		std::cout << "ResourcesPlayerCreationFailed:reason:NewResourcesPlayerFailed." << std::endl;
		return nullptr;
	}
	instance->init();

	dialogUXStateRelay->addObserver(instance);

	return instance;
}

void ResourcesPlayer::onDeregistered() {
	// default no-op
}

void ResourcesPlayer::preHandleDirective(std::shared_ptr<DirectiveInfo> info) {
	std::cout << "preHandleDirective: messageId: " << info->directive->getMessageId() << std::endl;
    m_executor.submit([this, info]() { executePreHandle(info); });
}

void ResourcesPlayer::handleDirective(std::shared_ptr<DirectiveInfo> info) {
	std::cout << "handleDirective: messageId: " << info->directive->getMessageId() << std::endl;
    m_executor.submit([this, info]() { executeHandle(info); });
}

void ResourcesPlayer::cancelDirective(std::shared_ptr<DirectiveInfo> info) {
	std::cout << "handleDirective: messageId: " << info->directive->getMessageId() << std::endl;
    m_executor.submit([this, info]() { executeCancel(info); });
}

void ResourcesPlayer::onTrackChanged(FocusState newTrace) {
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
            setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::GAINING_FOCUS);
            break;
        case FocusState::BACKGROUND:
            setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::LOSING_FOCUS);
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

void ResourcesPlayer::onPlaybackStarted(SourceId id) {
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

void ResourcesPlayer::onPlaybackFinished(SourceId id) {
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

void ResourcesPlayer::onPlaybackError(
	SourceId id,
	const utils::mediaPlayer::ErrorType& type,
	std::string error) {
	std::cout << "onPlaybackError:callbackSourceId: " << id << std::endl;
	
    m_executor.submit([this, type, error]() { executePlaybackError(type, error); });
}

void ResourcesPlayer::onPlaybackStopped(SourceId id) {
	std::cout << "onPlaybackStopped:callbackSourceId: " << id << std::endl;
    onPlaybackFinished(id);
}

void ResourcesPlayer::addObserver(std::shared_ptr<ResourcesPlayerObserverInterface> observer) {
	std::cout << __func__ << ":addObserver:observer: " << observer.get() << std::endl;
    m_executor.submit([this, observer]() { m_observers.insert(observer); });
}

void ResourcesPlayer::removeObserver(std::shared_ptr<ResourcesPlayerObserverInterface> observer) {
	std::cout << __func__ << ":removeObserver:observer: " << observer.get() << std::endl;	
    m_executor.submit([this, observer]() { m_observers.erase(observer); }).wait();
}

std::string ResourcesPlayer::getHandlerName() const {
	return m_handlerName;
}

void ResourcesPlayer::doShutdown() {
	std::cout << "doShutdown" << std::endl;
	m_speechPlayer->setObserver(nullptr);
	{
        std::unique_lock<std::mutex> lock(m_mutex);
        if (ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING == m_currentState ||
            ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING == m_desiredState) {
            m_desiredState = ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED;
            stopPlaying();
            m_currentState = ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED;
            lock.unlock();
            releaseForegroundTrace();
        }
    }
	
    {
        std::lock_guard<std::mutex> lock(m_chatInfoQueueMutex);
        for (auto& info : m_chatInfoQueue) {
            if (info.get()->result) {
                info.get()->result->setFailed("ResourcesPlayerShuttingDown");
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

ResourcesPlayer::ChatDirectiveInfo::ChatDirectiveInfo(
	std::shared_ptr<nlp::DomainProxy::DirectiveInfo> directiveInfo) :
	directive{directiveInfo->directive},
	result{directiveInfo->result},
	sendCompletedMessage{false} {
}
	
void ResourcesPlayer::ChatDirectiveInfo::clear() {
    sendCompletedMessage = false;
}

ResourcesPlayer::ResourcesPlayer(
	std::shared_ptr<MediaPlayerInterface> mediaPlayer,
	std::shared_ptr<AudioTrackManagerInterface> trackManager) :
	DomainProxy{SPEECHNAME},
	SafeShutdown{SPEECHNAME},
	m_handlerName{SPEECHNAME},
	m_speechPlayer{mediaPlayer},
	m_trackManager{trackManager},
	m_mediaSourceId{MediaPlayerInterface::ERROR},
	m_currentState{ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED},
	m_desiredState{ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED},
	m_currentFocus{FocusState::NONE},
	m_isAlreadyStopping{false} {
}

void ResourcesPlayer::init() {
    m_speechPlayer->setObserver(shared_from_this());
}

//--------------------add by wx @190402-----------------
#if 1
void AnalysisNlpDataForResourcesPlayer(cJSON          * datain , std::deque<std::string> &audiourllist );

void AnalysisNlpDataForResourcesPlayer(cJSON          * datain , std::deque<std::string> &audiourllist )
{
    cJSON* json = NULL,
     *json_data = NULL,*json_tts_url = NULL, *json_isMultiDialog = NULL, *json_answer = NULL,

     *json_parameters = NULL, *json_artist = NULL, *json_title = NULL,
     *json_album = NULL,*json_audio_list = NULL, *json_audio_url = NULL;

     (void )json;
     (void )json_data;
     (void )json_answer;
     (void )json_tts_url;
     (void )json_isMultiDialog;

     (void )json_parameters;
     (void )json_artist;
     (void )json_title;
     (void )json_album;
     (void )json_audio_list;
     (void )json_audio_url;  

     
       json_data = datain;
       if(!json_data)
       {
       std::cout << "json Error before: " <<cJSON_GetErrorPtr() << std::endl;
       }
       else
       {
          json_answer = cJSON_GetObjectItem(json_data, "answer");
          json_tts_url = cJSON_GetObjectItem(json_data, "tts_url");
          json_isMultiDialog = cJSON_GetObjectItem(json_data, "isMultiDialog");
         // std::cout << "json_answer =  " << json_answer->valuestring << std::endl;
        //  std::cout << "json_tts_url =  " << json_tts_url->valuestring << std::endl;
        //  std::cout << "json_isMultiDialog = " << json_isMultiDialog->valueint << std::endl;

          audiourllist.push_back(json_tts_url->valuestring);

             //parameters  
             json_parameters = cJSON_GetObjectItem(json_data, "parameters"); 
            if(json_parameters != NULL) 
            {
             json_album = cJSON_GetObjectItem(json_parameters, "album");
             json_title = cJSON_GetObjectItem(json_parameters, "title");
            }
             else
             {
             std::cout << "parameters is null"<< std::endl;
             }

             //audio_list
             cJSON *js_list = cJSON_GetObjectItem(json_data, "audio_list");
              if(!js_list){
                  std::cout << "no audio_list!"<< std::endl;
              }
              int array_size = cJSON_GetArraySize(js_list);
              std::cout << "audio_list size : " <<array_size << std::endl;
              int i = 0;
              cJSON *item,*it;
              char *p  = NULL;
              for(i=0; i< array_size; i++) {
                 item = cJSON_GetArrayItem(js_list, i);
                 if(!item) {
                    //TODO...
                 }
                 p = cJSON_PrintUnformatted(item);
                 it = cJSON_Parse(p);
                 if(!it)
                    continue ;
             json_title = cJSON_GetObjectItem(it, "title");
             json_audio_url = cJSON_GetObjectItem(it, "audio_url");
             // std::cout << "NO: " << i << ":json_title =  "<< json_title->valuestring << std::endl;
             // std::cout << "NO: " << i << ":json_audio_url =  " << json_audio_url->valuestring << std::endl;
             audiourllist.push_back(json_audio_url->valuestring);
              }
#if 0
       std::cout <<"get_udio_url_list："<< std::endl;        
       for(std::size_t i = 0; i< audiourllist.size(); i++)
       {
        std::cout << "list_no:" << i << "; get_udio_url=" << audiourllist.at(i) << std::endl;
       }
#endif          
       }

     
}

#endif
//--------------------add by wx @190402-----------------

void ResourcesPlayer::executePreHandleAfterValidation(std::shared_ptr<ChatDirectiveInfo> info) {
	/// To-Do parse tts url and insert chatInfo map
    /// add by wx @201904
     auto nlpDomain = info->directive;
     auto dateMessage = nlpDomain->getData();
     std::cout << "dateMessage =  " << dateMessage.c_str() << std::endl;

     cJSON* json = NULL, *json_data = NULL;
     (void )json;
     (void )json_data;
     json_data = cJSON_Parse(dateMessage.c_str());

    //get nlp data audio url list to resources player @20190408
     AnalysisNlpDataForResourcesPlayer(json_data, AUDIO_URL_LIST);
     std::cout <<"====【20190408】==========AnalysisNlpDataForResourcesPlayer:"<< std::endl;  
     
     for(std::size_t i = 0; i< AUDIO_URL_LIST.size(); i++)
     {
      std::cout << "audio_url:" << AUDIO_URL_LIST.at(i) << std::endl;
     }
     std::cout << "====【20190408】==========i'm here!!!audio list get down========================" << std::endl;

   //  for(std::size_t i = 0; i < AUDIO_URL_LIST.size(); i++)
      for(std::size_t i = 0; i < 1; i++) 
      {
          info->url = AUDIO_URL_LIST.at(i);
          std::cout << "playing_now_audio_url = " << AUDIO_URL_LIST.at(i) << std::endl;
     
          AUDIO_URL_LIST.pop_back();
      }

      // If everything checks out, add the chatInfo to the map.    
      std::cout << "=========================i'm here!!!-解析date数据===========================" << std::endl;


    if (!setChatDirectiveInfo(info->directive->getMessageId(), info)) {
		std::cout << "executePreHandleFailed:reason:prehandleCalledTwiceOnSameDirective:messageId: " << info->directive->getMessageId() << std::endl;
    }
     //clear reque list data;
     while (!AUDIO_URL_LIST.empty()) AUDIO_URL_LIST.pop_back();
     cJSON_Delete(json_data);  
}

void ResourcesPlayer::executeHandleAfterValidation(std::shared_ptr<ChatDirectiveInfo> info) {
    m_currentInfo = info;
    if (!m_trackManager->acquireChannel(CHANNEL_NAME, shared_from_this(), SPEECHNAME)) {
        static const std::string message = std::string("Could not acquire ") + CHANNEL_NAME + " for " + SPEECHNAME;
		std::cout << "executeHandleFailed:reason:CouldNotAcquireChannel:messageId: " << m_currentInfo->directive->getMessageId() << std::endl;
        reportExceptionFailed(info, message);
    }
}

void ResourcesPlayer::executePreHandle(std::shared_ptr<DirectiveInfo> info) {
	std::cout << "executePreHandle:messageId" << info->directive->getMessageId() << std::endl;
    auto chatInfo = validateInfo("executePreHandle", info);
    if (!chatInfo) {
		std::cout << "executePreHandleFailed:reason:invalidDirectiveInfo" << std::endl;
        return;
    }
    executePreHandleAfterValidation(chatInfo);
}

void ResourcesPlayer::executeHandle(std::shared_ptr<DirectiveInfo> info) {
	std::cout << "executeHandle:messageId" << info->directive->getMessageId() << std::endl;
    auto chatInfo = validateInfo("executeHandle", info);
    if (!chatInfo) {
		std::cout << "executeHandleFailed:reason:invalidDirectiveInfo" << std::endl;
        return;
    }
    addToDirectiveQueue(chatInfo);
}

void ResourcesPlayer::executeCancel(std::shared_ptr<DirectiveInfo> info) {
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
    if (ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED != m_desiredState) {
        m_desiredState = ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED;
        if (ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING == m_currentState ||
            ResourcesPlayerObserverInterface::ResourcesPlayerState::GAINING_FOCUS == m_currentState) {
            lock.unlock();
            if (m_currentInfo) {
                m_currentInfo->sendCompletedMessage = false;
            }
            stopPlaying();
        }
    }

}

void ResourcesPlayer::executeStateChange() {
    ResourcesPlayerObserverInterface::ResourcesPlayerState newState;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        newState = m_desiredState;
    }

	std::cout << "executeStateChange:newState: " << newState << std::endl;
    switch (newState) {
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING:
            if (m_currentInfo) {
                m_currentInfo->sendCompletedMessage = true;
            }
			// Trigger play
            startPlaying();
            break;
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED:
			// Trigger stop
            stopPlaying();
            break;
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::GAINING_FOCUS:
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::LOSING_FOCUS:
            // Nothing to do
            break;
    }
}

void ResourcesPlayer::executePlaybackStarted() {
	std::cout << "executePlaybackStarted." << std::endl;
	
    if (!m_currentInfo) {
		std::cout << "executePlaybackStartedIgnored:reason:nullptrDirectiveInfo" << std::endl;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
		/// Set current state @c m_currentState to PLAYING to specify device alreay start to playback.
        setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING);
    }
	
	/// wakeup condition wait
    m_waitOnStateChange.notify_one();
}

void ResourcesPlayer::executePlaybackFinished() {
	std::cout << "executePlaybackFinished." << std::endl;

    if (!m_currentInfo) {
		std::cout << "executePlaybackFinishedIgnored:reason:nullptrDirectiveInfo" << std::endl;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED);
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

void ResourcesPlayer::executePlaybackError(const utils::mediaPlayer::ErrorType& type, std::string error) {
	std::cout << "executePlaybackError: type: " << type << " error: " << error << std::endl;
    if (!m_currentInfo) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED);
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

void ResourcesPlayer::startPlaying() {
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
    	std::cout << "=======================here：startplaying=========." << std::endl;  
}

void ResourcesPlayer::stopPlaying() {
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
     std::cout << "=======================here：stopPlaying=========." << std::endl; 
}

void ResourcesPlayer::setCurrentStateLocked(
	ResourcesPlayerObserverInterface::ResourcesPlayerState newState) {
	std::cout << "setCurrentStateLocked:state: " << newState << std::endl;
    m_currentState = newState;

    for (auto observer : m_observers) {
        observer->onStateChanged(m_currentState);
    }
}

void ResourcesPlayer::setDesiredStateLocked(FocusState newTrace) {
    switch (newTrace) {
        case FocusState::FOREGROUND:
            m_desiredState = ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING;
            break;
        case FocusState::BACKGROUND:
        case FocusState::NONE:
            m_desiredState = ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED;
            break;
    }
}

void ResourcesPlayer::resetCurrentInfo(std::shared_ptr<ChatDirectiveInfo> chatInfo) {
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

void ResourcesPlayer::setHandlingCompleted() {
	std::cout << "setHandlingCompleted" << std::endl;
    if (m_currentInfo && m_currentInfo->result) {
        m_currentInfo->result->setCompleted();
    }
}

void ResourcesPlayer::reportExceptionFailed(
	std::shared_ptr<ChatDirectiveInfo> info,
	const std::string& message) {
    if (info && info->result) {
        info->result->setFailed(message);
    }
    info->clear();
    removeDirective(info->directive->getMessageId());
    std::unique_lock<std::mutex> lock(m_mutex);
    if (ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING == m_currentState ||
        ResourcesPlayerObserverInterface::ResourcesPlayerState::GAINING_FOCUS == m_currentState) {
        lock.unlock();
        stopPlaying();
    }
}

void ResourcesPlayer::releaseForegroundTrace() {
	std::cout << "releaseForegroundTrace" << std::endl;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentFocus = FocusState::NONE;
    }
    m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
}

void ResourcesPlayer::resetMediaSourceId() {
    m_mediaSourceId = MediaPlayerInterface::ERROR;
}

std::shared_ptr<ResourcesPlayer::ChatDirectiveInfo> ResourcesPlayer::validateInfo(
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

std::shared_ptr<ResourcesPlayer::ChatDirectiveInfo> ResourcesPlayer::getChatDirectiveInfo(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    auto it = m_chatDirectiveInfoMap.find(messageId);
    if (it != m_chatDirectiveInfoMap.end()) {
        return it->second;
    }
    return nullptr;
}

bool ResourcesPlayer::setChatDirectiveInfo(
	const std::string& messageId,
	std::shared_ptr<ResourcesPlayer::ChatDirectiveInfo> info) {
	std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    auto it = m_chatDirectiveInfoMap.find(messageId);
    if (it != m_chatDirectiveInfoMap.end()) {
        return false;
    }
    m_chatDirectiveInfoMap[messageId] = info;
    return true;
}

void ResourcesPlayer::addToDirectiveQueue(std::shared_ptr<ChatDirectiveInfo> info) {
    std::lock_guard<std::mutex> lock(m_chatInfoQueueMutex);
    if (m_chatInfoQueue.empty()) {
        m_chatInfoQueue.push_back(info);
        executeHandleAfterValidation(info);
    } else {
		std::cout << "addToDirectiveQueue:queueSize: " << m_chatInfoQueue.size() << std::endl;
        m_chatInfoQueue.push_back(info);
    }
}

void ResourcesPlayer::removeChatDirectiveInfo(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    m_chatDirectiveInfoMap.erase(messageId);
}

void ResourcesPlayer::onDialogUXStateChanged(
	utils::dialogRelay::DialogUXStateObserverInterface::DialogUXState newState) {
	std::cout << "onDialogUXStateChanged" << std::endl;
	m_executor.submit([this, newState]() { executeOnDialogUXStateChanged(newState); });
}

void ResourcesPlayer::executeOnDialogUXStateChanged(
    utils::dialogRelay::DialogUXStateObserverInterface::DialogUXState newState) {
	std::cout << "executeOnDialogUXStateChanged" << std::endl;
    if (newState != utils::dialogRelay::DialogUXStateObserverInterface::DialogUXState::IDLE) {
        return;
    }
    if (m_currentFocus != FocusState::NONE &&
        m_currentState != ResourcesPlayerObserverInterface::ResourcesPlayerState::GAINING_FOCUS) {
        m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
        m_currentFocus = FocusState::NONE;
    }
}


#if 0

int test();
void test_string();


//-------------
int test()
{
    std::deque<int> test;
    test.push_back(10);
    test.push_back(20);
    test.push_back(30);
    std::cout << "=========================i'm here!!!我是队列测试demo-start===========================" << std::endl;
    std::cout <<"原始双端队列："<< std::endl;  
    
    for(std::size_t i = 0; i< test.size(); i++)
    {
     std::cout << "test: = " << test.at(i) << std::endl;
    }

    test.push_front(99);
    test.push_front(88);
    test.push_front(77);
    std::cout <<"从前端插入：" << std::endl;

    for(std::size_t i = 0; i < test.size(); i++)
    {
    std::cout << "push front: = " << test.at(i) << std::endl;
    }

    test.pop_front();    
    for(std::size_t i = 0; i < test.size(); i++)
    {
    std::cout << "after pop front: = " << test.at(i) << std::endl;
    }

    test.pop_back();
    for(std::size_t i = 0; i < test.size(); i++)
    {
    std::cout << "after pop back: = " << test.at(i) << std::endl;
    }

    

    std::cout << "=========================i'm here!!!我是队列测试demo-end===========================" << std::endl;
    return 0;
}



//-------------
void test_string()
{
    std::deque<std::string> d;
    d.push_back("aaaa");
   // test.push_back(20);
   // test.push_back(30);
    std::cout << "=========================i'm here!!!我是队列测试demo-start===========================" << std::endl;
    std::cout <<"原始双端队列："<< std::endl;  
    
    for(std::size_t i = 0; i< d.size(); i++)
    {
     std::cout << "test: = " << d.at(i) << std::endl;
    }
    
    std::cout << "=========================i'm here!!!我是队列测试demo-end===========================" << std::endl;
    //return 0;
}

#endif
    
//-------------

}	// namespace speechSynthesizer
}	// namespace domain
}	// namespace aisdk

