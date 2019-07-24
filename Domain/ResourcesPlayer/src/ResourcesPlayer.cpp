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

/// The name of the @c SafeShutdown
static const std::string RESOURCESNAME{"ResourcesPlayer"};

static const std::string PLAYCONTROL{"PlayControl"};

/// The duration to wait for a state change in @c onTrackChanged before failing.
static const std::chrono::seconds STATE_CHANGE_TIMEOUT{3};

/// The duration to start playing offset position.
static const std::chrono::milliseconds DEFAULT_OFFSET{0};

//add deque for store audio_list;
std::deque<std::string> AUDIO_URL_LIST; 

//add deque for store audio_id_list;
std::deque<std::string> AUDIO_ID_LIST; 

///add for use store play control state; enable = 1;unable = 0;
int flag_playControl_pause = 0; 

///add for use enable single loop;enable = 1;unable = 0;
int enable_single_loop = 0;

///add for use enable LIST loop;enable = 1;unable = 0;
int enable_list_loop = 0;
    
bool m_isStopped = false;

///add for select resources num in audiolist;
std::size_t currentItemNum = 0;

///add for select resources from;
bool enable_kugou_resources = false;

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
    AISDK_INFO(LX("preHandleDirective").d("messageId",  info->directive->getMessageId()));
    m_executor.submit([this, info]() { executePreHandle(info); });
}

void ResourcesPlayer::handleDirective(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("handleDirective").d("messageId",  info->directive->getMessageId()));
    if(info->directive->getDomain() == RESOURCESNAME ){
        
        auto predicate = [this](){ 
           switch (m_currentState) {
               case  ResourcesPlayerObserverInterface::ResourcesPlayerState::IDLE:
               case  ResourcesPlayerObserverInterface::ResourcesPlayerState::STOPPED: 
               case  ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED: 
               return true;
               case  ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED:
               case  ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING:
               return false;
               default: 
               return false; 
           }
        };
       // Block until we achieve the desired state.
        std::unique_lock<std::mutex> lock(m_mutex);
        if ( !m_waitOnStateChange.wait_for( lock, STATE_CHANGE_TIMEOUT, predicate) ){
            AISDK_ERROR(LX("RESOURCESNAME-handleDirectiveTimeout"));
        }
        m_executor.submit([this, info]() { executeHandle(info); });

    }
    else if(info->directive->getDomain() == PLAYCONTROL ){

      std::string operation;
      AnalysisNlpDataForPlayControl(info, operation);
      auto predicate = [this](){ 
         switch (m_currentState) {
             case  ResourcesPlayerObserverInterface::ResourcesPlayerState::IDLE:
             case  ResourcesPlayerObserverInterface::ResourcesPlayerState::STOPPED: 
             case  ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED: 
             return true;
             case  ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED:
             case  ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING:
             return false;
             default: 
             return false; 
         }
      };

      if(  operation == "NEXT" 
        || operation == "PREVIOUS" 
        || operation == "SWITCH" 
        || operation == "RANDOM_PLAY" ){
          // Block until we achieve the desired state.
          std::unique_lock<std::mutex> lock(m_mutex);
          if ( !m_waitOnStateChange.wait_for( lock, STATE_CHANGE_TIMEOUT, predicate) ){
              AISDK_ERROR(LX("PLAYCONTROL-handleDirectiveTimeout"));
          }
      }
        
      responsePlayControl(info, operation);
      
    }
}

void ResourcesPlayer::cancelDirective(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("cancelDirective").d("messageId",  info->directive->getMessageId()));
    m_executor.submit([this, info]() { executeCancel(info); });
}

void ResourcesPlayer::onTrackChanged(FocusState newTrace) {
    AISDK_INFO(LX("onTrackChanged").d("newTrace", newTrace));
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
	AISDK_INFO(LX("onPlaybackStarted").d("callbackSourceId", id));
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
    AISDK_INFO(LX("onPlaybackFinished").d("callbackSourceId", id));

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
    AISDK_INFO(LX("onPlaybackPaused").d("callbackSourceId", id));
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
    AISDK_INFO(LX("onPlaybackResumed").d("callbackSourceId", id));
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
    AISDK_ERROR(LX("onPlaybackError").d("callbackSourceId", id));
            
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
    //AISDK_INFO(LX("buttonPressedPlayback").d("reason", "buttonPressed"));
	m_executor.submit( [this]() {
	    std::unique_lock<std::mutex> lock(m_mutex);
        
        if (ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING == m_currentState  ||
            ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED == m_currentState ) {
            
            if(flag_playControl_pause == 1){
                AISDK_INFO(LX("buttonPressedPlayback").d("flag_playControl_pause", "resume"));
                if( !m_resourcesPlayer->resume(m_mediaSourceId)){
                    AISDK_ERROR(LX("executeTrackChanged").d("resume","failed"));
                }
                flag_playControl_pause = 0;                    
            }else{
                AISDK_INFO(LX("buttonPressedPlayback").d("flag_playControl_pause", "pause"));
                if( !m_resourcesPlayer->pause(m_mediaSourceId)){
                    AISDK_ERROR(LX("executeTrackChanged").d("pause","failed"));
                } 
                flag_playControl_pause = 1;
            }

        }else{
        AISDK_INFO(LX("buttonPressedPlaybackFailed").d("reason", "state is not PLAYING or PAUSED"));
        }
	   
	});
}

