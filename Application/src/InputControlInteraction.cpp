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

#include "Application/InputControlInteraction.h"

static const std::string TAG{"InputControlInteraction"};
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

//use for receive msg to ipc.
struct MqRecvInfo m_mqRecvInfo;

namespace aisdk {
namespace application {

/**
 * This class manages most of the user control interaction by taking in commands and notifying the AIClient and the
 * userInterface (the view) accordingly.
 */
std::unique_ptr<InputControlInteraction> InputControlInteraction::create(
        std::shared_ptr<ControlActionManager> controlActionManager) {
	if(!controlActionManager) {
		return nullptr;
	}

	return std::unique_ptr<InputControlInteraction>(new InputControlInteraction(controlActionManager));
}

InputControlInteraction::InputControlInteraction(
	std::shared_ptr<ControlActionManager> controlActionManager):m_controlActionManager{controlActionManager} {

}


int InputControlInteraction::reveiceMsg(MqRecvInfo &mqRecvinfo) {
    ssize_t ret = 0;
    memset(&mqRecvinfo, 0x00, sizeof(MQ_RECV_INFO_T));
    std::this_thread::sleep_for( std::chrono::microseconds(200));
   // std::this_thread::sleep_for( std::chrono::nanosecons(200));
    mqRecvinfo.mq_flag = IPC_NOWAIT;
    mqRecvinfo.msg_info.msg_type = GM_MSG_GM_TASK;
    ret = mq_recv(&mqRecvinfo);

    if (0 > ret)
    {
        return -1;
    }
    return 0;
}

int InputControlInteraction::run() {
	// Display begin message.
	m_controlActionManager->begin();

  
	// TODO: Should to achieve listen event with block way in here.
	// An IPC should be implemented in the following code. The IPC should to listen button event.
    // char ch;
	while(true) {
        
        memset(&m_mqRecvInfo, 0x00, sizeof(m_mqRecvInfo));
        int flag_recv = reveiceMsg(m_mqRecvInfo);
        int ipcCmdMode =  m_mqRecvInfo.msg_info.sub_msg_info.sub_id;
        int ipcStatus = m_mqRecvInfo.msg_info.sub_msg_info.status;

        if(flag_recv == 0 ){
            AISDK_INFO(LX("IPC::ReceiveMsg")
                .d("msg_type", GM_MSG_GM_TASK)
                .d("mode", m_mqRecvInfo.msg_info.sub_msg_info.sub_id)
                .d("status", m_mqRecvInfo.msg_info.sub_msg_info.status)
                .d("content_len", m_mqRecvInfo.msg_info.sub_msg_info.content_len)
                .d("iparam", m_mqRecvInfo.msg_info.sub_msg_info.iparam));             
        }
            switch (ipcCmdMode){
                case KEY_EVT_VOL_UP:
                      AISDK_INFO(LX("IPC::ReceiveMsg").d("ipcCmdMode","KEY_EVT_VOL_UP"));
                    break;
                case KEY_EVT_VOL_DOWN:
                      AISDK_INFO(LX("IPC::ReceiveMsg").d("ipcCmdMode","KEY_EVT_VOL_DOWN"));
                    break;
                case KEY_EVT_MUTE:
                    AISDK_INFO(LX("IPC::ReceiveMsg").d("ipcCmdMode","KEY_EVT_MUTE"));
                    m_controlActionManager->microphoneToggle();
                    break;
                case KEY_EVT_PAUSE_AND_RESUME:
                    AISDK_INFO(LX("IPC::ReceiveMsg").d("ipcCmdMode","KEY_EVT_PAUSE_AND_RESUME"));
                    m_controlActionManager->playbackControl();
                    break;
                case BRINGUP:
                    AISDK_INFO(LX("IPC::ReceiveMsg BRINGUP").d("ipcStatus", ipcStatus));
                    m_controlActionManager->playBringupSound(ipcStatus);
                    break;
                default:
                    break;
            }
            
#if 0       
		std::cin >> ch;        
		switch (ch) {
		case 'q':
		case 'Q':
			return 1;
		case 'i':
		case 'I':
			m_controlActionManager->help();
			break;
		case 'P':
		case 'p':
			m_controlActionManager->playbackControl();
			break;
		case 'm':
		case 'M':
            m_controlActionManager->microphoneToggle();
			break;            
		}
 #endif 
	}
}

}  // namespace application
}  // namespace aisdk
