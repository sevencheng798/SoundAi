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
#include <sqlite3.h>

#include "AlarmsPlayer/AlarmsPlayer.h"
#include <Utils/cJSON.h>
#include "string.h"

#include<deque>  
#include <Utils/Logging/Logger.h>

//#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>   //sleep 


/// String to identify log entries originating from this file.
static const std::string TAG{"AlarmsPlayer"};

#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

//using namespace std;
namespace aisdk {
namespace domain {
namespace alarmsPlayer {

using namespace utils::mediaPlayer;
using namespace utils::channel;
using namespace dmInterface;

/// The name of the @c AudioTrackManager channel used by the @c SpeechSynthesizer.
static const std::string CHANNEL_NAME = AudioTrackManagerInterface::DIALOG_CHANNEL_NAME;

/// The name of DomainProxy and SpeechChat handler interface
//static const std::string SPEECHCHAT{"SpeechChat"};

/// The name of the @c SafeShutdown
static const std::string SPEECHNAME{"AlarmsPlayer"};

/// The duration to wait for a state change in @c onTrackChanged before failing.
static const std::chrono::seconds STATE_CHANGE_TIMEOUT{5};

/// The duration to start playing offset position.
static const std::chrono::milliseconds DEFAULT_OFFSET{0};

//add deque for store TTS_URL_LIST;
std::deque<std::string> TTS_URL_LIST; 

//add deque for store repeat_alarm;
std::deque<std::string> REPEAT_ALARM_LIST; 



std::shared_ptr<AlarmsPlayer> AlarmsPlayer::create(
	std::shared_ptr<MediaPlayerInterface> mediaPlayer,
	std::shared_ptr<utils::attachment::AttachmentManagerInterface> ttsDocker,
	std::shared_ptr<asr::GenericAutomaticSpeechRecognizer> asrEngine,
	std::shared_ptr<AudioTrackManagerInterface> trackManager,
	std::shared_ptr<utils::dialogRelay::DialogUXStateRelay> dialogUXStateRelay){
	if(!mediaPlayer){
        
        AISDK_ERROR(LX("AlarmsPlayerCreationFailed").d("reason: ", "mediaPlayerNull"));
		return nullptr;
	}

	if(!trackManager){
        AISDK_ERROR(LX("AlarmsPlayerCreationFailed").d("reason: ", "trackManagerNull"));
		return nullptr;
	}

	if(!dialogUXStateRelay){
        AISDK_ERROR(LX("AlarmsPlayerCreationFailed").d("reason: ", "dialogUXStateRelayNull"));
		return nullptr;
	}

	auto instance = std::shared_ptr<AlarmsPlayer>(new AlarmsPlayer(mediaPlayer, ttsDocker, asrEngine, trackManager));
	if(!instance){
        AISDK_ERROR(LX("AlarmsPlayerCreationFailed").d("reason: ", "NewAlarmsPlayerFailed"));
		return nullptr;
	}
	instance->init();

	dialogUXStateRelay->addObserver(instance);

	return instance;
}

void AlarmsPlayer::onDeregistered() {
	// default no-op
}

void AlarmsPlayer::preHandleDirective(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("preHandleDirective").d("messageId: ",  info->directive->getMessageId()));
    m_executor.submit([this, info]() { executePreHandle(info); });
}

void AlarmsPlayer::handleDirective(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("handleDirective").d("messageId: ",  info->directive->getMessageId()));
    m_executor.submit([this, info]() { executeHandle(info); });
}

void AlarmsPlayer::cancelDirective(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("cancelDirective").d("messageId: ",  info->directive->getMessageId()));
    m_executor.submit([this, info]() { executeCancel(info); });
}

void AlarmsPlayer::onTrackChanged(FocusState newTrace) {
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
            setCurrentStateLocked(AlarmsPlayerObserverInterface::AlarmsPlayerState::GAINING_FOCUS);
            break;
        case FocusState::BACKGROUND:
            setCurrentStateLocked(AlarmsPlayerObserverInterface::AlarmsPlayerState::LOSING_FOCUS);
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

void AlarmsPlayer::onPlaybackStarted(SourceId id) {
	AISDK_INFO(LX("AlarmsPlayeronPlaybackStarted").d("callbackSourceId：", id));
    if (id != m_mediaSourceId) {
		AISDK_ERROR(LX("queueingExecutePlaybackStartedFailed").d("reason:mismatchSourceId:callbackSourceId:", id));
    
        m_executor.submit([this] {
            executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "executePlaybackStartedFailed");
        });
    } else {
        m_executor.submit([this]() { executePlaybackStarted(); });
    }

}

void AlarmsPlayer::onPlaybackFinished(SourceId id) {
    AISDK_INFO(LX("onPlaybackFinished").d("callbackSourceId:", id));

    if (id != m_mediaSourceId) {
        AISDK_ERROR(LX("queueingExecutePlaybackFinishedFailed").d("reason:mismatchSourceId:callbackSourceId:", id));
            
        m_executor.submit([this] {
            executePlaybackError(ErrorType::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "executePlaybackFinishedFailed");
        });
    } else {
        m_executor.submit([this]() { executePlaybackFinished(); });
    }
}

void AlarmsPlayer::onPlaybackError(
	SourceId id,
	const utils::mediaPlayer::ErrorType& type,
	std::string error) {
    AISDK_ERROR(LX("onPlaybackError").d("callbackSourceId:", id));
            
    m_executor.submit([this, type, error]() { executePlaybackError(type, error); });
}

void AlarmsPlayer::onPlaybackStopped(SourceId id) {
	std::cout << "onPlaybackStopped:callbackSourceId: " << id << std::endl;
    onPlaybackFinished(id);
}