void ResourcesPlayer::asrRefreshConfiguration(
		const std::string &uid,
		const std::string &appid, 
		const std::string &key,
		const std::string &deviceId) {
	if(uid.empty() || appid.empty() || key.empty() || deviceId.empty()) {
		AISDK_ERROR(LX("asrRefreshConfigurationFailed").d("reason", "paramsIsEmpty"));
	} else {
//		AISDK_DEBUG5(LX("asrRefreshConfiguration")
//				.d("uid", uid)
//				.d("appid", appid)
//				.d("key", key)
//				.d("deviceId", deviceId));
		// TODO:
    	m_aiuiUid = uid;
        m_appId = appid;
        m_appKey = key;
        m_clientDeviceId = deviceId;
	}
}


void ResourcesPlayer::doShutdown() {
	AISDK_INFO(LX("doShutdown").d("reason", "destory"));
	m_resourcesPlayer->setObserver(nullptr);
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
    m_resourcesPlayer.reset();
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
	DomainProxy{RESOURCESNAME},
	SafeShutdown{RESOURCESNAME},
	m_handlerName{RESOURCESNAME, PLAYCONTROL},
	m_resourcesPlayer{mediaPlayer},
	m_trackManager{trackManager},
	m_mediaSourceId{MediaPlayerInterface::ERROR},
	m_currentState{ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED},
	m_desiredState{ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED},
	m_currentFocus{FocusState::NONE},
	m_isAlreadyStopping{false} {
}

void ResourcesPlayer::init() {
    m_resourcesPlayer->setObserver(shared_from_this());
}

void ResourcesPlayer::AnalysisNlpDataForResourcesPlayer(cJSON          * datain , std::deque<std::string> &audiourllist )
{
    cJSON* json = NULL, *json_data = NULL, *json_answer = NULL,
     *json_parameters = NULL, *json_title = NULL,*json_album = NULL, *json_audio_url = NULL;

     (void )json;
     (void )json_data;
     (void )json_answer;
     (void )json_parameters;
     (void )json_title;
     (void )json_album;
     (void )json_audio_url;  

     json_data = datain;
     if(!json_data) {
        AISDK_ERROR(LX("AnalysisNlpDataForResourcesPlayer").d("json Error before", cJSON_GetErrorPtr()));
     }else{
       json_answer = cJSON_GetObjectItem(json_data, "answer");
       AISDK_INFO(LX("AnalysisNlpDataForResourcesPlayer").d("json_answer", json_answer->valuestring));
 
        //parameters  
        json_parameters = cJSON_GetObjectItem(json_data, "parameters"); 
        if(json_parameters != NULL) {
            json_album = cJSON_GetObjectItem(json_parameters, "album");
            json_title = cJSON_GetObjectItem(json_parameters, "title");
        }else {
            AISDK_ERROR(LX("AnalysisNlpDataForResourcesPlayer").d("json_parameters", "parameters is null"));
        }
 
        //audio_list
        cJSON *js_list = cJSON_GetObjectItem(json_data, "audio_list");
         if(!js_list){
            AISDK_ERROR(LX("AnalysisNlpDataForResourcesPlayer").d("audio_list", "no audio_list!"));
            return;
         }
         
         int array_size = cJSON_GetArraySize(js_list);
         AISDK_DEBUG(LX("AnalysisNlpDataForResourcesPlayer").d("audio_list size", array_size));

         if(array_size == 0){
               AISDK_ERROR(LX("AnalysisNlpDataForResourcesPlayer").d("audio_list", "NULL"));
         }else{
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
         }
         
           
     }

}

void ResourcesPlayer::AnalysisAudioIdForResourcesPlayer(std::shared_ptr<DirectiveInfo> info , std::deque<std::string> &audioidlist ) {
    auto data = info->directive->getData();
    Json::CharReaderBuilder readerBuilder;
    JSONCPP_STRING errs;
    Json::Value root;
    std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());
    if (!reader->parse(data.c_str(), data.c_str()+data.length(), &root, &errs)) {
        AISDK_ERROR(LX("AnalysisAudioIdForResourcesPlayer").d("reason", "parseDataKeyError"));
        return;
    }

    if(!root.isMember("audio_list")) {
        AISDK_ERROR(LX("AnalysisAudioIdForResourcesPlayer").d("audio_list", "no audio_list!"));
        return;
    }

    Json::Value audio_List = root["audio_list"];
    int audioListSize = audio_List.size();
    for (int i = 0; i < audioListSize; (i++)) {
        std::string audio_itemid = audio_List[i]["itemid"].asString();
        AISDK_INFO(LX("AnalysisAudioIdForResourcesPlayer").d("audioIdList[i]", i+1 ).d("itemid", audio_itemid));
        audioidlist.push_back(audio_itemid);
        
        std::string audio_albumid = audio_List[i]["albumid"].asString();
        AISDK_INFO(LX("AnalysisAudioIdForResourcesPlayer").d("audioIdList[i]", i+1 ).d("albumid", audio_albumid));
        audioidlist.push_back(audio_albumid);
    }
    
}


