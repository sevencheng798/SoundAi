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
static const std::string CHANNEL_NAME = AudioTrackManagerInterface::MEDIA_CHANNEL_NAME;

/// The name of DomainProxy and SpeechChat handler interface
//static const std::string SPEECHCHAT{"SpeechChat"};

/// The name of the @c SafeShutdown
static const std::string SPEECHNAME{"ResourcesPlayer"};

/// The duration to wait for a state change in @c onTrackChanged before failing.
static const std::chrono::seconds STATE_CHANGE_TIMEOUT{3};

/// The duration to start playing offset position.
static const std::chrono::milliseconds DEFAULT_OFFSET{0};

//add deque for store audio_list;
std::deque<std::string> AUDIO_URL_LIST; 

///add for use store play control state; enable = 1;unable = 0;
int flag_playControl_pause = 0; 

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

    if(info->directive->getDomain() == SPEECHNAME ){

         if( !m_speechPlayer->stop(m_mediaSourceId)){
            AISDK_ERROR(LX("executeTrackChanged").d("stop","failed"));
         }else{
            AISDK_INFO(LX("executeTrackChanged = clean current resources and stop player state ")); 
         }
         m_executor.submit([this, info]() { executeHandle(info); });

    }else if(info->directive->getDomain() == "PlayControl" ){

        //add @20190612
        std::string operation;
        AnalysisNlpDataForPlayControl(info, operation);
     
        if( operation == "PAUSE"){
             std::this_thread::sleep_for( std::chrono::microseconds(100));
             
              flag_playControl_pause = 1;
              if( !m_speechPlayer->pause(m_mediaSourceId)){
                 AISDK_ERROR(LX("executeTrackChanged").d("pause","failed"));
              } 
       
              info->result->setCompleted();  
        }else if(operation == "STOP" ){
              std::this_thread::sleep_for( std::chrono::microseconds(100));
 
              flag_playControl_pause = 1;
              if( !m_speechPlayer->pause(m_mediaSourceId)){
                  AISDK_ERROR(LX("executeTrackChanged").d("pause","failed"));
              }  
              
              info->result->setCompleted();  
        }else if(operation == "CONTINUE" ){
              flag_playControl_pause = 0;
              if( !m_speechPlayer->resume(m_mediaSourceId)){
                  AISDK_ERROR(LX("executeTrackChanged").d("resume","failed"));
              }  
              
              info->result->setCompleted();  
        }else if(operation == "NEXT"){

#if 0        
               flag_playControl_pause = 0;
              if(!AUDIO_URL_LIST.empty()){
                  AUDIO_URL_LIST.pop_front();
          if( !m_speechPlayer->stop(m_mediaSourceId)){
                  AISDK_ERROR(LX("executeTrackChanged").d("stop","failed"));
              }
                  m_mediaSourceId = m_speechPlayer->setSource(AUDIO_URL_LIST.at(0));
              }
              if( !m_speechPlayer->play(m_mediaSourceId)){
                  AISDK_ERROR(LX("executeTrackChanged").d("NEXT","failed"));
              }  
#endif     
              info->result->setCompleted();  
              
        }else if(operation == "SWITCH"){
        
                    AISDK_ERROR(LX("executeTrackChanged").d("SWITCH","failed"));
    
        }else if(operation == "PREVIOUS"){
        
                    AISDK_ERROR(LX("executeTrackChanged").d("PREVIOUS","failed"));
    
        }else if(operation == "SINGLE_LOOP"){
        
                    AISDK_ERROR(LX("executeTrackChanged").d("SINGLE_LOOP","failed"));
    
        }else{
               AISDK_ERROR(LX("executeTrackChanged").d("operation","mull"));
    
               info->result->setCompleted();  
        }


           
    }
    
 
}

void ResourcesPlayer::cancelDirective(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("cancelDirective").d("messageId: ",  info->directive->getMessageId()));
    m_executor.submit([this, info]() { executeCancel(info); });
}