void AlarmsPlayer::addObserver(std::shared_ptr<AlarmsPlayerObserverInterface> observer) {
	std::cout << __func__ << ":addObserver:observer: " << observer.get() << std::endl;
    m_executor.submit([this, observer]() { m_observers.insert(observer); });
}

void AlarmsPlayer::removeObserver(std::shared_ptr<AlarmsPlayerObserverInterface> observer) {
	std::cout << __func__ << ":removeObserver:observer: " << observer.get() << std::endl;	
    m_executor.submit([this, observer]() { m_observers.erase(observer); }).wait();
}

std::unordered_set<std::string> AlarmsPlayer::getHandlerName() const {
	return m_handlerName;
}

void AlarmsPlayer::doShutdown() {
	std::cout << "doShutdown" << std::endl;
	m_speechPlayer->setObserver(nullptr);
	{
        std::unique_lock<std::mutex> lock(m_mutex);
        if (AlarmsPlayerObserverInterface::AlarmsPlayerState::PLAYING == m_currentState ||
            AlarmsPlayerObserverInterface::AlarmsPlayerState::PLAYING == m_desiredState) {
            m_desiredState = AlarmsPlayerObserverInterface::AlarmsPlayerState::FINISHED;
            stopPlaying();
            m_currentState = AlarmsPlayerObserverInterface::AlarmsPlayerState::FINISHED;
            lock.unlock();
            releaseForegroundTrace();
        }
    }
	
    {
        std::lock_guard<std::mutex> lock(m_chatInfoQueueMutex);
        for (auto& info : m_chatInfoQueue) {
            if (info.get()->result) {
                info.get()->result->setFailed("AlarmsPlayerShuttingDown");
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

AlarmsPlayer::AlarmDirectiveInfo::AlarmDirectiveInfo(
	std::shared_ptr<nlp::DomainProxy::DirectiveInfo> directiveInfo) :
	directive{directiveInfo->directive},
	result{directiveInfo->result},
	sendCompletedMessage{false} {
}
	
void AlarmsPlayer::AlarmDirectiveInfo::clear() {
    attachmentReader.reset();
    sendCompletedMessage = false;
}

AlarmsPlayer::AlarmsPlayer(
	std::shared_ptr<MediaPlayerInterface> mediaPlayer,
    std::shared_ptr<utils::attachment::AttachmentManagerInterface> ttsDocker,
	std::shared_ptr<asr::GenericAutomaticSpeechRecognizer> asrEngine,
	std::shared_ptr<AudioTrackManagerInterface> trackManager) :
	DomainProxy{SPEECHNAME},
	SafeShutdown{SPEECHNAME},
	m_handlerName{SPEECHNAME},
	m_speechPlayer{mediaPlayer},
	m_ttsDocker{ttsDocker},
	m_asrEngine{asrEngine},
	m_trackManager{trackManager},
	m_mediaSourceId{MediaPlayerInterface::ERROR},
	m_currentState{AlarmsPlayerObserverInterface::AlarmsPlayerState::FINISHED},
	m_desiredState{AlarmsPlayerObserverInterface::AlarmsPlayerState::FINISHED},
	m_currentFocus{FocusState::NONE},
	m_isAlreadyStopping{false} {
}


/* Create random UUID */
/* @param uuid[37] - buffer to be filled with the uuid string */
const char* AlarmsPlayer::CreateRandomUuid(char *uuid)
{
    const char *c = "89ab";
    char *p = uuid;
    int n;
    for( n = 0; n < 16; ++n ) {
        int b = rand()%255;
        switch(n)
        {
            case 6:
                sprintf(p, "4%x", b%15 );
            break;
            case 8:
                sprintf(p, "%c%x", c[rand()%strlen(c)], b%15 );
            break;
            default:
                sprintf(p, "%02x", b);
            break;
        }
        p += 2;
        switch(n) {
            case 3:
            case 5:
            case 7:
            case 9:
                *p++ = '-';
                break;
        }
    }
    *p = 0;
    return uuid;
}



unsigned int AlarmsPlayer::getMorningTime() 
{  
    time_t t = time(NULL); 
    struct tm * tm= localtime(&t);  
    tm->tm_hour = 0;  
    tm->tm_min = 0;  
    tm->tm_sec = 0;  
    return  mktime(tm);  
}  
    
void AlarmsPlayer::CheckAlarmList(sqlite3 *db)
{
    int nrow;
    int ncolumn;
    char *zErrMsg =NULL;
    char **azResult=NULL;
    char deleteAlarmTime[512];
    int nrowContent;
    int ncolumnContent;
    char *zErrMsgContent =NULL;
    char **azResultContent=NULL;
    char deleteAlarmContent[512];

    char const *alarmSql= "select *from alarm;";
    sqlite3_get_table( db , alarmSql , &azResult , &nrow , &ncolumn , &zErrMsg );
    //AISDK_DEBUG(LX("CheckAlarmList").d("alarm::nrow", nrow).d(" ncolumn", ncolumn));

    if((nrow >= 1) && (ncolumn != 0))
    {
        char const *minsql = "select min(timestamp) from alarm;";
        sqlite3_get_table( db , minsql , &azResult , &nrow , &ncolumn , &zErrMsg );

        // AISDK_DEBUG(LX("CheckAlarmList").d("min(timestamp) from alarm:", azResult[nrow*ncolumn]));        
        long int alarmtimesec = (long int)(atoll(azResult[nrow*ncolumn])/1000);   //long long int --> long int;
        // struct tm *alarmp;
        // alarmp = localtime(&alarmtimesec);
        // printf("AlarmsPlayer:sqliteThreadHander::first alarm time:%d-%d-%d %d:%d:%d\n", 1900+alarmp->tm_year, 1+alarmp->tm_mon, alarmp->tm_mday, alarmp->tm_hour, alarmp->tm_min, alarmp->tm_sec);

        // current time
        time_t timesec;
        // struct tm *p;
        time(&timesec);
        // p = localtime(&timesec);         
        // printf("AlarmsPlayer:sqliteThreadHander::current system time:%d-%d-%d %d:%d:%d\n", 1900+p->tm_year, 1+p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);


        if((timesec/20) == (alarmtimesec/20)) {
            sprintf(deleteAlarmContent, "select content from alarm where timestamp = %s;" ,azResult[nrow*ncolumn]);
            sqlite3_get_table( db , deleteAlarmContent , &azResultContent , &nrowContent , &ncolumnContent , &zErrMsgContent );
            AISDK_DEBUG(LX("deleteAlarmContent").d("select content from alarm where timestamp", azResultContent[nrowContent*ncolumnContent]));        
            std::string currentContent = azResultContent[nrowContent*ncolumnContent];

            for(int i = 0; i < 1; i++) { //'i' use for set repeat times;
                AISDK_INFO(LX("AlarmsPlayer").d("sqliteThreadHander", "alarm time is coming!"));

                char contentId[37];
                CreateRandomUuid(contentId);
                auto writer = m_ttsDocker->createWriter(contentId);
                auto reader = m_ttsDocker->createReader(contentId, utils::sharedbuffer::ReaderPolicy::BLOCKING);
                AISDK_DEBUG(LX("deleteAlarmContent").d("currentContent", currentContent));
                m_asrEngine->acquireTextToSpeech(currentContent, std::move(writer));

                utils::AudioFormat format {
                        .encoding = aisdk::utils::AudioFormat::Encoding::LPCM,
                        .endianness = aisdk::utils::AudioFormat::Endianness::LITTLE,
                        .sampleRateHz = 16000,
                        .sampleSizeInBits = 16,
                        .numChannels = 1,
                        .dataSigned = true };

                auto sourceId = m_speechPlayer->setSource(std::move(reader), &format);
                m_speechPlayer->play(sourceId);
            }
            sprintf(deleteAlarmTime, "delete from alarm where timestamp = %s;" ,azResult[nrow*ncolumn]);
            sqlite3_exec( db , deleteAlarmTime , NULL , NULL , &zErrMsg );
            sqlite3_free(zErrMsg);
        }else if((timesec/20) > (alarmtimesec/20)) { 
            sprintf(deleteAlarmTime, "delete from alarm where timestamp = %s;" ,azResult[nrow*ncolumn]);
            sqlite3_exec( db , deleteAlarmTime , NULL , NULL , &zErrMsg );
            sqlite3_free(zErrMsg);
        }

    }

}


void AlarmsPlayer::CheckRepeatAlarmList(sqlite3 *db)
{
    int nrow;
    int ncolumn;
    char *zErrMsg =NULL;
    char **azResult=NULL;
    int nrowContent;
    int ncolumnContent;
    char *zErrMsgContent =NULL;
    char **azResultContent=NULL;
    char deleteAlarmContent[1024];
    int nrowWeekday;
    int ncolumWeekday;
    char *zErrMsgWeekday =NULL;
    char **azResultWeekday=NULL;
    char deleteAlarmWeekday[1024];

    char const *alarmSql_repeat ="select *from alarmList_repeat;";    
    sqlite3_get_table( db , alarmSql_repeat , &azResult , &nrow , &ncolumn , &zErrMsg );

    // current time
    time_t timesec;
    struct tm *p;
    time(&timesec);
    p = localtime(&timesec);         
    int m_weekday = p->tm_wday; 

    if((nrow >= 1) && (ncolumn != 0)) {
        for(int i=5;i<(nrow+1)*ncolumn;i=i+5) {
            //AISDK_DEBUG(LX("CheckRepeatAlarmList").d("NO:", (i/5)).d("repeat alarm time ", azResult[i]));           
            long int alarmtimesec =(long int)( getMorningTime()+ (atoll(azResult[i])/1000));
       
            sprintf(deleteAlarmContent, "select content from alarmList_repeat where timestamp_day = %s;" ,azResult[i]);
            sqlite3_get_table( db , deleteAlarmContent , &azResultContent , &nrowContent , &ncolumnContent , &zErrMsgContent );
            //AISDK_DEBUG5(LX("deleteAlarmContent").d("select content from alarmList_repeat where timestamp_day", azResultContent[nrowContent*ncolumnContent]));        
            std::string currentContent = azResultContent[nrowContent*ncolumnContent];
            
            sprintf(deleteAlarmWeekday, "select weekday from alarmList_repeat where timestamp_day = %s;" ,azResult[i]);
            sqlite3_get_table( db , deleteAlarmWeekday , &azResultWeekday , &nrowWeekday , &ncolumWeekday , &zErrMsgWeekday );
            //AISDK_DEBUG5(LX("deleteAlarmWeekday").d("select weekday from alarmList_repeat where timestamp_day", azResultWeekday[nrowWeekday*ncolumWeekday]));        
            std::string currentWeekday = azResultWeekday[nrowWeekday*ncolumWeekday];

           if( atoi(currentWeekday.c_str()) == 0 || atoi(currentWeekday.c_str()) == m_weekday ){
                //AISDK_DEBUG5(LX("deleteAlarmWeekday").d("currentWeekday", atoi(currentWeekday.c_str()) ) );        
                 if((timesec/20) == (alarmtimesec/20)) {    
                     AISDK_INFO(LX("CheckRepeatAlarmList").d(" currentContent", currentContent)
                                                          .d(" currentWeekday", currentWeekday)); 
                     for(int i = 0; i < 1; i++) {
                          AISDK_INFO(LX("AlarmsPlayer").d("sqliteThreadHander", "alarm time is coming!"));
     
                          char contentId[37];
                          CreateRandomUuid(contentId);
                          auto writer = m_ttsDocker->createWriter(contentId);
                          auto reader = m_ttsDocker->createReader(contentId, utils::sharedbuffer::ReaderPolicy::BLOCKING);
                          AISDK_DEBUG(LX("deleteAlarmContent").d("currentContent", currentContent));
                          m_asrEngine->acquireTextToSpeech(currentContent, std::move(writer));
                          
                          utils::AudioFormat format {
                                  .encoding = aisdk::utils::AudioFormat::Encoding::LPCM,
                                  .endianness = aisdk::utils::AudioFormat::Endianness::LITTLE,
                                  .sampleRateHz = 16000,
                                  .sampleSizeInBits = 16,
                                  .numChannels = 1,
                                  .dataSigned = true };
                          
                          auto sourceId = m_speechPlayer->setSource(std::move(reader), &format);
                          m_speechPlayer->play(sourceId);
                          
                     }
                 }

           }       
        }
    }

}


void AlarmsPlayer::sqliteThreadHander() {
    sqlite3 *db=NULL;
    int len;
    while(1){
        len = sqlite3_open("/data/alarm.db",&db);
        if( len ) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
        }

        CheckAlarmList(db);
        CheckRepeatAlarmList(db);
        sqlite3_close(db);
        // sleep(10);
        std::this_thread::sleep_for( std::chrono::seconds(10));
    }
}


void AlarmsPlayer::init() {
    m_speechPlayer->setObserver(shared_from_this());  
    //add for check sqlite3 db
    m_sqliteThread = std::thread(&AlarmsPlayer::sqliteThreadHander, this);
}

//--------------------add by wx @190402-----------------
#if 1
void AnalysisNlpDataForAlarmsPlayer(cJSON          * datain , std::deque<std::string> &ttsurllist );

void AnalysisNlpDataForAlarmsPlayer(cJSON          * datain , std::deque<std::string> &ttsurllist )
{

     const char *ALARM_SET_OPERATION = "SET";
     const char *ALARM_DELETE_OPERATION = "DELETE";
     const char *ALARM_FLUSH_OPERATION = "FLUSH";
     const char *ALARM_UPDATE_OPERATION = "UPDATE";

     sqlite3 *db=NULL;
     int len;
     char *zErrMsg =NULL;
     char alarmSql[1024];
     char deleteSql[1024];
     long long int timestamp = 0;
     long long int repeat_timestamp_day = 0;
     int repeat_weekday = 0;
     char evt_type[64];
     int action_type = 0;
     int loop_mask = 0;
     char content[1024];
     
    cJSON* json = NULL,
    *json_data = NULL, *json_answer = NULL,
    *json_parameters = NULL, *json_event = NULL, *json_operation = NULL, *json_timestamp = NULL,
    *json_repeat = NULL, *json_repeat_type = NULL, *json_repeat_timestamp_day = NULL ,*json_repeat_weekday = NULL;
     
     (void )json;
     (void )json_data;
     (void )json_answer;
     (void )json_parameters;
     (void )json_event;
     (void )json_operation;
     (void )json_timestamp;   

     (void )json_repeat;
     (void )json_repeat_type;
     (void )json_repeat_timestamp_day;
     (void )json_repeat_weekday;


      json_data = datain;  
      if(!json_data)
      {
      AISDK_ERROR(LX("json_data").d("json Error before:", cJSON_GetErrorPtr()));
      }
      else
      {
         json_answer = cJSON_GetObjectItem(json_data, "answer");
         AISDK_DEBUG(LX("json_data").d("json_answer", json_answer->valuestring));
      
         //parameters  
         json_parameters = cJSON_GetObjectItem(json_data, "parameters"); 
         if(json_parameters != NULL) 
         { 
          json_operation = cJSON_GetObjectItem(json_parameters, "operation");
          AISDK_DEBUG(LX("json_parameters").d("json_operation", json_operation->valuestring));


          json_repeat = cJSON_GetObjectItem(json_parameters, "repeat");
          if(json_repeat != NULL)
          //repeat alarm
          {
            int array_size = cJSON_GetArraySize(json_repeat);
            std::cout << "repeat_alarm_list size : " <<array_size << std::endl;
            int i = 0;
            cJSON *item,*it;
            char *p  = NULL;
            for(i=0; i< array_size; i++) {
              item = cJSON_GetArrayItem(json_repeat, i);
              if(!item) {
                 //TODO...
              }
              p = cJSON_PrintUnformatted(item);
              it = cJSON_Parse(p);
              if(!it)
                 continue ;
            json_repeat_timestamp_day = cJSON_GetObjectItem(it, "timestamp_day");
            json_repeat_type = cJSON_GetObjectItem(it, "type");
            json_repeat_weekday = cJSON_GetObjectItem(it, "weekday");
            AISDK_DEBUG(LX("json_parameters").d("No:", i).d("json_repeat_timestamp_day", json_repeat_timestamp_day->valuestring));
            AISDK_DEBUG(LX("json_parameters").d("No:", i).d("json_repeat_type", json_repeat_type->valuestring));
            REPEAT_ALARM_LIST.push_back(json_repeat_timestamp_day->valuestring);
            }

          }
          else
          //one time alarm
          {
          AISDK_DEBUG(LX("-------one time alarm--------------i'm here !!!!-----------------"));
          if(strcmp(json_operation->valuestring, ALARM_FLUSH_OPERATION) != 0)
            {
            json_timestamp = cJSON_GetObjectItem(json_parameters, "timestamp");
            AISDK_DEBUG(LX("json_parameters").d("json_timestamp", json_timestamp->valuestring));
            }  
          }

           /* 打开数据库 */
            len = sqlite3_open("/data/alarm.db",&db);
            if( len )
            {
                //fprintf函数格式化输出错误信息到指定的stderr文件流中  
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                //sqlite3_errmsg(db)用以获得数据库打开错误码的英文描述。
                sqlite3_close(db);
                exit(1);
            }
            else 
                AISDK_DEBUG(LX("You have opened a sqlite3 database named alarm.db successfully created by WX!\n"));
            
          /* 创建表 */
          char const *alarmList = " CREATE TABLE alarm(timestamp, evt_type, action_type, loop_mask, content); " ;
          sqlite3_exec(db,alarmList,NULL,NULL,&zErrMsg);
          sqlite3_free(zErrMsg);
          char const *alarmList_repeat = " CREATE TABLE alarmList_repeat(timestamp_day, evt_type, weekday, loop_mask, content); " ;   
          sqlite3_exec(db,alarmList_repeat,NULL,NULL,&zErrMsg);
          sqlite3_free(zErrMsg);

                   
          AISDK_DEBUG(LX("json_parameters").d("json_operation", json_operation->valuestring));
        ////operation type:SET 
          if( strcmp(json_operation->valuestring, ALARM_SET_OPERATION) == 0)
          {
          AISDK_INFO(LX("AnalysisNlpDataForAlarmsPlayer").d("OPERATION:","SET")); 
          
          if(json_repeat != NULL)
          //repeat alarm
          {
          std::string repeat_type = json_repeat_type->valuestring;
           if(repeat_type == "WEEKLY" ){
              repeat_weekday = json_repeat_weekday->valueint;
           }else{
              repeat_weekday = 0;
           }
          
          json_event = cJSON_GetObjectItem(json_parameters, "event");  
          AISDK_DEBUG(LX("json_parameters").d("json_event", json_event->valuestring));
          sprintf(evt_type,"%s", json_event->valuestring);
          
          repeat_timestamp_day = atoll(json_repeat_timestamp_day->valuestring);      
          AISDK_DEBUG(LX("repeat_timestamp_day").d("value:", repeat_timestamp_day));

          long int timesec = (long int)((repeat_timestamp_day/1000) - 28800);
          printf("%ld \n", timesec);
          struct tm *p;
          p = localtime(&timesec);

          if(json_event != NULL){ 
              sprintf(content, "重复闹钟：现在是北京时间%d点%d分，您有一个提醒%s时间到了",   p->tm_hour, p->tm_min, evt_type);
          }
          else
          {
              sprintf(content, "重复闹钟：现在是北京时间%d点%d分，您有一个提醒时间到了",   p->tm_hour, p->tm_min);
          }
          
          loop_mask = 1;
          
          sprintf(alarmSql, "INSERT INTO 'alarmList_repeat'VALUES(%lld, '%s', %d, %d, '%s');" ,repeat_timestamp_day ,evt_type ,repeat_weekday ,loop_mask ,content);
          sqlite3_exec(db, alarmSql, NULL, NULL, &zErrMsg);  

          
          }
          else
          //one time alarm
          {  
          json_event = cJSON_GetObjectItem(json_parameters, "event");  
          AISDK_DEBUG(LX("json_parameters").d("json_event", json_event->valuestring));
          sprintf(evt_type,"%s", json_event->valuestring);
          
          timestamp = atoll(json_timestamp->valuestring);    //把字符串转换成长长整型数（64位）long long atoll(const char *nptr);
          AISDK_DEBUG(LX("timestamp").d("value:", timestamp));
          
          long int timesec = (long int)(timestamp/1000);   //long long int --> long int;
          printf("%ld \n", timesec);
          struct tm *p ;
          p = localtime(&timesec);

          //AISDK_INFO(LX("SET ALARM").d("set alarm time:", asctime(p)));
          if(json_event != NULL){
              sprintf(content, "现在是北京时间%d年%d月%d日%d点%d分，您有一个提醒%s时间到了",
                1900+p->tm_year,
                1+p->tm_mon,
                p->tm_mday, 
                p->tm_hour, 
                p->tm_min,
                evt_type);
          }
          else{
              sprintf(content, "现在是北京时间%d年%d月%d日%d点%d分，您有一个提醒时间到了",
                1900+p->tm_year,
                1+p->tm_mon,
                p->tm_mday, 
                p->tm_hour, 
                p->tm_min);
          }
          
          AISDK_INFO(LX("Set Alarm Time:").d("content:", content));
          action_type = 1;
          loop_mask = 0;
          sprintf(alarmSql, "INSERT INTO 'alarm'VALUES(%lld, '%s', %d, %d, '%s');" ,timestamp ,evt_type ,action_type ,loop_mask ,content);
          sqlite3_exec(db, alarmSql, NULL, NULL, &zErrMsg);  
          }

          
          }
          
          //operation type:DELETE 
          if(strcmp(json_operation->valuestring, ALARM_DELETE_OPERATION) == 0)
          {
             AISDK_INFO(LX("AnalysisNlpDataForAlarmsPlayer").d("OPERATION:","DELETE"));
             if(json_repeat != NULL)
            //repeat alarm
            {
            repeat_timestamp_day = atoll(json_repeat_timestamp_day->valuestring);      
            AISDK_DEBUG(LX("repeat_timestamp_day").d("value:", repeat_timestamp_day));
            sprintf(deleteSql, "delete from alarmList_repeat where timestamp_day = %lld;" ,repeat_timestamp_day);
            sqlite3_exec(db, deleteSql, NULL, NULL, &zErrMsg);
            sqlite3_free(zErrMsg); 
            }
            else
            //one time alarm
            {
             timestamp = atoll(json_timestamp->valuestring);    //把字符串转换成长长整型数（64位）long long atoll(const char *nptr);
             AISDK_DEBUG(LX("timestamp").d("value:", timestamp));   
             sprintf(deleteSql, "delete from alarm where timestamp = %lld;" ,timestamp);
             sqlite3_exec(db, deleteSql, NULL, NULL, &zErrMsg);
             printf("zErrMsg = %s \n", zErrMsg);
             sqlite3_free(zErrMsg); 
            }
             
          }
          
          //operation type:FLUSH 
          if(strcmp(json_operation->valuestring, ALARM_FLUSH_OPERATION) == 0)
          {
             AISDK_INFO(LX("AnalysisNlpDataForAlarmsPlayer").d("OPERATION:","FLUSH"));   
             char const *flushSql = "drop table alarm;";
             sqlite3_exec(db, flushSql, NULL, NULL, &zErrMsg);
             printf("zErrMsg = %s \n", zErrMsg);
             sqlite3_free(zErrMsg); 
             char const *flushSqlRep = "drop table alarmList_repeat;";
             sqlite3_exec(db, flushSqlRep, NULL, NULL, &zErrMsg);
             printf("zErrMsg = %s \n", zErrMsg);
             sqlite3_free(zErrMsg); 
          
             /* 创建表 */
              char const *rebulidalarmList = " CREATE TABLE alarm(timestamp, evt_type, action_type, loop_mask, content ); " ;
              sqlite3_exec(db,rebulidalarmList,NULL,NULL,&zErrMsg);
              sqlite3_free(zErrMsg);      
              char const *rebulidalarmList_repeat = " CREATE TABLE alarmList_repeat(timestamp_day, evt_type, weekday, loop_mask, content ); " ;
              sqlite3_exec(db,rebulidalarmList_repeat,NULL,NULL,&zErrMsg);
              sqlite3_free(zErrMsg);                 
           }
          
          //operation type:UPDATE 
          if(strcmp(json_operation->valuestring, ALARM_UPDATE_OPERATION) == 0)
          {
             AISDK_INFO(LX("AnalysisNlpDataForAlarmsPlayer").d("OPERATION:","UPDATE"));   
             //add operation
             //...
          }
          
          sqlite3_close(db);
         }
          else
          {
              AISDK_INFO(LX("parameters is null "));
          }

      }
    //   ttsurllist.push_back(json_tts_url->valuestring);
      
}

#endif

void AlarmsPlayer::executePreHandleAfterValidation(std::shared_ptr<AlarmDirectiveInfo> info) {
	/// To-Do parse tts url and insert chatInfo map
    /// add by wx @201904
   
#ifdef ENABLE_SOUNDAI_ASR
     auto nlpDomain = info->directive;
     auto dateMessage = nlpDomain->getData();
     std::cout << "dateMessage =  " << dateMessage.c_str() << std::endl;

     cJSON* json = NULL, *json_data = NULL;
     (void )json;
     (void )json_data;
     json_data = cJSON_Parse(dateMessage.c_str());
     AnalysisNlpDataForAlarmsPlayer(json_data, TTS_URL_LIST);
     info->url = TTS_URL_LIST.at(0);
     
     AISDK_INFO(LX("alarmplayer").d("当前播放内容:", info->url ));

#else
        auto nlpDomain = info->directive;
        auto dateMessage = nlpDomain->getData();
        AISDK_INFO(LX("executePreHandleAfterValidation").d("dateMessage", dateMessage.c_str()));

        cJSON* json = NULL, *json_data = NULL;
        (void )json;
        (void )json_data;
        json_data = cJSON_Parse(dateMessage.c_str());
        AnalysisNlpDataForAlarmsPlayer(json_data, TTS_URL_LIST);
        cJSON_Delete(json_data);  

//        info->attachmentReader = info->directive->getAttachmentReader(
//                info->directive->getMessageId(), utils::sharedbuffer::ReaderPolicy::BLOCKING);
    
#endif
//    if (!setAlarmDirectiveInfo(info->directive->getMessageId(), info)) {
//		AISDK_ERROR(LX("executePreHandleFailed")
//					.d("reason", "prehandleCalledTwiceOnSameDirective")
//					.d("messageId", info->directive->getMessageId()));
//    }
}

void AlarmsPlayer::executeHandleAfterValidation(std::shared_ptr<AlarmDirectiveInfo> info) {
    m_currentInfo = info;
    AISDK_INFO(LX("executeHandleAfterValidation").d("m_currentInfo-url: ", m_currentInfo->url ));
    if (!m_trackManager->acquireChannel(CHANNEL_NAME, shared_from_this(), SPEECHNAME)) {
        static const std::string message = std::string("Could not acquire ") + CHANNEL_NAME + " for " + SPEECHNAME;
        AISDK_ERROR(LX("executeHandleFailed").d("reason:CouldNotAcquireChannel:messageId:", m_currentInfo->directive->getMessageId()));
        reportExceptionFailed(info, message);
    }
}

void AlarmsPlayer::executePreHandle(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("executePreHandle").d("messageId: ", info->directive->getMessageId() ));
    auto chatInfo = validateInfo("executePreHandle", info);
    if (!chatInfo) {
        AISDK_ERROR(LX("executePreHandleFailed").d("reason", "invalidDirectiveInfo"));
        return;
    }
    executePreHandleAfterValidation(chatInfo);
}

void AlarmsPlayer::executeHandle(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("executeHandle").d("messageId: ", info->directive->getMessageId() ));
    auto chatInfo = validateInfo("executeHandle", info);
    if (!chatInfo) {
        AISDK_ERROR(LX("executeHandleFailed").d("reason", "invalidDirectiveInfo"));
        return;
    }
    addToDirectiveQueue(chatInfo);
}

void AlarmsPlayer::executeCancel(std::shared_ptr<DirectiveInfo> info) {
    AISDK_INFO(LX("executeCancel").d("messageId: ", info->directive->getMessageId() ));
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
    if (AlarmsPlayerObserverInterface::AlarmsPlayerState::FINISHED != m_desiredState) {
        m_desiredState = AlarmsPlayerObserverInterface::AlarmsPlayerState::FINISHED;
        if (AlarmsPlayerObserverInterface::AlarmsPlayerState::PLAYING == m_currentState ||
            AlarmsPlayerObserverInterface::AlarmsPlayerState::GAINING_FOCUS == m_currentState) {
            lock.unlock();
            if (m_currentInfo) {
                m_currentInfo->sendCompletedMessage = false;
            }
            stopPlaying();
        }
    }

}

void AlarmsPlayer::executeStateChange() {
    AlarmsPlayerObserverInterface::AlarmsPlayerState newState;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        newState = m_desiredState;
    }

    AISDK_INFO(LX("executeStateChange").d("newState: ", newState ));
    switch (newState) {
        case AlarmsPlayerObserverInterface::AlarmsPlayerState::PLAYING:
            if (m_currentInfo) {
                m_currentInfo->sendCompletedMessage = true;
            }
			// Trigger play
            startPlaying();
            break;
        case AlarmsPlayerObserverInterface::AlarmsPlayerState::FINISHED:
			// Trigger stop
            stopPlaying();
            break;
        case AlarmsPlayerObserverInterface::AlarmsPlayerState::GAINING_FOCUS:
        case AlarmsPlayerObserverInterface::AlarmsPlayerState::LOSING_FOCUS:
            // Nothing to do
            break;
    }
}