void ResourcesPlayer::AnalysisNlpDataForPlayControl(std::shared_ptr<DirectiveInfo> info, std::string &operation ) {
    auto data = info->directive->getData();
    Json::CharReaderBuilder readerBuilder;
    JSONCPP_STRING errs;
    Json::Value root;
    std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());
    if (!reader->parse(data.c_str(), data.c_str()+data.length(), &root, &errs)) {
        AISDK_ERROR(LX("AnalysisNlpDataForPlayControl").d("reason", "parseDataKeyError"));
        return;
    }
    Json::Value parameters = root["parameters"];
    operation = parameters["operation"].asString();
    AISDK_INFO(LX("AnalysisNlpDataForPlayControl").d("operation", operation));
}


void ResourcesPlayer::AnalysisNlpDataForVolume(std::shared_ptr<DirectiveInfo> info, std::string &operation, int &volumeValue ) {
    auto data = info->directive->getData();
    Json::CharReaderBuilder readerBuilder;
    JSONCPP_STRING errs;
    Json::Value root;
    std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());
    if (!reader->parse(data.c_str(), data.c_str()+data.length(), &root, &errs)) {
        AISDK_ERROR(LX("AnalysisNlpDataForVolume").d("reason", "parseDataKeyError"));
        return;
    }
    Json::Value parameters = root["parameters"];
    operation = parameters["operation"].asString();
    AISDK_INFO(LX("AnalysisNlpDataForVolume").d("operation", operation));

    if(operation == "SET"){
        volumeValue = parameters["value"].asInt();
        AISDK_INFO(LX("AnalysisNlpDataForVolume").d("volumeValue", volumeValue));
    }
}


void ResourcesPlayer::responsePlayControl(std::shared_ptr<DirectiveInfo> info, std::string operation) {
       std::string m_operation = operation;

        if( m_operation == "PAUSE"){
              flag_playControl_pause = 1;
              if( !m_resourcesPlayer->pause(m_mediaSourceId)){
                 AISDK_ERROR(LX("responsePlayControl").d("pause","failed"));
              } 
        }else if(m_operation == "STOP" ){
              flag_playControl_pause = 1;
              if( !m_resourcesPlayer->pause(m_mediaSourceId)){
                  AISDK_ERROR(LX("responsePlayControl").d("pause","failed"));
              }  
        }else if(m_operation == "CONTINUE" ){
              flag_playControl_pause = 0;
              if( !m_resourcesPlayer->resume(m_mediaSourceId)){
                  AISDK_ERROR(LX("responsePlayControl").d("resume","failed"));
              }  
        }else if(m_operation == "NEXT" || m_operation == "SWITCH" || m_operation == "RANDOM_PLAY") {
              flag_playControl_pause = 0;
              
              if(enable_kugou_resources == 1){
                  currentItemNum = currentItemNum + 2;
                  if(currentItemNum >= AUDIO_ID_LIST.size()){
                    currentItemNum = 0;
                  }
              }else{
                  currentItemNum ++;
                  if(currentItemNum >= AUDIO_URL_LIST.size()){
                    currentItemNum = 0;
                  }
              }

              AISDK_ERROR(LX("responsePlayControl").d("currentItemNum", currentItemNum));
              m_executor.submit([this, info]() { executeHandle(info); });
        }else if(m_operation == "PREVIOUS"){
              flag_playControl_pause = 0; 

              if(enable_kugou_resources == 1){
                //kugou
                  if(currentItemNum == 0){
                      currentItemNum = AUDIO_ID_LIST.size() - 2 ;
                  }else{
                      currentItemNum = currentItemNum - 2; 
                  }

              }else{//situo
                  if(currentItemNum == 0){
                      currentItemNum = AUDIO_URL_LIST.size() - 1 ;
                  }else{
                      currentItemNum --; 
                  }
              }

              AISDK_ERROR(LX("responsePlayControl").d("currentItemNum", currentItemNum));
              m_executor.submit([this, info]() { executeHandle(info); });
        }else if(m_operation == "SINGLE_LOOP"){
              enable_single_loop = 1;
              AISDK_INFO(LX("responsePlayControl").d("SINGLE_LOOP","success"));
        }else if(m_operation == "CLOSE_SINGLE_LOOP"){
              enable_single_loop = 0;  
              AISDK_INFO(LX("responsePlayControl").d("CLOSE_SINGLE_LOOP","success"));        
        }else if(m_operation == "LIST_LOOP"){
              enable_list_loop = 1;
              AISDK_INFO(LX("responsePlayControl").d("LIST_LOOP","success"));
        }else if(m_operation == "LIST_ORDER"){
              enable_list_loop = 0;
              enable_single_loop = 0;
              AISDK_INFO(LX("responsePlayControl").d("LIST_ORDER","success"));
        }else{
            
              AISDK_ERROR(LX("responsePlayControl").d("operation","null"));
        }
        
        info->result->setCompleted();  
        AISDK_INFO(LX("responsePlayControl").d("m_operation",m_operation)
                                            .d("flag_playControl_pause",flag_playControl_pause)
                                            .d("enable_list_loop",enable_list_loop)
                                            .d("enable_single_loop",enable_single_loop)
                                            .d("currentItemNum",currentItemNum));

}