void ResourcesPlayer::onTrackChanged(FocusState newTrace) {
    AISDK_INFO(LX("onTrackChanged").d("newTrace: ", newTrace));
    m_executor.submit([this, newTrace]() { executeTrackChanged(newTrace); });

    // Set intermediate state to avoid being considered idle
    switch (newTrace) {
        case FocusState::FOREGROUND:
            return;
        case FocusState::BACKGROUND:
        {
             auto predicate = [this](){ 
                switch (m_currentState) {
                    case  ResourcesPlayerObserverInterface::ResourcesPlayerState::IDLE:
                    case  ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED:
                    case  ResourcesPlayerObserverInterface::ResourcesPlayerState::STOPPED: 
                    case  ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED: 
                    return true;
                    case  ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING:
                    return false;
                    default: 
                    return false; 
                }
             };
             // Block until we achieve the desired state.
             std::unique_lock<std::mutex> lock(m_mutex);
             if ( !m_waitOnStateChange.wait_for( lock, STATE_CHANGE_TIMEOUT, predicate) ){
                 AISDK_ERROR(LX("onTrackChangedTimeout"));
             }
        }
            break;
        case FocusState::NONE:
        {
             auto predicate = [this](){ 
                switch (m_currentState) {
                    case  ResourcesPlayerObserverInterface::ResourcesPlayerState::IDLE:
                    case  ResourcesPlayerObserverInterface::ResourcesPlayerState::STOPPED:
                    case  ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED: 
                    return true;
                    case  ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING:
                    case  ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED:  
                    return false;
                    default: 
                    return false; 
                }
             };
             // Block until we achieve the desired state.
             std::unique_lock<std::mutex> lock(m_mutex);
             if ( !m_waitOnStateChange.wait_for( lock, STATE_CHANGE_TIMEOUT, predicate)) {
                 AISDK_ERROR(LX("onTrackChangedTimeout"));
             }
        }    
            break;
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


void ResourcesPlayer::onPlaybackPaused(SourceId id){
    AISDK_INFO(LX("ResourcesPlayeronPlaybackPaused").d("callbackSourceId：", id));
    if (id != m_mediaSourceId) {
        AISDK_ERROR(LX("queueingExecutePlaybackPausedFailed")
                    .d("reason", "mismatchSourceId")
                    .d("callbackSourceId", id)
                    .d("sourceId", m_mediaSourceId));
        m_executor.submit([this] {
            executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "executePlaybackPausedFailed");
        });
    } else {
        m_executor.submit([this]() { executePlaybackPaused(); });
    }

}


void ResourcesPlayer::onPlaybackResumed(SourceId id){
    AISDK_INFO(LX("ResourcesPlayeronPlaybackResumed").d("callbackSourceId：", id));
    if (id != m_mediaSourceId) {
        AISDK_ERROR(LX("queueingExecutePlaybackResumedFailed")
                    .d("reason", "mismatchSourceId")
                    .d("callbackSourceId", id)
                    .d("sourceId", m_mediaSourceId));
        m_executor.submit([this] {
            executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "executePlaybackResumedFailed");
        });
    } else {
        m_executor.submit([this]() { executePlaybackResumed(); });
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
    m_executor.submit([this]() { executePlaybackStopped(); });

}

void ResourcesPlayer::addObserver(std::shared_ptr<ResourcesPlayerObserverInterface> observer) {
	AISDK_INFO(LX("addObserver").d("observer", observer.get()));
    m_executor.submit([this, observer]() { m_observers.insert(observer); });
}

void ResourcesPlayer::removeObserver(std::shared_ptr<ResourcesPlayerObserverInterface> observer) {
	AISDK_INFO(LX("removeObserver").d("observer", observer.get()));
    m_executor.submit([this, observer]() { m_observers.erase(observer); }).wait();
}

std::unordered_set<std::string> ResourcesPlayer::getHandlerName() const {
	return m_handlerName;
}

void ResourcesPlayer::buttonPressedPlayback() {
    AISDK_INFO(LX("buttonPressedPlayback").d("reason", "buttonPressed"));
	m_executor.submit( [this]() {
	    std::unique_lock<std::mutex> lock(m_mutex);
        
         if (ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING == m_currentState  ||
             ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED == m_currentState ) {
             if(flag_playControl_pause == 1){
 
                 AISDK_INFO(LX("buttonPressedPlayback").d("flag_playControl_pause", "resume"));
                 if( !m_speechPlayer->resume(m_mediaSourceId)){
                    AISDK_ERROR(LX("executeTrackChanged").d("resume","failed"));
                 }
                flag_playControl_pause = 0;                    
 
             }else{
                AISDK_INFO(LX("buttonPressedPlayback").d("flag_playControl_pause", "pause"));
                 if( !m_speechPlayer->pause(m_mediaSourceId)){
                    AISDK_ERROR(LX("executeTrackChanged").d("pause","failed"));
                 } 
                flag_playControl_pause = 1;
     
             }
             
         }else{
                 AISDK_INFO(LX("buttonPressedPlaybackFailed").d("reason", "state is not PLAYING or PAUSED"));
         }
	   
	});
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
	m_handlerName.clear();
}

ResourcesPlayer::ResourcesDirectiveInfo::ResourcesDirectiveInfo(
	std::shared_ptr<nlp::DomainProxy::DirectiveInfo> directiveInfo) :
	directive{directiveInfo->directive},
	result{directiveInfo->result},
	sendCompletedMessage{false} {
}
	
void ResourcesPlayer::ResourcesDirectiveInfo::clear() {
    sendCompletedMessage = false;
}

ResourcesPlayer::ResourcesPlayer(
	std::shared_ptr<MediaPlayerInterface> mediaPlayer,
	std::shared_ptr<AudioTrackManagerInterface> trackManager) :
	DomainProxy{SPEECHNAME},
	SafeShutdown{SPEECHNAME},
	m_handlerName{SPEECHNAME, "PlayControl"},
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

void ResourcesPlayer::AnalysisNlpDataForResourcesPlayer(cJSON          * datain , std::deque<std::string> &audiourllist )
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
     AISDK_ERROR(LX("AnalysisNlpDataForResourcesPlayer").d("json Error before", cJSON_GetErrorPtr()));
     }
     else
     {
       json_answer = cJSON_GetObjectItem(json_data, "answer");
      // json_tts_url = cJSON_GetObjectItem(json_data, "tts_url");
      // json_isMultiDialog = cJSON_GetObjectItem(json_data, "isMultiDialog");
       std::cout << "json_answer =  " << json_answer->valuestring << std::endl;
      // std::cout << "json_tts_url =  " << json_tts_url->valuestring << std::endl;
      // std::cout << "json_isMultiDialog = " << json_isMultiDialog->valueint << std::endl;
 
      // audiourllist.push_back(json_tts_url->valuestring);
 
          //parameters  
          json_parameters = cJSON_GetObjectItem(json_data, "parameters"); 
         if(json_parameters != NULL) 
         {
          json_album = cJSON_GetObjectItem(json_parameters, "album");
          json_title = cJSON_GetObjectItem(json_parameters, "title");
         }
          else
          {
          AISDK_ERROR(LX("AnalysisNlpDataForResourcesPlayer").d("json_parameters", "parameters is null"));
          }
 
          //audio_list
          cJSON *js_list = cJSON_GetObjectItem(json_data, "audio_list");
           if(!js_list){
              AISDK_ERROR(LX("AnalysisNlpDataForResourcesPlayer").d("audio_list", "no audio_list!"));
           }
           
           int array_size = cJSON_GetArraySize(js_list);
           AISDK_DEBUG(LX("AnalysisNlpDataForResourcesPlayer").d("audio_list size", array_size));
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
          //std::cout << "NO: " << i << ":json_title =  "<< json_title->valuestring << std::endl;
          //std::cout << "NO: " << i << ":json_audio_url =  " << json_audio_url->valuestring << std::endl;
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


void ResourcesPlayer::AnalysisNlpDataForPlayControl(std::shared_ptr<DirectiveInfo> info, std::string &operation )
{
    auto data = info->directive->getData();
    Json::CharReaderBuilder readerBuilder;
    JSONCPP_STRING errs;
    Json::Value root;
    std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());
    if (!reader->parse(data.c_str(), data.c_str()+data.length(), &root, &errs)) {
        AISDK_ERROR(LX("handleDirective").d("reason", "parseDataKeyError"));
        return;
    }
    Json::Value parameters = root["parameters"];
    operation = parameters["operation"].asString();
    AISDK_INFO(LX("handleDirective").d("operation", operation));
}


