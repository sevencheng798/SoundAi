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
#include "properties.h"

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

//use for store wake up audio resources from @wakeUpAudioPath.    
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
        if (NULL != tmp) {
            memset(tmp, 0x00, sizeof(char)*255);
        }
        
        snd_info.msg_info.msg_type = GM_MSG_SAMPLE;
        snd_info.msg_info.sub_msg_info.sub_id = mqSndInfo.msg_info.sub_msg_info.sub_id;
        snd_info.msg_info.sub_msg_info.status = mqSndInfo.msg_info.sub_msg_info.status;
        memcpy(snd_info.msg_info.sub_msg_info.content, tmp, sizeof(char)* 255);
        memcpy(snd_info.msg_info.sub_msg_info.content,
               mqSndInfo.msg_info.sub_msg_info.content, 
               sizeof(mqSndInfo.msg_info.sub_msg_info.content));
        snd_info.msg_info.sub_msg_info.content_len = sizeof(char) * 255;
        snd_info.msg_info.sub_msg_info.iparam = 0xffffffff;
        snd_info.mq_flag = IPC_NOWAIT;

        AISDK_INFO(LX("IPC::creatMsg")
            .d("mode ", mqSndInfo.msg_info.sub_msg_info.sub_id)
            .d("status ", mqSndInfo.msg_info.sub_msg_info.status)
            .d("content", snd_info.msg_info.sub_msg_info.content));
        ret = mq_send(&snd_info);
        
        if (0 > ret) {
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
			m_connectionState = newState;
	        if(m_connectionState == utils::NetworkStateObserverInterface::Status::DISCONNECTED) {
		        AISDK_INFO(LX(CONNECTION_MESSAGE));
	        }else if(m_connectionState == utils::NetworkStateObserverInterface::Status::CONNECTED) {
			
			}
		});
}

bool UIManager::onVolumeChange(dmInterface::VolumeObserverInterface::Type volumeType, int volume) {
	AISDK_DEBUG5(LX("onVolumeChange").d("Type", volumeType).d("volume", volume));
	
	m_executor.submit([this, volumeType, volume]() { adjustVolume(volumeType, volume); });

	return true;
}

void UIManager::asrRefreshConfiguration(
		const std::string &uid,
		const std::string &appid, 
		const std::string &key,
		const std::string &deviceId) {
	AISDK_DEBUG5(LX("asrRefreshConfiguration").d("uid", uid));
	if(uid.empty()) {
		AISDK_ERROR(LX("asrRefreshConfigurationFailed").d("reason", "uidIsEmpty"));
	}else{
		//setprop((char *)"xf.aiui.uid", (char *)uid.c_str());
        memset(&m_mqSndInfo, 0x00, sizeof(m_mqSndInfo));   
        m_mqSndInfo.msg_info.sub_msg_info.sub_id = MQ_EVT_SET_PROP;
        memcpy(m_mqSndInfo.msg_info.sub_msg_info.content, uid.c_str(), uid.length());
        creatMsg(m_mqSndInfo);
	}
}

void UIManager::printErrorScreen() {
    m_executor.submit([]() { AISDK_INFO(LX("Invalid Option")); });
}

void UIManager::printHelpScreen() {
    m_executor.submit([]() { AISDK_INFO(LX(HELP_MESSAGE)); });
}

void UIManager::microphoneOff() {
    memset(&m_mqSndInfo, 0x00, sizeof(m_mqSndInfo));   
    m_mqSndInfo.msg_info.sub_msg_info.sub_id = LED_MODE_MUTE;
    m_mqSndInfo.msg_info.sub_msg_info.status = 1;
    creatMsg(m_mqSndInfo);

    system("echo 0 > /sys/devices/platform/dummy/mute");
    m_executor.submit([]() { AISDK_INFO(LX("Microphone Off!")); });
}