void ResourcesPlayer::executePreHandleAfterValidation(std::shared_ptr<DirectiveInfo> info) {
	/// To-Do parse tts url and insert resourcesInfo map
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

        auto dateMessage = info->directive->getData();
        Json::CharReaderBuilder readerBuilder;
        JSONCPP_STRING errs;
        Json::Value root;
        std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());
        if (!reader->parse(dateMessage.c_str(), dateMessage.c_str()+dateMessage.length(), &root, &errs)) {
            AISDK_ERROR(LX("executePreHandleAfterValidation").d("reason", "parseDataKeyError"));
            return;
        }
        Json::Value audiolist = root["audio_list"][0];
        
        if(audiolist.isMember("itemid")){
            //kugou resources     
            AISDK_INFO(LX("executePreHandleAfterValidation").d("RESOURCES_FROM", "KuGou"));
            enable_kugou_resources = true;
            m_kugouUserToken = root["kugouUserToken"].asString();
            m_kugouUserId = root["kugouUserId"].asString();
            AISDK_INFO(LX("handleDirective").d("m_kugouUserToken", m_kugouUserToken));
            AISDK_INFO(LX("handleDirective").d("m_kugouUserId", m_kugouUserId));

            AnalysisAudioIdForResourcesPlayer(info, AUDIO_ID_LIST);
            AISDK_DEBUG(LX("AUDIO_ID_LIST").d("size",  AUDIO_ID_LIST.size()));
            
        }else{
            //stormorai resources 
            AISDK_INFO(LX("executePreHandleAfterValidation").d("RESOURCES_FROM", "Stormorai or others"));
            enable_kugou_resources = false;
            cJSON* json = NULL, *json_data = NULL;
            (void )json;
            (void )json_data;
            json_data = cJSON_Parse(dateMessage.c_str());
            AnalysisNlpDataForResourcesPlayer(json_data, AUDIO_URL_LIST);
            AISDK_DEBUG(LX("AUDIO_URL_LIST").d("size:",  AUDIO_URL_LIST.size()));
            for(std::size_t i = 0; i< AUDIO_URL_LIST.size(); i++) {
                AISDK_DEBUG(LX("executePreHandleAfterValidation")
                    .d("AUDIO_URL_LIST[i]", i+1 )
                    .d("audio_url",  AUDIO_URL_LIST.at(i)));
            }
            cJSON_Delete(json_data); 
        }       
 

#endif	
}


void ResourcesPlayer::executeHandleAfterValidation(std::shared_ptr<ResourcesDirectiveInfo> info) {
    m_currentInfo = info;
    if(m_currentFocus == utils::channel::FocusState::NONE ){
        if (!m_trackManager->acquireChannel(CHANNEL_NAME, shared_from_this(), RESOURCESNAME)) {
            static const std::string message = std::string("Could not acquire ") + CHANNEL_NAME + " for " + RESOURCESNAME;
            AISDK_INFO(LX("executeHandleFailed")
                        .d("reason", "CouldNotAcquireChannel")
                        .d("messageId", m_currentInfo->directive->getMessageId()));
            reportExceptionFailed(info, message);
        }
 
    }

    //setHandlingCompleted();
    resetCurrentInfo();
}

void ResourcesPlayer::executePreHandle(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("executePreHandle").d("messageId", info->directive->getMessageId() ));
     
    if(info->directive->getDomain() == PLAYCONTROL){
        std::string operation;
        AnalysisNlpDataForPlayControl(info, operation);
        AISDK_ERROR(LX("executePreHandle").d("operation", operation));
        if(    operation == "PAUSE" 
            || operation == "STOP" 
            || operation == "CLOSE_SINGLE_LOOP" 
            || operation == "CONTINUE" 
            || operation == "SINGLE_LOOP" 
            || operation == "LIST_LOOP" 
            || operation == "LIST_ORDER" ){
                 return;
        }
        if( !m_resourcesPlayer->stop(m_mediaSourceId)){
            AISDK_ERROR(LX("executePreHandle").d("stop","failed"));
        }
    }else if(info->directive->getDomain() == RESOURCESNAME){
        //initialization parameters 
        AUDIO_URL_LIST.clear();
        AUDIO_ID_LIST.clear();
        flag_playControl_pause = 0 ;
        currentItemNum = 0;
        enable_single_loop = 0;
        enable_list_loop = 0; 

        if( !m_resourcesPlayer->stop(m_mediaSourceId)){
            AISDK_ERROR(LX("executePreHandle").d("stop","failed"));
        }

        AISDK_INFO(LX("executePreHandle")
            .d(" initialization parameters ", "AUDIO_URL_LIST cleared ")
            .d(" flag_playControl_pause ", flag_playControl_pause)
            .d(" currentItemNum ", currentItemNum)
            .d(" enable_list_loop ", enable_list_loop)
            .d(" enable_single_loop ", enable_single_loop));
        executePreHandleAfterValidation(info);
    }

}

