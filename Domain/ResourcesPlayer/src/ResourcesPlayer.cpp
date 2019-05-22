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

//nclude <Utils/cJSON.h>
// jsoncpp ver-1.8.3
#include <json/json.h>



#include "string.h"

#include<deque>  
#include <Utils/Logging/Logger.h>


/// String to identify log entries originating from this file.
static const std::string TAG{"ResourcesPlayer"};

#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

//using namespace std;
namespace aisdk {
namespace domain {
namespace resourcesPlayer {

using namespace utils::mediaPlayer;
using namespace utils::channel;
using namespace dmInterface;

/// The name of the @c AudioTrackManager channel used by the @c resourcesPlayer.
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
        
        AISDK_ERROR(LX("ResourcesPlayerCreationFailed").d("reason: ", "mediaPlayerNull"));
		return nullptr;
	}

	if(!trackManager){
        AISDK_ERROR(LX("ResourcesPlayerCreationFailed").d("reason: ", "trackManagerNull"));
		return nullptr;
	}

	if(!dialogUXStateRelay){
        AISDK_ERROR(LX("ResourcesPlayerCreationFailed").d("reason: ", "dialogUXStateRelayNull"));
		return nullptr;
	}

	auto instance = std::shared_ptr<ResourcesPlayer>(new ResourcesPlayer(mediaPlayer, trackManager));
	if(!instance){
        AISDK_ERROR(LX("ResourcesPlayerCreationFailed").d("reason: ", "NewResourcesPlayerFailed"));
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
    AISDK_INFO(LX("preHandleDirective").d("messageId: ",  info->directive->getMessageId()));
    m_executor.submit([this, info]() { executePreHandle(info); });
}

void ResourcesPlayer::handleDirective(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("handleDirective").d("messageId: ",  info->directive->getMessageId()));
    m_executor.submit([this, info]() { executeHandle(info); });
}

void ResourcesPlayer::cancelDirective(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("cancelDirective").d("messageId: ",  info->directive->getMessageId()));
    m_executor.submit([this, info]() { executeCancel(info); });
}

void ResourcesPlayer::onTrackChanged(FocusState newTrace) {
    AISDK_INFO(LX("onTrackChanged").d("newTrace: ", newTrace));
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
            AISDK_INFO(LX("onTrackChangedSuccess"));
    } else {
            AISDK_INFO(LX("onFocusChangeFailed").d("reason:stateChangeTimeout:messageId:", messageId));
        if (m_currentInfo) {
            lock.unlock();
			reportExceptionFailed(m_currentInfo, "stateChangeTimeout");
        }
    }
}

void ResourcesPlayer::onPlaybackStarted(SourceId id) {
	AISDK_INFO(LX("ResourcesPlayeronPlaybackStarted").d("callbackSourceId：", id));
    if (id != m_mediaSourceId) {
		AISDK_ERROR(LX("queueingExecutePlaybackStartedFailed")
					.d("reason", "mismatchSourceId")
					.d("callbackSourceId", id)
					.d("sourceId", m_mediaSourceId));
        m_executor.submit([this] {
            executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "executePlaybackStartedFailed");
        });
    } else {
        m_executor.submit([this]() { executePlaybackStarted(); });
    }

}