void ResourcesPlayer::executePreHandleAfterValidation(std::shared_ptr<ResourcesDirectiveInfo> info) {
	/// To-Do parse tts url and insert chatInfo map
    /// TODO:parse tts url and insert chatInfo map - Fix me.
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
        auto nlpDomain = info->directive;
        auto dateMessage = nlpDomain->getData();
        AUDIO_URL_LIST.clear();
        flag_playControl_pause = 0 ;

        cJSON* json = NULL, *json_data = NULL;
        (void )json;
        (void )json_data;
        json_data = cJSON_Parse(dateMessage.c_str());
        AnalysisNlpDataForResourcesPlayer(json_data, AUDIO_URL_LIST);
        info->audioList = AUDIO_URL_LIST;
        cJSON_Delete(json_data);  
        AISDK_DEBUG(LX("AUDIO_URL_LIST").d("size:",  AUDIO_URL_LIST.size()));
        for(std::size_t i = 0; i< AUDIO_URL_LIST.size(); i++)
        {
        AISDK_DEBUG(LX("AUDIO_URL_LIST").d("audio_url:",  AUDIO_URL_LIST.at(i)));
        }

        //info->url = AUDIO_URL_LIST.at(0);
        //AUDIO_URL_LIST.pop_front();


      //  m_attachmentReader = info->directive->getAttachmentReader(
      //      info->directive->getMessageId(), utils::sharedbuffer::ReaderPolicy::BLOCKING);

        //auto resourceSourceId = setSource(AUDIO_URL_LIST.at(0));
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
//    std::this_thread::sleep_for( std::chrono::seconds(2));
    if (!m_trackManager->acquireChannel(CHANNEL_NAME, shared_from_this(), SPEECHNAME)) {
        static const std::string message = std::string("Could not acquire ") + CHANNEL_NAME + " for " + SPEECHNAME;
		AISDK_INFO(LX("executeHandleFailed")
					.d("reason", "CouldNotAcquireChannel")
					.d("messageId", m_currentInfo->directive->getMessageId()));
        reportExceptionFailed(info, message);
    }

    setHandlingCompleted();
    resetCurrentInfo();
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
        if (ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING == m_currentState ) {
            lock.unlock();
            if (m_currentInfo) {
                m_currentInfo->sendCompletedMessage = false;
            }
            stopPlaying();
        }
    }

}