void AlarmsPlayer::executePlaybackStarted() {
	std::cout << "executePlaybackStarted." << std::endl;
	
    if (!m_currentInfo) {
		std::cout << "executePlaybackStartedIgnored:reason:nullptrDirectiveInfo" << std::endl;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
		/// Set current state @c m_currentState to PLAYING to specify device alreay start to playback.
        setCurrentStateLocked(AlarmsPlayerObserverInterface::AlarmsPlayerState::PLAYING);
    }
	
	/// wakeup condition wait
    m_waitOnStateChange.notify_one();
}

void AlarmsPlayer::executePlaybackFinished() {
	std::cout << "executePlaybackFinished." << std::endl;
    std::shared_ptr<AlarmDirectiveInfo> infotest;

    if (!m_currentInfo) {
        
        AISDK_INFO(LX("executePlaybackFinishedIgnored").d("reason: ", "nullptrDirectiveInfo" ));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        setCurrentStateLocked(AlarmsPlayerObserverInterface::AlarmsPlayerState::FINISHED);
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

}

void AlarmsPlayer::executePlaybackError(const utils::mediaPlayer::ErrorType& type, std::string error) {
	std::cout << "executePlaybackError: type: " << type << " error: " << error << std::endl;
    if (!m_currentInfo) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        setCurrentStateLocked(AlarmsPlayerObserverInterface::AlarmsPlayerState::FINISHED);
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

void AlarmsPlayer::startPlaying() {
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


void AlarmsPlayer::stopPlaying() {
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


void AlarmsPlayer::setCurrentStateLocked(
	AlarmsPlayerObserverInterface::AlarmsPlayerState newState) {
    AISDK_INFO(LX("setCurrentStateLocked").d("state", newState));
    m_currentState = newState;

    for (auto observer : m_observers) {
        observer->onStateChanged(m_currentState);
    }
}

void AlarmsPlayer::setDesiredStateLocked(FocusState newTrace) {
    switch (newTrace) {
        case FocusState::FOREGROUND:
            m_desiredState = AlarmsPlayerObserverInterface::AlarmsPlayerState::PLAYING;
            break;
        case FocusState::BACKGROUND:
        case FocusState::NONE:
            m_desiredState = AlarmsPlayerObserverInterface::AlarmsPlayerState::FINISHED;
            break;
    }
}

void AlarmsPlayer::resetCurrentInfo(std::shared_ptr<AlarmDirectiveInfo> chatInfo) {
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

void AlarmsPlayer::setHandlingCompleted() {
	std::cout << "setHandlingCompleted" << std::endl;
    if (m_currentInfo && m_currentInfo->result) {
        m_currentInfo->result->setCompleted();
    }
}

void AlarmsPlayer::reportExceptionFailed(
	std::shared_ptr<AlarmDirectiveInfo> info,
	const std::string& message) {
    if (info && info->result) {
        info->result->setFailed(message);
    }
    info->clear();
    removeDirective(info->directive->getMessageId());
    std::unique_lock<std::mutex> lock(m_mutex);
    if (AlarmsPlayerObserverInterface::AlarmsPlayerState::PLAYING == m_currentState ||
        AlarmsPlayerObserverInterface::AlarmsPlayerState::GAINING_FOCUS == m_currentState) {
        lock.unlock();
        stopPlaying();
    }
}

void AlarmsPlayer::releaseForegroundTrace() {
	std::cout << "releaseForegroundTrace" << std::endl;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentFocus = FocusState::NONE;
    }
    m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
}

void AlarmsPlayer::resetMediaSourceId() {
    m_mediaSourceId = MediaPlayerInterface::ERROR;
}

std::shared_ptr<AlarmsPlayer::AlarmDirectiveInfo> AlarmsPlayer::validateInfo(
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

    auto chatDirInfo = getAlarmDirectiveInfo(info->directive->getMessageId());
    if (chatDirInfo) {
        return chatDirInfo;
    }

    chatDirInfo = std::make_shared<AlarmDirectiveInfo>(info);

    return chatDirInfo;

}

std::shared_ptr<AlarmsPlayer::AlarmDirectiveInfo> AlarmsPlayer::getAlarmDirectiveInfo(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    auto it = m_chatDirectiveInfoMap.find(messageId);
    if (it != m_chatDirectiveInfoMap.end()) {
        return it->second;
    }
    return nullptr;
}

bool AlarmsPlayer::setAlarmDirectiveInfo(
	const std::string& messageId,
	std::shared_ptr<AlarmsPlayer::AlarmDirectiveInfo> info) {
	std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    auto it = m_chatDirectiveInfoMap.find(messageId);
    if (it != m_chatDirectiveInfoMap.end()) {
        return false;
    }
    m_chatDirectiveInfoMap[messageId] = info;
    return true;
}

void AlarmsPlayer::addToDirectiveQueue(std::shared_ptr<AlarmDirectiveInfo> info) {
    std::lock_guard<std::mutex> lock(m_chatInfoQueueMutex);

    if (m_chatInfoQueue.empty()) {
        m_chatInfoQueue.push_back(info);
        executeHandleAfterValidation(info);
    } else {
        AISDK_INFO(LX("addToDirectiveQueue").d("queueSize: ", m_chatInfoQueue.size()));
        m_chatInfoQueue.push_back(info);
    }
}

void AlarmsPlayer::removeChatDirectiveInfo(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_chatDirectiveInfoMutex);
    m_chatDirectiveInfoMap.erase(messageId);
}

void AlarmsPlayer::onDialogUXStateChanged(
	utils::dialogRelay::DialogUXStateObserverInterface::DialogUXState newState) {
	std::cout << "onDialogUXStateChanged" << std::endl;
	m_executor.submit([this, newState]() { executeOnDialogUXStateChanged(newState); });
}

void AlarmsPlayer::executeOnDialogUXStateChanged(
    utils::dialogRelay::DialogUXStateObserverInterface::DialogUXState newState) {
	std::cout << "executeOnDialogUXStateChanged" << std::endl;
    if (newState != utils::dialogRelay::DialogUXStateObserverInterface::DialogUXState::IDLE) {
        return;
    }
    if (m_currentFocus != FocusState::NONE &&
        m_currentState != AlarmsPlayerObserverInterface::AlarmsPlayerState::GAINING_FOCUS) {
        m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
        m_currentFocus = FocusState::NONE;
    }
}



#if 0

void testsqlites()
{
     sqlite3 *db=NULL;
     int len;

     char *zErrMsg =NULL;
   //  char **azResult=NULL; //二维数组存放结果
     /* 打开数据库 */
     len = sqlite3_open("alarm.db",&db);  //create test db named user.db 
     if( len )
     {
        /*  fprintf函数格式化输出错误信息到指定的stderr文件流中  */
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
            //sqlite3_errmsg(db)用以获得数据库打开错误码的英文描述。
        sqlite3_close(db);
        exit(1);
     }
     else printf("You have opened a sqlite3 database named user.db successfully created by WX!\n");
 
     /* 创建表 */
     
      char const *sql = " CREATE TABLE alarmlist(num, time, repeat, enable, execute ); " ;
      //char *sql = (char*)" CREATE TABLE TestData(num,time,repeat,enable,execute); " ;
      sqlite3_exec(db,sql,NULL,NULL,&zErrMsg);
      sqlite3_free(zErrMsg);
      char alarm1[100];
      int a = 1;
      char b[100] = "aaaaaa";
      sprintf(alarm1, "INSERT INTO 'alarmlist'VALUES(%d, '%s', 1, 0, 0);" ,a ,b);
      //char const *alarm1 = "INSERT INTO 'alarmlist'VALUES(1, 12334455, 1, 0, 0);"; 
      sqlite3_exec(db,alarm1,NULL,NULL,&zErrMsg);
      char const *alarm2 = "INSERT INTO 'alarmlist'VALUES(2, 55567767, 1, 0, 1);"; 
      sqlite3_exec(db,alarm2,NULL,NULL,&zErrMsg);
      char const *alarm3 = "INSERT INTO 'alarmlist'VALUES(3, 89898989, 1, 1, 1);"; 
      sqlite3_exec(db,alarm3,NULL,NULL,&zErrMsg);



#if 0

      /*插入数据  */
      char *sql1 ="INSERT INTO 'TestData'VALUES(0,11,22,33,44);";
      sqlite3_exec(db,sql1,NULL,NULL,&zErrMsg);
      char*sql2 ="INSERT INTO 'TestData'VALUES(1,111,222,333,444);";
      sqlite3_exec(db,sql2,NULL,NULL,&zErrMsg);
      char*sql3 ="INSERT INTO 'TestData'VALUES(2,1111,2222,3333,4444);";
      sqlite3_exec(db,sql3,NULL,NULL,&zErrMsg);
 
      /* 查询数据 */
      sql="select *from TestData";
      sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );
      printf("nrow=%d ncolumn=%d\n",nrow,ncolumn);
      printf("the result is:\n");
      for(i=0;i<(nrow+1)*ncolumn;i++)
        {
          printf("=======here!!!======azResult[%d]=%s\n",i,azResult[i]);
        }
 
     /* 删除某个特定的数据 */
      sql="delete from TestData where ID = 1;";
      sqlite3_exec( db , sql , NULL , NULL , &zErrMsg );
      printf("zErrMsg = %s \n", zErrMsg);
      sqlite3_free(zErrMsg);
 
      /* 查询删除后的数据 */
      sql = "SELECT * FROM TestData ";
      sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );
      printf( "row:%d column=%d\n " , nrow , ncolumn );
      printf( "After deleting , the result is : \n" );
      for( i=0 ; i<( nrow + 1 ) * ncolumn ; i++ )
      {
            printf( "azResult[%d] = %s\n", i , azResult[i] );
      }
      sqlite3_free_table(azResult);
   printf("zErrMsg = %s \n", zErrMsg);
   sqlite3_free(zErrMsg);
#endif 
      sqlite3_close(db);
   //   return 0;

}

#endif

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