void ResourcesPlayer::executeHandle(std::shared_ptr<DirectiveInfo> info) {
	AISDK_INFO(LX("executeHandle").d("messageId", info->directive->getMessageId()));
    auto resourcesInfo = validateInfo("executeHandle", info);
    if (!resourcesInfo) {
		AISDK_ERROR(LX("executeHandleFailed").d("reason", "invalidDirectiveInfo"));
      //  return;
    }
    addToDirectiveQueue(resourcesInfo);
}

void ResourcesPlayer::handlePlayDirective(std::shared_ptr<DirectiveInfo> info) {
	AISDK_INFO(LX("handlePlayDirective").d("messageId", info->directive->getMessageId()));
    m_executor.submit([this, info]() { executePlay(info); });
}

void ResourcesPlayer::executePlay(std::shared_ptr<DirectiveInfo> info) {

    stopPlaying();

    {
        // Block until we achieve the desired state.
       std::unique_lock<std::mutex> lock(m_mutexStopRequest);
        if ( !m_waitOnStopChange.wait_for( lock, STATE_CHANGE_TIMEOUT, []() {return m_isStopped; })){
            AISDK_ERROR(LX("executePlayWaitStopStateTimeout"));
        }
    }

    m_isStopped = false;
    
     switch (m_currentState) {
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING:
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED:
            //stopPlaying();
            //AISDK_INFO(LX("executeTrackChanged = clean current resources and stop player state ")); 
            //flag_playControl_pause = 0;
        break;
        case  ResourcesPlayerObserverInterface::ResourcesPlayerState::IDLE:
        case  ResourcesPlayerObserverInterface::ResourcesPlayerState::STOPPED:             
        case  ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED:  
            AISDK_INFO(LX("executePlay").d("m_currentFocus", m_currentFocus));
            if(m_currentFocus == FocusState::NONE) {
                if (!m_trackManager->acquireChannel(CHANNEL_NAME, shared_from_this(), RESOURCESNAME)) {
                    static const std::string message = std::string("Could not acquire ") + CHANNEL_NAME + " for " + RESOURCESNAME;
                    AISDK_INFO(LX("executePlaysFailed")
                       .d("reason", "CouldNotAcquireChannel")
                       .d("messageId", m_currentInfo->directive->getMessageId()));
                   // reportExceptionFailed(info, message);
                }
            }
        break;
     }

     AISDK_INFO(LX("executePlay").d("reason", "stating completed..."));
    info->result->setCompleted();
}