void ResourcesPlayer::onPlaybackFinished(SourceId id) {
    AISDK_INFO(LX("onPlaybackFinished").d("callbackSourceId:", id));

    if (id != m_mediaSourceId) {
		AISDK_ERROR(LX("queueingExecutePlaybackFinishedFailed")
					.d("reason", "mismatchSourceId")
					.d("callbackSourceId", id)
					.d("sourceId", m_mediaSourceId));
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
    AISDK_ERROR(LX("onPlaybackError").d("callbackSourceId:", id));
            
    m_executor.submit([this, type, error]() { executePlaybackError(type, error); });
}

void ResourcesPlayer::onPlaybackStopped(SourceId id) {
	AISDK_INFO(LX("onPlaybackStopped").d("callbackSourceId", id));
    onPlaybackFinished(id);
}

void ResourcesPlayer::addObserver(std::shared_ptr<ResourcesPlayerObserverInterface> observer) {
	AISDK_INFO(LX("addObserver").d("observer", observer.get()));
    m_executor.submit([this, observer]() { m_observers.insert(observer); });
}

void ResourcesPlayer::removeObserver(std::shared_ptr<ResourcesPlayerObserverInterface> observer) {
	AISDK_INFO(LX("removeObserver").d("observer", observer.get()));
    m_executor.submit([this, observer]() { m_observers.erase(observer); }).wait();
}

std::string ResourcesPlayer::getHandlerName() const {
	return m_handlerName;
}

void ResourcesPlayer::doShutdown() {
	AISDK_INFO(LX("doShutdown").d("reason", "destory"));
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

ResourcesPlayer::ResourcesDirectiveInfo::ResourcesDirectiveInfo(
	std::shared_ptr<nlp::DomainProxy::DirectiveInfo> directiveInfo) :
	directive{directiveInfo->directive},
	result{directiveInfo->result},
	sendCompletedMessage{false} {
}
	
void ResourcesPlayer::ResourcesDirectiveInfo::clear() {
    attachmentReader.reset();
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
#if 0
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
/*
void ResourcesPlayer::executePreHandleAfterValidation(std::shared_ptr<ResourcesDirectiveInfo> info) {
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
     info->audioList = AUDIO_URL_LIST;
     AISDK_INFO(LX("=【20190408】=========AnalysisNlpDataForResourcesPlayer"));
     
     for(std::size_t i = 0; i< AUDIO_URL_LIST.size(); i++)
     {
      AISDK_INFO(LX("AUDIO_URL_LIST").d("audio_url:",  AUDIO_URL_LIST.at(i)));
     }


      info->url = AUDIO_URL_LIST.at(0);
      AUDIO_URL_LIST.pop_front();
#if 0
   //  for(std::size_t i = 0; i < AUDIO_URL_LIST.size(); i++)
      for(std::size_t i = 0; i < 1; i++) 
      {
          info->url = AUDIO_URL_LIST.at(i);
          AISDK_INFO(LX("AUDIO_URL_LIST").d("playing_now_audio_url:",  AUDIO_URL_LIST.at(i)));
          AUDIO_URL_LIST.pop_back();
      }

#endif
      // If everything checks out, add the chatInfo to the map.    
      std::cout << "=========================i'm here!!!-解析date数据===========================" << std::endl;


    if (!setResourcesDirectiveInfo(info->directive->getMessageId(), info)) {
        AISDK_ERROR(LX("executePreHandleFailed").d("reason:prehandleCalledTwiceOnSameDirective:messageId:", info->directive->getMessageId()));
    }
     //clear reque list data;
   //  while (!AUDIO_URL_LIST.empty()) AUDIO_URL_LIST.pop_back();
     cJSON_Delete(json_data);  
}

*/



void ResourcesPlayer::executePreHandleAfterValidation(std::shared_ptr<ResourcesDirectiveInfo> info) {
	/// To-Do parse tts url and insert chatInfo map
    ///
        // TODO:parse tts url and insert chatInfo map - Fix me.
#ifdef ENABLE_SOUNDAI_ASR
        auto data = info->directive->getData();
        Json::CharReaderBuilder readerBuilder;
        JSONCPP_STRING errs;
        Json::Value root;
        std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());
        if (!reader->parse(data.c_str(), data.c_str()+data.length(), &root, &errs)) {
            AISDK_ERROR(LX("executePreHandleAfterValidation").d("reason", "parseDataKeyError"));
            return;
        }
        std::string url = root["tts_url"].asString();
        AISDK_INFO(LX("executePreHandleAfterValidation").d("url", url));
        info->url = url;
#else	
            info->attachmentReader = info->directive->getAttachmentReader(
                info->directive->getMessageId(), utils::sharedbuffer::ReaderPolicy::BLOCKING);
#endif	
            // If everything checks out, add the chatInfo to the map.
            if (!setResourcesDirectiveInfo(info->directive->getMessageId(), info)) {
                AISDK_ERROR(LX("executePreHandleFailed")
                            .d("reason", "prehandleCalledTwiceOnSameDirective")
                            .d("messageId", info->directive->getMessageId()));
            }       
}


void ResourcesPlayer::executeHandleAfterValidation(std::shared_ptr<ResourcesDirectiveInfo> info) {
    m_currentInfo = info;
    AISDK_INFO(LX("executeHandleAfterValidation").d("m_currentInfo-url: ", m_currentInfo->url ));
    if (!m_trackManager->acquireChannel(CHANNEL_NAME, shared_from_this(), SPEECHNAME)) {
        static const std::string message = std::string("Could not acquire ") + CHANNEL_NAME + " for " + SPEECHNAME;
		AISDK_INFO(LX("executeHandleFailed")
					.d("reason", "CouldNotAcquireChannel")
					.d("messageId", m_currentInfo->directive->getMessageId()));
        reportExceptionFailed(info, message);
    }
}

void ResourcesPlayer::executePreHandle(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("executePreHandle").d("messageId: ", info->directive->getMessageId() ));
    auto chatInfo = validateInfo("executePreHandle", info);
    if (!chatInfo) {
		AISDK_ERROR(LX("executePreHandleFailed").d("reason", "invalidDirectiveInfo"));
        return;
    }
    executePreHandleAfterValidation(chatInfo);
}

void ResourcesPlayer::executeHandle(std::shared_ptr<DirectiveInfo> info) {
	AISDK_INFO(LX("executeHandle").d("messageId", info->directive->getMessageId()));
    auto chatInfo = validateInfo("executeHandle", info);
     AISDK_INFO(LX("===========executeHandle").d("chatInfo=========: ", chatInfo->url));
    if (!chatInfo) {
		AISDK_ERROR(LX("executeHandleFailed").d("reason", "invalidDirectiveInfo"));
        return;
    }
    addToDirectiveQueue(chatInfo);
}

void ResourcesPlayer::executeCancel(std::shared_ptr<DirectiveInfo> info) {
	AISDK_INFO(LX("executeCancel").d("messageId", info->directive->getMessageId()));
    auto chatInfo = validateInfo("executeCancel", info);
    if (!chatInfo) {
        AISDK_ERROR(LX("executeCancelFailed").d("reason", "invalidDirectiveInfo"));
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

    AISDK_INFO(LX("executeStateChange").d("newState: ", newState ));
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
	AISDK_INFO(LX("executePlaybackStarted"));
	
    if (!m_currentInfo) {
		AISDK_ERROR(LX("executePlaybackStartedIgnored").d("reason", "nullptrDirectiveInfo"));
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
	AISDK_INFO(LX("executePlaybackFinished"));
    std::shared_ptr<ResourcesDirectiveInfo> infotest;

    if (!m_currentInfo) {
        
		AISDK_ERROR(LX("executePlaybackFinishedIgnored").d("reason", "nullptrDirectiveInfo"));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED);
        //todo something 
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

#if 1
//-------------------------
  //  while (!AUDIO_URL_LIST.empty()) AUDIO_URL_LIST.pop_back();
     for(std::size_t i = 0; i< AUDIO_URL_LIST.size(); i++)
     {
      AISDK_INFO(LX("current_AUDIO_URL_LIST").d("==============:",  AUDIO_URL_LIST.at(i)));
     }

     if(!AUDIO_URL_LIST.empty()){

         //  std::make_shared<ResourcesDirectiveInfo>(infotest);
          // infotest->url = AUDIO_URL_LIST.at(3);
           AISDK_INFO(LX("infotest->url").d("=====infotest->url======:",  AUDIO_URL_LIST.at(3)));
       
     //    addToDirectiveQueue(infotest);
     }
#endif
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

/*
void ResourcesPlayer::startPlaying() {
	std::cout << "startPlaying" << std::endl;
    m_mediaSourceId = m_speechPlayer->setSource(m_currentInfo->url, DEFAULT_OFFSET);


    
    if (MediaPlayerInterface::ERROR == m_mediaSourceId) {
        AISDK_ERROR(LX("startPlayingFailed").d("reason", "setSourceFailed."));
        executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
    } else if (!m_speechPlayer->play(m_mediaSourceId)) {
        executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
    } else {
        // Execution of play is successful.
        m_isAlreadyStopping = false;
    }
    	std::cout << "=======================here：startplaying=========." << std::endl;  
}

*/
void ResourcesPlayer::startPlaying() {
	AISDK_INFO(LX("startPlaying"));
	#ifndef ENABLE_SOUNDAI_ASR
	/// The following params must be set fix point.
	const utils::AudioFormat format{
						.encoding = aisdk::utils::AudioFormat::Encoding::LPCM,
					   .endianness = aisdk::utils::AudioFormat::Endianness::LITTLE,
					   .sampleRateHz = 16000,	
					   .sampleSizeInBits = 16,
					   .numChannels = 1,	
					   .dataSigned = true
	};
	
    m_mediaSourceId = m_speechPlayer->setSource(std::move(m_currentInfo->attachmentReader), &format);
	#else
	m_mediaSourceId = m_speechPlayer->setSource(m_currentInfo->url);
	#endif
    if (MediaPlayerInterface::ERROR == m_mediaSourceId) {
		AISDK_ERROR(LX("startPlayingFailed").d("reason", "setSourceFailed"));
        executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
    } else if (!m_speechPlayer->play(m_mediaSourceId)) {
        executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
    } else {
        // Execution of play is successful.
        m_isAlreadyStopping = false;
    }
}


void ResourcesPlayer::stopPlaying() {
	AISDK_INFO(LX("stopPlaying"));
    if (MediaPlayerInterface::ERROR == m_mediaSourceId) {
		AISDK_ERROR(LX("stopPlayingFailed").d("reason", "invalidMediaSourceId").d("mediaSourceId", m_mediaSourceId));
    } else if (m_isAlreadyStopping) {
		AISDK_WARN(LX("stopPlayingIgnored").d("reason", "isAlreadyStopping"));
    } else if (!m_speechPlayer->stop(m_mediaSourceId)) {
        executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "stopFailed");
    } else {
        // Execution of stop is successful.
        m_isAlreadyStopping = true;
    }
}


void ResourcesPlayer::setCurrentStateLocked(
	ResourcesPlayerObserverInterface::ResourcesPlayerState newState) {
    AISDK_INFO(LX("setCurrentStateLocked").d("state", newState));
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

void ResourcesPlayer::resetCurrentInfo(std::shared_ptr<ResourcesDirectiveInfo> chatInfo) {
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
	AISDK_INFO(LX("setHandlingCompleted"));
    if (m_currentInfo && m_currentInfo->result) {
        m_currentInfo->result->setCompleted();
    }
}

void ResourcesPlayer::reportExceptionFailed(
	std::shared_ptr<ResourcesDirectiveInfo> info,
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
	AISDK_INFO(LX("releaseForegroundTrace"));
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentFocus = FocusState::NONE;
    }
    m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
}

void ResourcesPlayer::resetMediaSourceId() {
    m_mediaSourceId = MediaPlayerInterface::ERROR;
}

std::shared_ptr<ResourcesPlayer::ResourcesDirectiveInfo> ResourcesPlayer::validateInfo(
	const std::string& caller,
	std::shared_ptr<DirectiveInfo> info,
	bool checkResult) {
    if (!info) {
		AISDK_ERROR(LX(caller + "Failed").d("reason", "nullptrInfo"));
        return nullptr;
    }
    if (!info->directive) {
		AISDK_ERROR(LX(caller + "Failed").d("reason", "nullptrDirective"));
        return nullptr;
    }
    if (checkResult && !info->result) {
		AISDK_ERROR(LX(caller + "Failed").d("reason", "nullptrResult"));
        return nullptr;
    }

    auto chatDirInfo = getResourcesDirectiveInfo(info->directive->getMessageId());
    if (chatDirInfo) {
        return chatDirInfo;
    }

    chatDirInfo = std::make_shared<ResourcesDirectiveInfo>(info);

    return chatDirInfo;

}

std::shared_ptr<ResourcesPlayer::ResourcesDirectiveInfo> ResourcesPlayer::getResourcesDirectiveInfo(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    auto it = m_chatDirectiveInfoMap.find(messageId);
    if (it != m_chatDirectiveInfoMap.end()) {
        return it->second;
    }
    return nullptr;
}

bool ResourcesPlayer::setResourcesDirectiveInfo(
	const std::string& messageId,
	std::shared_ptr<ResourcesPlayer::ResourcesDirectiveInfo> info) {
	std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    auto it = m_chatDirectiveInfoMap.find(messageId);
    if (it != m_chatDirectiveInfoMap.end()) {
        return false;
    }
    m_chatDirectiveInfoMap[messageId] = info;
    return true;
}

void ResourcesPlayer::addToDirectiveQueue(std::shared_ptr<ResourcesDirectiveInfo> info) {
    std::lock_guard<std::mutex> lock(m_chatInfoQueueMutex);
     AISDK_INFO(LX("========================iamamama-"));

    if (m_chatInfoQueue.empty()) {
        m_chatInfoQueue.push_back(info);
        executeHandleAfterValidation(info);
    } else {
		AISDK_INFO(LX("addToDirectiveQueue").d("queueSize", m_chatInfoQueue.size()));
        m_chatInfoQueue.push_back(info);
    }
}

void ResourcesPlayer::removeChatDirectiveInfo(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    m_chatDirectiveInfoMap.erase(messageId);
}

void ResourcesPlayer::onDialogUXStateChanged(
	utils::dialogRelay::DialogUXStateObserverInterface::DialogUXState newState) {
	AISDK_DEBUG5(LX("onDialogUXStateChanged"));
	m_executor.submit([this, newState]() { executeOnDialogUXStateChanged(newState); });
}

void ResourcesPlayer::executeOnDialogUXStateChanged(
    utils::dialogRelay::DialogUXStateObserverInterface::DialogUXState newState) {
	AISDK_INFO(LX("executeOnDialogUXStateChanged"));
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

