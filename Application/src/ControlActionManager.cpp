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
#include "Application/UIManager.h"
#include "Application/ControlActionManager.h"

//use for send msg to ipc.
struct MqSndInfo m_mqSndMuteInfo;

namespace aisdk {
namespace application {

ControlActionManager::ControlActionManager(
    std::shared_ptr<AIClient> client,
    std::shared_ptr<utils::microphone::MicrophoneInterface> micWrapper,
    std::shared_ptr<UIManager> userInterface):
    SafeShutdown{"ControlActionManager"},
    m_client{client},
    m_micWrapper{micWrapper},
    m_userInterface{userInterface},
    m_isMicOn{true} {
	m_micWrapper->startStreamingMicrophoneData();
}

void ControlActionManager::begin() {
    m_executor.submit([this]() {
        m_userInterface->printHelpScreen();
    });
}

void ControlActionManager::help() {
    m_executor.submit([this]() {
        m_userInterface->printHelpScreen();
    });
}

void ControlActionManager::microphoneToggle() {
    m_executor.submit([this]() {
        if (m_isMicOn) {
            system("cvlc /cfg/sai_config/mic_close.mp3 --play-and-exit &");
            std::this_thread::sleep_for( std::chrono::seconds(2));
                     
            m_isMicOn = false;
            m_micWrapper->stopStreamingMicrophoneData();
            m_userInterface->microphoneOff();
   
            m_mqSndMuteInfo.msg_info.sub_msg_info.sub_id = LED_MODE_MUTE;
            m_mqSndMuteInfo.msg_info.sub_msg_info.status = 1;
            m_userInterface->creatMsg(m_mqSndMuteInfo);
                   } else {
            m_isMicOn = true;
            m_micWrapper->startStreamingMicrophoneData();
            m_userInterface->microphoneOn();
            
            system("cvlc /cfg/sai_config/mic_open.mp3 --play-and-exit &");
            m_mqSndMuteInfo.msg_info.sub_msg_info.sub_id = LED_MODE_MUTE;
            m_mqSndMuteInfo.msg_info.sub_msg_info.status = 0;
            m_userInterface->creatMsg(m_mqSndMuteInfo);
          }
    });	
}

void ControlActionManager::playbackControl() {
    m_executor.submit([this]() { m_client->buttonPressed(); });
}

void ControlActionManager::tap() {

}

void ControlActionManager::errorValue() {
    m_executor.submit([this]() { m_userInterface->printErrorScreen(); });
}

void ControlActionManager::doShutdown() {
	m_client.reset();
}

}  // namespace application
}  // namespace aisdk
