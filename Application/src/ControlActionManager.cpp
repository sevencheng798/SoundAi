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
#include <Utils/Logging/Logger.h>
#include "string.h"

// String to identify log entries originating from this file.
static const std::string TAG("ControlActionManager");
/// Define output
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)



namespace aisdk {
namespace application {

ControlActionManager::ControlActionManager(
    std::shared_ptr<AIClient> client,
    std::shared_ptr<utils::microphone::MicrophoneInterface> micWrapper,
    std::shared_ptr<UIManager> userInterface,
    std::shared_ptr<utils::sharedbuffer::SharedBuffer> stream):
    SafeShutdown{"ControlActionManager"},
    m_client{client},
    m_micWrapper{micWrapper},
    m_userInterface{userInterface},
    m_stream{stream},
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
            m_isMicOn = false;
            m_micWrapper->stopStreamingMicrophoneData();
            m_userInterface->microphoneOff();
            playBringupSound(utils::bringup::eventType::MICROPHONE_OFF, (char *)"");

        } else {
            m_isMicOn = true;
            m_micWrapper->startStreamingMicrophoneData();
            m_userInterface->microphoneOn();
            playBringupSound(utils::bringup::eventType::MICROPHONE_ON, (char *)"");
            
        }
    });	
}

void ControlActionManager::playbackControl() {
    m_executor.submit([this]() { m_client->buttonPressed(); });
}

void ControlActionManager::playBringupSound(utils::bringup::eventType type, std::string ttsTxt) {
    AISDK_INFO(LX("playBringupSound").d("ttsTxt", ttsTxt));

    m_executor.submit([this, type, ttsTxt]() { m_client->bringupSound(type, ttsTxt);});
}

void ControlActionManager::tap() {
#ifdef PUSH_TAP
	m_executor.submit([this]() { m_client->notifyOfWakeWord(m_stream, 0, 0); });
#endif
}

void ControlActionManager::errorValue() {
    m_executor.submit([this]() { m_userInterface->printErrorScreen(); });
}

void ControlActionManager::onDialogUXStateChanged(DialogUXState newState) {
	if(newState == DialogUXState::FINISHED)
		 playBringupSound(utils::bringup::eventType::BRINGUP_DEFAULT, "");
}

void ControlActionManager::onAlarmAckStatusChanged(const Status newState, std::string ttsTxt) {
   if(newState == dmInterface::AlarmAckObserverInterface::Status::PLAYING){
        playBringupSound(utils::bringup::eventType::ALARM_ACK, ttsTxt);
    }else if(newState == dmInterface::AlarmAckObserverInterface::Status::WAITING){

    }
    
}

void ControlActionManager::doShutdown() {
	m_client.reset();
}

}  // namespace application
}  // namespace aisdk