void ResourcesPlayer::executeCancel(std::shared_ptr<DirectiveInfo> info) {
	AISDK_INFO(LX("executeCancel").d("messageId", info->directive->getMessageId()));
    info->result->setCompleted();
    auto resourcesInfo = validateInfo("executeCancel", info);
    if (!resourcesInfo) {
        AISDK_ERROR(LX("executeCancelFailed").d("reason", "invalidDirectiveInfo"));
        return;
    }
    if (resourcesInfo != m_currentInfo) {
        resourcesInfo->clear();
        removeChatDirectiveInfo(resourcesInfo->directive->getMessageId());
        {
            std::lock_guard<std::mutex> lock(m_chatInfoQueueMutex);
            for (auto it = m_chatInfoQueue.begin(); it != m_chatInfoQueue.end(); it++) {
                if (resourcesInfo->directive->getMessageId() == it->get()->directive->getMessageId()) {
                    it = m_chatInfoQueue.erase(it);
                    break;
                }
            }
        }
        removeDirective(resourcesInfo->directive->getMessageId());
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
    AISDK_INFO(LX("executeTrackChanged").d("newTrace", newTrace));
    switch (newTrace) {
    case FocusState::FOREGROUND:
       switch (m_currentState) {
        case  ResourcesPlayerObserverInterface::ResourcesPlayerState::IDLE:
        case  ResourcesPlayerObserverInterface::ResourcesPlayerState::STOPPED:             
        case  ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED: 
            {
                if(enable_kugou_resources == true){
                    
                        if(AUDIO_ID_LIST.empty()){
                           AISDK_ERROR(LX("executeTrackChanged").d("reason", "AUDIO_ID_LIST is null"));
                           return;
                        }
                        AISDK_DEBUG5(LX("executeTrackChanged").d("playKuGouResourceItemID", "play type-->-->[kugou]-->-->【1】"));
                        if(currentItemNum >= AUDIO_ID_LIST.size()){
                            if(enable_list_loop == 1){
                                currentItemNum = 0;
                            }else{
                                if( !m_resourcesPlayer->stop(m_mediaSourceId)){
                                AISDK_ERROR(LX("executeTrackChanged").d("stop","failed"));
                                }
                                break;
                            }
                        }
                        TryTheNextItem:
                        int val = playKuGouResourceItemID(AUDIO_ID_LIST.at(currentItemNum), AUDIO_ID_LIST.at(currentItemNum+1));  
                        if(val != 0){
                           AISDK_DEBUG5(LX("executeTrackChanged").d("playKuGouResourceItemID", "play type-->-->[kugou]-->-->【2】"));
                           currentItemNum = currentItemNum + 2;
                           if(currentItemNum >= AUDIO_ID_LIST.size()){
                                if(enable_list_loop == 1){
                                    currentItemNum = 0;
                                }else{
                                    if( !m_resourcesPlayer->stop(m_mediaSourceId)){
                                    AISDK_ERROR(LX("executeTrackChanged").d("stop","failed"));
                                    }
                                    break;
                                } 
                           }                           
                           goto TryTheNextItem;
                        }

                }else{
                
                        if(AUDIO_URL_LIST.empty()){
                           AISDK_ERROR(LX("executeTrackChanged").d("reason", "AUDIO_URL_LIST is null"));
                           return;
                        }
                        AISDK_DEBUG5(LX("executeTrackChanged").d("playResourceItem", "play type-->-->[stormorai or others]-->-->【1】"));
                        if(currentItemNum >= AUDIO_URL_LIST.size()){
                            if(enable_list_loop == 1){
                                currentItemNum = 0;
                            }else{
                                if( !m_resourcesPlayer->stop(m_mediaSourceId)){
                                AISDK_ERROR(LX("executeTrackChanged").d("stop","failed"));
                                }
                                break;
                            }

                        }
                        playResourceItem(AUDIO_URL_LIST.at(currentItemNum));   
                }
                


            }
            break;
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING:
   
            break;
        case  ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED:

            if(flag_playControl_pause == 0 ){
                AISDK_INFO(LX("executeTrackChanged").d("flag_playControl_pause",flag_playControl_pause));
                if( !m_resourcesPlayer->resume(m_mediaSourceId)){
                    AISDK_ERROR(LX("executeTrackChanged").d("resume","failed"));
                }

            }else{        
               AISDK_INFO(LX("executeTrackChanged").d("flag_playControl_pause",flag_playControl_pause));
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
            if( !m_resourcesPlayer->pause(m_mediaSourceId)){
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
    if(m_currentState != ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING ){
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            /// Set current state @c m_currentState to PLAYING to specify device alreay start to playback.
            setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING);
        }
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
                if(m_currentFocus != FocusState::NONE){
                    m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
                 }
                std::lock_guard<std::mutex> lock(m_mutex);
                /// Set current state @c m_currentState to PLAYING to specify device alreay start to playback.
                setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::STOPPED);
            }
        break;
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED:
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::STOPPED:
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::IDLE:
#if 0            
            if(AUDIO_URL_LIST.empty() && AUDIO_ID_LIST.empty() && m_isAlreadyStopping != false ){
                 AISDK_INFO(LX("executePlaybackFinished").d("reason", "audioUrlList and audioIDList is Null"));
                 if(m_currentFocus != FocusState::NONE){
                    m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
                 }
            }else{

                if(enable_kugou_resources == true){
                     AISDK_DEBUG5(LX("executeTrackChanged").d("playKuGouResourceItemID", "play type----->2"));
                     if(currentItemNum >= AUDIO_ID_LIST.size()){
                         //currentItemNum = 0;
                         if( !m_resourcesPlayer->stop(m_mediaSourceId)){
                             AISDK_ERROR(LX("executeTrackChanged").d("stop","failed"));
                         }

                         break;
                     }
                     playKuGouResourceItemID(AUDIO_ID_LIST.at(currentItemNum));  

                }else{
                     // playNextItem();
                     AISDK_DEBUG5(LX("executePlaybackStopped").d("playResourceItem", "play type----->2"));
                     if(currentItemNum >= AUDIO_URL_LIST.size()){
                        //currentItemNum = 0;
                         if( !m_resourcesPlayer->stop(m_mediaSourceId)){
                             AISDK_ERROR(LX("executeTrackChanged").d("stop","failed"));
                         }

                        break;
                     }
                     playResourceItem(AUDIO_URL_LIST.at(currentItemNum));
                }

            }
#endif
        break;
            
        }
}

void ResourcesPlayer::executePlaybackFinished() {
	AISDK_INFO(LX("executePlaybackFinished"));
    std::shared_ptr<ResourcesDirectiveInfo> infotest;
    if(m_currentState != ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED){
         if (!m_attachmentReader) {
             m_attachmentReader.reset();
         }
         {
             std::lock_guard<std::mutex> lock(m_mutex);
             setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED);
             //todo something 
         }
         
         resetMediaSourceId();
         
         //Play only once when the audio resource is just one.
         if(AUDIO_URL_LIST.size() == 1 ) {
             AUDIO_URL_LIST.clear();
             AISDK_INFO(LX("executePlaybackFinished").d("reason", "AUDIO_URL_LIST.size=1"));
         }
         if(AUDIO_ID_LIST.size() == 1 ) {
             AUDIO_ID_LIST.clear();
             AISDK_INFO(LX("executePlaybackFinished").d("reason", "AUDIO_ID_LIST.size=1"));
         }   
         
         
         if(AUDIO_URL_LIST.empty() && AUDIO_ID_LIST.empty()){
               AISDK_INFO(LX("executePlaybackFinished").d("reason", "audioUrlListNull"));
               if( !m_resourcesPlayer->stop(m_mediaSourceId)){
                   AISDK_ERROR(LX("executePlaybackFinished").d("stop","failed"));
               }

               
         }else{
              if(enable_kugou_resources == true) {
                  AISDK_DEBUG5(LX("executePlaybackFinished").d("playKuGouResourceItemID", "play type-->-->[kugou]-->-->【3】"));
                  
                  TryAgain:
                  currentItemNum = currentItemNum + 2;
                  if(enable_single_loop == 1 ){
                      currentItemNum = currentItemNum - 2;
                      AISDK_DEBUG5(LX("executePlaybackFinished").d(" playKuGouResourceItemID", "enter to single_loop")
                                                                .d(" enable_single_loop", enable_single_loop));
                    }
                  if(currentItemNum >= AUDIO_ID_LIST.size()){
                       if(enable_list_loop == 1){
                           currentItemNum = 0;
                       }else{
                           if( !m_resourcesPlayer->stop(m_mediaSourceId)){
                           AISDK_ERROR(LX("executePlaybackFinished").d("stop","failed"));
                           }
                           AISDK_INFO(LX("executePlaybackFinished").d("reason", "End of Plaging List!"));
                           return;
                       } 
                  }
                  int val = playKuGouResourceItemID(AUDIO_ID_LIST.at(currentItemNum), AUDIO_ID_LIST.at(currentItemNum+1));
                  if(val != 0){
                     AISDK_DEBUG5(LX("executePlaybackFinished").d("playKuGouResourceItemID", "play type-->-->[kugou]-->-->【4】"));                          
                     goto TryAgain;
                  }                  
         
              }else{
                  // playNextItem();
                  AISDK_DEBUG5(LX("executePlaybackFinished").d("playResourceItem", "play type-->-->[stormorai or others]-->-->【3】"));
                  currentItemNum ++;
                  if(enable_single_loop == 1 ){
                      currentItemNum --;
                      AISDK_DEBUG5(LX("executePlaybackFinished").d(" playResourceItem", "enter to single_loop")
                                                                .d(" enable_single_loop", enable_single_loop));
                    }
                  if(currentItemNum >= AUDIO_URL_LIST.size()){
                        if(enable_list_loop == 1){
                            currentItemNum = 0;
                        }else{
                            if( !m_resourcesPlayer->stop(m_mediaSourceId)){
                            AISDK_ERROR(LX("executePlaybackFinished").d("stop","failed"));
                            }
                            AISDK_INFO(LX("executePlaybackFinished").d("reason", "End of Plaging List!"));
                            return;
                        }                 
                  }
                 playResourceItem(AUDIO_URL_LIST.at(currentItemNum));
              }
         
         }

    }



}