void ResourcesPlayer::executeStateChange() {
#if 0
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
    }
#endif    
}


void ResourcesPlayer::executeTrackChanged(FocusState newTrace){
    if(m_currentFocus == newTrace){
        
    AISDK_ERROR(LX("executeTrackChanged").d("reason", "m_currentFocus == newTrace"));
        return;
    }
             
    m_currentFocus = newTrace;
    AISDK_ERROR(LX("executeTrackChanged").d("newTrace", newTrace));
    switch (newTrace) {
    case FocusState::FOREGROUND:
       switch (m_currentState) {
        case  ResourcesPlayerObserverInterface::ResourcesPlayerState::IDLE:
        case  ResourcesPlayerObserverInterface::ResourcesPlayerState::STOPPED:             
        case  ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED: 
        {
            if(AUDIO_URL_LIST.empty()){
               AISDK_ERROR(LX("executeTrackChanged").d("reason", "nullAUDIO_URL_LIST"));
               return;
            }

            playNextItem();
        }
            break;
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING:
   
            break;
        case  ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED:

            if(flag_playControl_pause == 0 ){
                AISDK_ERROR(LX("executeTrackChanged").d("flag_playControl_pause",flag_playControl_pause));
                if( !m_speechPlayer->resume(m_mediaSourceId)){
                    AISDK_ERROR(LX("executeTrackChanged").d("resume","failed"));
                }

            }else{        
               AISDK_ERROR(LX("executeTrackChanged").d("flag_playControl_pause",flag_playControl_pause));
            }

            break;
       }
   break;
    case FocusState::BACKGROUND:
        switch (m_currentState){
         case  ResourcesPlayerObserverInterface::ResourcesPlayerState::IDLE:
         case  ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED:  
         case  ResourcesPlayerObserverInterface::ResourcesPlayerState::STOPPED:             
         case  ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED: 
            
            break;
         case ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING:
            if( !m_speechPlayer->pause(m_mediaSourceId)){
                AISDK_ERROR(LX("executeTrackChanged").d("pause","failed"));
            }
            break;
        }
        
   break;
    case FocusState::NONE:
        
   break;

   }
    
}


void ResourcesPlayer::executePlaybackStarted() {
	AISDK_INFO(LX("executePlaybackStarted"));
    {
        std::lock_guard<std::mutex> lock(m_mutex);
		/// Set current state @c m_currentState to PLAYING to specify device alreay start to playback.
        setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING);
    }
	
}

void ResourcesPlayer::executePlaybackStopped(){
	AISDK_INFO(LX("executePlaybackStopped"));


    resetMediaSourceId();

    switch (m_currentState)
        {
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING:
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED:
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                /// Set current state @c m_currentState to PLAYING to specify device alreay start to playback.
                setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::STOPPED);
            }
        break;
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED:
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::STOPPED:
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::IDLE:
            if(AUDIO_URL_LIST.empty() && m_isAlreadyStopping != false ){
                  AISDK_INFO(LX("executePlaybackFinished").d("reason", "audioUrlListNull"));
                  if(m_currentFocus != FocusState::NONE){
                     m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
                  }
            }else{
            playNextItem();
            }

        break;
            
        }


}

