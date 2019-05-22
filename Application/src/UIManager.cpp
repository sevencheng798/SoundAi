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
#include<dirent.h>

static const std::string TAG{"UIManager"};
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)
//audio dir path in devices;
char wakeUpAudioPath[] = "/cfg/sai_config/wakeup" ;
//to check the read audio dir time when starting up or wake up; 
int flag_Time_read_audioDir = 0; 

namespace aisdk {
namespace application {
    
std::deque<std::string> WAKEUP_AUDIO_LIST; 

static const std::string HELP_MESSAGE =
		"+----------------------------------------------------------------------------+\n"
		"|									Options:								  |\n"
		"| Tap to talk: 															  |\n"
		"|		 Press 't' and Enter followed by your query (no need for the kewword  |\n"
		"|			'xiaokangxiaokang').											  |\n"
		"| Stop an interaction: 													  |\n"
		"|		 Press 's' and Enter to stop an ongoing interaction.				  |\n";

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

void UIManager::onDialogUXStateChanged(DialogUXStateObserverInterface::DialogUXState newState) {
	m_executor.submit([this, newState]() {
			if(m_dialogState == newState)
				return;

			m_dialogState = newState;
			printState();
		});
}

void UIManager::printErrorScreen() {
    m_executor.submit([]() { AISDK_INFO(LX("Invalid Option")); });
}

void UIManager::printHelpScreen() {
    m_executor.submit([]() { AISDK_INFO(LX(HELP_MESSAGE)); });
}

void UIManager::microphoneOff() {
    m_executor.submit([]() { AISDK_INFO(LX("Microphone Off!")); });
}

void UIManager::microphoneOn() {
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
     if ((ent->d_reclen==24)&&(ent->d_type==8))
     {    
         AISDK_INFO(LX("readWakeupAudioDir").d("WAKEUP_AUDIO_LIST ",ent->d_name));
         wakeUpAudioList.push_back(ent->d_name);
     }
    }    
}

void UIManager::responseWakeUp(std::deque<std::string> wakeUpAudioList)
{  
    char operationText[512];
    int i ;
    if(flag_Time_read_audioDir < (int)(wakeUpAudioList.size()) ){
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
}   

void UIManager::printState() {
     if(flag_Time_read_audioDir == 0)
     {
    //clear reque list data;
     while (!WAKEUP_AUDIO_LIST.empty()) WAKEUP_AUDIO_LIST.pop_back();
     readWakeupAudioDir(wakeUpAudioPath,WAKEUP_AUDIO_LIST);
     }

	switch(m_dialogState) {
		case DialogUXStateObserverInterface::DialogUXState::IDLE:
			AISDK_INFO(LX(IDLE_MESSAGE));
			break;
		case DialogUXStateObserverInterface::DialogUXState::LISTENING:
			AISDK_INFO(LX(LISTEN_MESSAGE));
            responseWakeUp(WAKEUP_AUDIO_LIST);
            //+WAKE UP  ; LED  ;
			break;
		case DialogUXStateObserverInterface::DialogUXState::THINKING:
			AISDK_INFO(LX(THINK_MESSAGE));
			break;
		case DialogUXStateObserverInterface::DialogUXState::SPEAKING:
			AISDK_INFO(LX(SPEAK_MESSAGE));
			break;
		case DialogUXStateObserverInterface::DialogUXState::FINISHED:
			break;
	}
}

} // namespace application
} // namespace aisdk