void ResourcesPlayer::executePlaybackPaused(){
	AISDK_INFO(LX("executePlaybackPaused"));
    if(m_currentState != ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED){

        {
            std::lock_guard<std::mutex> lock(m_mutex);
    		/// Set current state @c m_currentState to PLAYING to specify device alreay start to playback.
            setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::PAUSED);
        }
    }	
}


void ResourcesPlayer::executePlaybackResumed(){
    if(m_currentState != ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING){

    	AISDK_INFO(LX("executePlaybackResumed"));
        {
            std::lock_guard<std::mutex> lock(m_mutex);
    		/// Set current state @c m_currentState to PLAYING to specify device alreay start to playback.
            setCurrentStateLocked(ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING);
        }
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
        m_mediaSourceId = m_resourcesPlayer->setSource(std::move(m_attachmentReader), &format);
    }else{

         if( !AUDIO_URL_LIST.empty()){
             AISDK_INFO(LX("playNextItem").d("AUDIO_URL", AUDIO_URL_LIST.at(0)));
             m_mediaSourceId = m_resourcesPlayer->setSource(AUDIO_URL_LIST.at(0));
             AUDIO_URL_LIST.pop_front();
         }

    }
    
     if (MediaPlayerInterface::ERROR == m_mediaSourceId) {
 		AISDK_ERROR(LX("playNextItemFailed").d("reason", "setSourceFailed"));
         executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
     } else if (!m_resourcesPlayer->play(m_mediaSourceId)) {
         executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
     } else {
         // Execution of play is successful.
         m_isAlreadyStopping = false;
     }
}