void ResourcesPlayer::executePlaybackFinished() {
	AISDK_INFO(LX("executePlaybackFinished"));
    std::shared_ptr<ResourcesDirectiveInfo> infotest;

    if (!m_attachmentReader) {
        m_attachmentReader.reset();
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED);
        //todo something 
    }

    resetMediaSourceId();
    if(AUDIO_URL_LIST.empty()){
          AISDK_INFO(LX("executePlaybackFinished").d("reason", "audioUrlListNull"));
          if(m_currentFocus != FocusState::NONE){
             m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
          }
          
    }else{
    playNextItem();
    }


#if 0
  //  while (!AUDIO_URL_LIST.empty()) AUDIO_URL_LIST.pop_back();
     if(!AUDIO_URL_LIST.empty()){

        for(std::size_t i = 0; i< AUDIO_URL_LIST.size(); i++)
        {
        AISDK_INFO(LX("executePlaybackFinished").d("AUDIO_URL_LIST",  AUDIO_URL_LIST.at(i)));
        }

        //  std::make_shared<ResourcesDirectiveInfo>(infotest);
        //AISDK_INFO(LX("infotest->url").d("=====infotest->url======:",  AUDIO_URL_LIST.at(0)));
        
        m_mediaSourceId = m_speechPlayer->setSource(AUDIO_URL_LIST.at(0));
        m_speechPlayer->play(m_mediaSourceId);   
        AUDIO_URL_LIST.pop_front();
        onPlaybackStopped(m_mediaSourceId);


     }
#endif     
}

void ResourcesPlayer::executePlaybackPaused(){
	AISDK_INFO(LX("executePlaybackPaused"));
    {
        std::lock_guard<std::mutex> lock(m_mutex);
		/// Set current state @c m_currentState to PLAYING to specify device alreay start to playback.
        setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED);
    }
	
}


void ResourcesPlayer::executePlaybackResumed(){
	AISDK_INFO(LX("executePlaybackResumed"));
    {
        std::lock_guard<std::mutex> lock(m_mutex);
		/// Set current state @c m_currentState to PLAYING to specify device alreay start to playback.
        setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING);
    }

}


void ResourcesPlayer::executePlaybackError(const utils::mediaPlayer::ErrorType& type, std::string error) {
	std::cout << "executePlaybackError: type: " << type << " error: " << error << std::endl;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED);
    }
    resetMediaSourceId();
    executePlaybackStopped();

}


void ResourcesPlayer::playNextItem() {
	AISDK_INFO(LX("playNextItem"));
    if(m_attachmentReader != nullptr){
    	/// The following params must be set fix point.
    	const utils::AudioFormat format{
    						.encoding = aisdk::utils::AudioFormat::Encoding::LPCM,
    					   .endianness = aisdk::utils::AudioFormat::Endianness::LITTLE,
    					   .sampleRateHz = 16000,	
    					   .sampleSizeInBits = 16,
    					   .numChannels = 1,	
    					   .dataSigned = true
    	};
        m_mediaSourceId = m_speechPlayer->setSource(std::move(m_attachmentReader), &format);
    }else{
        AISDK_INFO(LX("playNextItem").d("AUDIO_URL", AUDIO_URL_LIST.at(0)));
        m_mediaSourceId = m_speechPlayer->setSource(AUDIO_URL_LIST.at(0));
        AUDIO_URL_LIST.pop_front();
    }
    
     if (MediaPlayerInterface::ERROR == m_mediaSourceId) {
 		AISDK_ERROR(LX("playNextItemFailed").d("reason", "setSourceFailed"));
         executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
     } else if (!m_speechPlayer->play(m_mediaSourceId)) {
         executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
     } else {
         // Execution of play is successful.
         m_isAlreadyStopping = false;
     }
}


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
	
    m_mediaSourceId = m_speechPlayer->setSource(std::move(m_attachmentReader), &format);
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
    m_waitOnStateChange.notify_one();
}

void ResourcesPlayer::setDesiredStateLocked(FocusState newTrace) {
    switch (newTrace) {
        case FocusState::FOREGROUND:
            m_desiredState = ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING;
            break;
        case FocusState::BACKGROUND:
            m_desiredState = ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED;
            break;
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
    if (ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING == m_currentState ) {
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



}	// namespace speechSynthesizer
}	// namespace domain
}	// namespace aisdk