void UIManager::microphoneOn() {
    memset(&m_mqSndInfo, 0x00, sizeof(m_mqSndInfo));   
    m_mqSndInfo.msg_info.sub_msg_info.sub_id = LED_MODE_MUTE;
    m_mqSndInfo.msg_info.sub_msg_info.status = 0;
    creatMsg(m_mqSndInfo);

    m_executor.submit([this]() { printState(); });
}

void UIManager::adjustVolume(dmInterface::VolumeObserverInterface::Type volumeType, int volume) {
	AISDK_INFO(LX("adjustVolume").d("volumeType", volumeType));
	memset(&m_mqSndInfo, 0x00, sizeof(m_mqSndInfo));   
    switch (volumeType) {
        case dmInterface::VolumeObserverInterface::Type::NLP_VOLUME_UP:
            m_mqSndInfo.msg_info.sub_msg_info.sub_id = MQ_EVT_VOL_UP;
        break;
        case dmInterface::VolumeObserverInterface::Type::NLP_VOLUME_DOWN:
            m_mqSndInfo.msg_info.sub_msg_info.sub_id = MQ_EVT_VOL_DOWN;
        break;
        case dmInterface::VolumeObserverInterface::Type::NLP_VOLUME_SET:
            m_mqSndInfo.msg_info.sub_msg_info.sub_id = MQ_EVT_VOL_VALUE;
            m_mqSndInfo.msg_info.sub_msg_info.status = volume;
        break;
        default:
        break; 
    }
    creatMsg(m_mqSndInfo);
}

void UIManager::readWakeupAudioDir(char *path, std::deque<std::string> &wakeUpAudioList) {
    AISDK_INFO(LX("readWakeupAudioDir").d("Wake Up Audio Dir Path ",path));
    struct dirent* ent = NULL;
    DIR *pDir;
    pDir=opendir(path);
    while (NULL != (ent=readdir(pDir)))
    {
     if ((ent->d_reclen != 0 )&&(ent->d_type==8))
     {    
         AISDK_DEBUG(LX("readWakeupAudioDir").d("WAKEUP_AUDIO_LIST ",ent->d_name));
         wakeUpAudioList.push_back(ent->d_name);
     }
    }    
}

int UIManager::responseWakeUp(std::deque<std::string> wakeUpAudioList) {  
    char operationText[512];
    int i ;
    if(0 == (int)(wakeUpAudioList.size())){
        AISDK_ERROR(LX("responseWakeUp").d("reason", "cfg/soundai/wakeup = NULL"));
        return 0;
    }else if(flag_Time_read_audioDir < (int)(wakeUpAudioList.size()) ){
       i =  flag_Time_read_audioDir;
       flag_Time_read_audioDir ++;
    }else{
       i = 0;     
       flag_Time_read_audioDir = 0;
    }
     sprintf(operationText, "aplay /cfg/sai_config/wakeup/%s ",  wakeUpAudioList.at(i).c_str() );    
     AISDK_DEBUG(LX("responseWakeUp").d("current operation text", operationText ));    
     system(operationText);
     return 1;
}   

void UIManager::printState() {
     if(flag_Time_read_audioDir == 0)
     {
        WAKEUP_AUDIO_LIST.clear();
        readWakeupAudioDir(wakeUpAudioPath, WAKEUP_AUDIO_LIST);
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
             m_mqSndInfo.msg_info.sub_msg_info.sub_id = LED_MODE_WAKEUP;
             m_mqSndInfo.msg_info.sub_msg_info.status = 1;
             creatMsg(m_mqSndInfo);
             responseWakeUp(WAKEUP_AUDIO_LIST);
             break;
     	case DialogUXStateObserverInterface::DialogUXState::LISTEN_EXPECTING:
             m_mqSndInfo.msg_info.sub_msg_info.sub_id = LED_MODE_WAKEUP;
             m_mqSndInfo.msg_info.sub_msg_info.status = 1;
             creatMsg(m_mqSndInfo);
             AISDK_INFO(LX(LISTEN_MESSAGE));
             system("aplay /cfg/sai_config/ding.wav");
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