void ResourcesPlayer::playResourceItem(std::string ResourceItem ) {
    AISDK_INFO(LX("playResourceItem").d("ResourceItem", ResourceItem));
    m_mediaSourceId = m_resourcesPlayer->setSource(ResourceItem);
    
     if (MediaPlayerInterface::ERROR == m_mediaSourceId) {
 		AISDK_ERROR(LX("playNextItemFailed").d("reason", "setSourceFailed"));
         executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
     } else if (!m_resourcesPlayer->play(m_mediaSourceId)) {
         executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
     } else {
         // Execution of play is successful.
         m_isAlreadyStopping = false;
     }
}

int ResourcesPlayer::playKuGouResourceItemID(std::string ResourceItemID, std::string ResourceAlbumIdID){
    AISDK_INFO(LX("playKuGouResourceItemID").d("ResourceItemID", ResourceItemID).d("ResourceAlbumIdID", ResourceAlbumIdID));
    //KUGOU API   
    char audioUrl[1024];
    memset(audioUrl, 0x00, sizeof(audioUrl));
    const char* aiuiUid = m_aiuiUid.c_str();
    const char* appId = m_appId.c_str();
    const char* appKey = m_appKey.c_str();
    const char* clientDeviceId = m_clientDeviceId.c_str();
    //const char* aiuiUid = "d13007661883";
    //const char* appId = "5c3d8827";
    //const char* appKey = "914a80c3399a12e993559f79ae898205";
    //const char* clientDeviceId = "250b4299265f4bcbe0dc8d147aaa3aa";
    const char* kugouUserId = m_kugouUserId.c_str();
    const char* kugouUserToken = m_kugouUserToken.c_str();
    const char* itemId = ResourceItemID.c_str();
    const char* albumId = ResourceAlbumIdID.c_str();

    //AISDK_INFO(LX("playKuGouResourceItemID").d("appKey", appKey).d("appId", appId));   //加密id
    //AISDK_INFO(LX("playKuGouResourceItemID").d("aiuiUid", aiuiUid).d("clientDeviceId", clientDeviceId));
    //AISDK_INFO(LX("playKuGouResourceItemID").d("kugouUserId", kugouUserId).d("kugouUserToken", kugouUserToken));

    int val = getMusicUrl(aiuiUid, appId, appKey, kugouUserId, kugouUserToken, clientDeviceId, itemId, albumId, audioUrl);
    if(val != 0 || strlen(audioUrl) == 0 ){
        AISDK_ERROR(LX("playKuGouResourceItemID").d("getMusicUrl", "NULL"));
        return -1;
    }else{
        playResourceItem(std::string (audioUrl));
    }
    return 0;
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
	
    m_mediaSourceId = m_resourcesPlayer->setSource(std::move(m_attachmentReader), &format);
	#else
	m_mediaSourceId = m_resourcesPlayer->setSource(m_currentInfo->url);
	#endif
    if (MediaPlayerInterface::ERROR == m_mediaSourceId) {
		AISDK_ERROR(LX("startPlayingFailed").d("reason", "setSourceFailed"));
        executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "playFailed");
    } else if (!m_resourcesPlayer->play(m_mediaSourceId)) {
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
    } else if (!m_resourcesPlayer->stop(m_mediaSourceId)) {
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

void ResourcesPlayer::resetCurrentInfo(std::shared_ptr<ResourcesDirectiveInfo> resourcesInfo) {
    if (m_currentInfo != resourcesInfo) {
        if (m_currentInfo) {
            removeChatDirectiveInfo(m_currentInfo->directive->getMessageId());
			// Removing map of @c DomainProxy's @c m_directiveInfoMap
            removeDirective(m_currentInfo->directive->getMessageId());
            m_currentInfo->clear();
        }
        m_currentInfo = resourcesInfo;
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
       // std::lock_guard<std::mutex> lock(m_mutex);
       // m_currentFocus = FocusState::NONE;
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

    auto resourcesDirInfo = getResourcesDirectiveInfo(info->directive->getMessageId());
    if (resourcesDirInfo) {
        return resourcesDirInfo;
    }

    resourcesDirInfo = std::make_shared<ResourcesDirectiveInfo>(info);

    return resourcesDirInfo;

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

    executeHandleAfterValidation(info);
    info->result->setCompleted();
	AISDK_INFO(LX("addToDirectiveQueue").d("executeHandleAfterValidation"," info->result->setCompleted();"));
}

void ResourcesPlayer::removeChatDirectiveInfo(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    m_chatDirectiveInfoMap.erase(messageId);
}



}	// namespace ResourcesPlayer
}	// namespace domain
}	// namespace aisdk

