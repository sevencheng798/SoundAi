/*
 * Copyright 2019 its affiliates. All Rights Reserved.
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
#include <string>
#include <Utils/Logging/Logger.h>

#include "Application/UIManager.h"
#include <dirent.h>


static const std::string TAG{"UIManager"};
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)
//audio dir path in devices;
char wakeUpAudioPath[] = "/cfg/sai_config/wakeup" ;
//to check the read audio dir time when starting up or wake up; 
int flag_Time_read_audioDir = 0; 

//use for send msg to ipc.
struct MqSndInfo m_mqSndInfo;

namespace aisdk {
namespace application {
    
std::deque<std::string> WAKEUP_AUDIO_LIST; 

static const std::string HELP_MESSAGE =
		"\n+----------------------------------------------------------------------------+\n"
		"|                              Options:                                      |\n"
		"| Info:                                                                      |\n"
        "|       Press 'i' followed by Enter at any time to see the help screen.      |\n"
        "| Microphone off:                                                            |\n"
        "|       Press 'm' and Enter to turn on and off the microphone.               |\n"
		"| Playback to control:                                                       |\n"
		"|        Press 'p' and Enter to PLAY and PAUSE state                         |\n"
		"| Stop an interaction:                                                       |\n"
		"|        Press 's' and Enter to stop an ongoing interaction.                 |\n";

static const std::string CONNECTION_MESSAGE =
		"\n#############################################\n"
		"########    Network Disconnected    ###########\n"
		"#############################################\n";

static const std::string IDLE_MESSAGE =
		"\n#############################################\n"
		"########## NLP Client is currently IDLE! ####\n"
		"#############################################\n";

static const std::string LISTEN_MESSAGE =
		"\n#############################################\n"
		"##########    LISTENING          ############\n"
		"#############################################\n";

static const std::string SPEAK_MESSAGE =
		"\n#############################################\n"
		"##########    SPEAKING          #############\n"
		"#############################################\n";

static const std::string THINK_MESSAGE =
		"\n#############################################\n"
		"##########    THINKING          #############\n"
		"#############################################\n";

UIManager::UIManager():
	m_dialogState{DialogUXStateObserverInterface::DialogUXState::IDLE} {

}

int UIManager::creatMsg(MqSndInfo mqSndInfo){
        MQ_SND_INFO_T snd_info;
        ssize_t ret = 0;
        char* tmp = NULL;
        memset(&snd_info, 0x00, sizeof(MQ_SND_INFO_T));
        tmp = (char*)malloc(sizeof(char)*255);

        if (NULL != tmp)
        {
            memset(tmp, 0x00, sizeof(char)*255);
            // snprintf(tmp, sizeof(char)* 255, "");
        }
        snd_info.msg_info.msg_type = GM_MSG_SAMPLE;
        snd_info.msg_info.sub_msg_info.sub_id = mqSndInfo.msg_info.sub_msg_info.sub_id;
        snd_info.msg_info.sub_msg_info.status = mqSndInfo.msg_info.sub_msg_info.status;
        memcpy(snd_info.msg_info.sub_msg_info.content, tmp, sizeof(char)* 255);
        snd_info.msg_info.sub_msg_info.content_len = sizeof(char) * 255;
        snd_info.msg_info.sub_msg_info.iparam = 0xffffffff;
        snd_info.mq_flag = IPC_NOWAIT;

        AISDK_INFO(LX("IPC::creatMsg")
            .d("msg_type ",GM_MSG_SAMPLE)
            .d("mode ",mqSndInfo.msg_info.sub_msg_info.sub_id)
            .d("status ",mqSndInfo.msg_info.sub_msg_info.status));
        ret = mq_send(&snd_info);
        if (0 > ret)
        {
            return -1;
        }
        
        free(tmp);
        tmp = NULL;

        return 0;

}


void UIManager::onDialogUXStateChanged(DialogUXStateObserverInterface::DialogUXState newState) {
	m_executor.submit([this, newState]() {
			if(m_dialogState == newState)
				return;

			m_dialogState = newState;
			printState();
		});
}

void UIManager::onNetworkStatusChanged(const Status newState) {
	m_executor.submit([this, newState]() {
			//if(m_connectionState == newState)
			//	return;
			
			m_connectionState = newState;
	        if(m_connectionState == utils::NetworkStateObserverInterface::Status::DISCONNECTED) {
		        AISDK_INFO(LX(CONNECTION_MESSAGE));
                system("cvlc /cfg/sai_config/net_connecting.mp3 --play-and-exit &");
		
	        }else if(m_connectionState == utils::NetworkStateObserverInterface::Status::CONNECTED) {
			
			}
		});
}

void UIManager::printErrorScreen() {
    m_executor.submit([]() { AISDK_INFO(LX("Invalid Option")); });
}

void UIManager::printHelpScreen() {
    m_executor.submit([]() { AISDK_INFO(LX(HELP_MESSAGE)); });
}

void UIManager::microphoneOff() {
	// TODO: LED to indicate.
    memset(&m_mqSndInfo, 0x00, sizeof(m_mqSndInfo));   
    m_mqSndInfo.msg_info.sub_msg_info.sub_id = LED_MODE_MUTE;
    m_mqSndInfo.msg_info.sub_msg_info.status = 1;
    creatMsg(m_mqSndInfo);

    system("cvlc /cfg/sai_config/mic_close.mp3 --play-and-exit &");
    std::this_thread::sleep_for( std::chrono::seconds(2));
    m_executor.submit([]() { AISDK_INFO(LX("Microphone Off!")); });
}

void UIManager::microphoneOn() {
    memset(&m_mqSndInfo, 0x00, sizeof(m_mqSndInfo));   
    system("cvlc /cfg/sai_config/mic_open.mp3 --play-and-exit &");
    m_mqSndInfo.msg_info.sub_msg_info.sub_id = LED_MODE_MUTE;
    m_mqSndInfo.msg_info.sub_msg_info.status = 0;
    creatMsg(m_mqSndInfo);

    m_executor.submit([this]() { printState(); });
}

void UIManager::readWakeupAudioDir(char *path, std::deque<std::string> &wakeUpAudioList)
{
    AISDK_INFO(LX("readWakeupAudioDir").d("Wake Up Audio Dir Path ",path));
    struct dirent* ent = NULL;
    DIR *pDir;
    pDir=opendir(path);
    //d_reclen：16表示子目录或以.开头的隐藏文件，24表示普通文本文件,28为二进制文件，还有其他.
    //d_type：4表示为目录，8表示为文件.
    while (NULL != (ent=readdir(pDir)))
    {
    //printf("reclen=%d    type=%d\t", ent->d_reclen, ent->d_type);
     if ((ent->d_reclen != 0 )&&(ent->d_type==8))
     {    
         AISDK_INFO(LX("readWakeupAudioDir").d("WAKEUP_AUDIO_LIST ",ent->d_name));
         wakeUpAudioList.push_back(ent->d_name);
     }
    }    
}

int UIManager::responseWakeUp(std::deque<std::string> wakeUpAudioList)
{  
    char operationText[512];
    int i ;
    if(0 == (int)(wakeUpAudioList.size())){
        AISDK_ERROR(LX("responseWakeUp").d("reason", "cfg/soundai/wakeup = NULL"));
        return 0;
    }
    else if(flag_Time_read_audioDir < (int)(wakeUpAudioList.size()) ){
       i =  flag_Time_read_audioDir;
       flag_Time_read_audioDir ++;
    }
    else{
       i = 0;     
       flag_Time_read_audioDir = 0;
    }
     sprintf(operationText, "aplay /cfg/sai_config/wakeup/%s ",  wakeUpAudioList.at(i).c_str() );    
     AISDK_INFO(LX("responseWakeUp").d("current operation text", operationText ));    
     system(operationText);
     return 1;
}   

void UIManager::printState() {
     if(flag_Time_read_audioDir == 0)
     {
     //while (!WAKEUP_AUDIO_LIST.empty()) WAKEUP_AUDIO_LIST.pop_back();
     WAKEUP_AUDIO_LIST.clear();
     readWakeupAudioDir(wakeUpAudioPath,WAKEUP_AUDIO_LIST);
     }
     
     memset(&m_mqSndInfo, 0x00, sizeof(m_mqSndInfo));
		switch(m_dialogState) {
			case DialogUXStateObserverInterface::DialogUXState::IDLE:
	            m_mqSndInfo.msg_info.sub_msg_info.sub_id = LED_MODE_TTS;
	            m_mqSndInfo.msg_info.sub_msg_info.status = 0;
	            creatMsg(m_mqSndInfo);
				AISDK_INFO(LX(IDLE_MESSAGE));
				break;
			case DialogUXStateObserverInterface::DialogUXState::LISTENING:
				AISDK_INFO(LX(LISTEN_MESSAGE));
	            responseWakeUp(WAKEUP_AUDIO_LIST);
	            m_mqSndInfo.msg_info.sub_msg_info.sub_id = LED_MODE_WAKEUP;
	            m_mqSndInfo.msg_info.sub_msg_info.status = 1;
	            creatMsg(m_mqSndInfo);
	            //+WAKE UP  ; LED  ;
				break;
			case DialogUXStateObserverInterface::DialogUXState::LISTEN_EXPECTING:
                m_mqSndInfo.msg_info.sub_msg_info.sub_id = LED_MODE_TTS;
	            m_mqSndInfo.msg_info.sub_msg_info.status = 1;
	            creatMsg(m_mqSndInfo);
				AISDK_INFO(LX(LISTEN_MESSAGE));
				system("aplay /cfg/sai_config/wakeup/ding.wav");
				break;	
			case DialogUXStateObserverInterface::DialogUXState::THINKING:
	            m_mqSndInfo.msg_info.sub_msg_info.sub_id = LED_MODE_WAKEUP;
	            m_mqSndInfo.msg_info.sub_msg_info.status = 0;
	            creatMsg(m_mqSndInfo);
				AISDK_INFO(LX(THINK_MESSAGE));
				break;
			case DialogUXStateObserverInterface::DialogUXState::SPEAKING:
	            m_mqSndInfo.msg_info.sub_msg_info.sub_id = LED_MODE_TTS;
	            m_mqSndInfo.msg_info.sub_msg_info.status = 1;
	            creatMsg(m_mqSndInfo);
				AISDK_INFO(LX(SPEAK_MESSAGE));
				break;
			case DialogUXStateObserverInterface::DialogUXState::FINISHED:
				break;
		}
    

}

} // namespace application
} // namespace aisdk
