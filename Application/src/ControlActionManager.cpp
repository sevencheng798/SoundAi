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

// String to identify log entries originating from this file.
static const std::string TAG("ControlActionManager");
/// Define output
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)



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
            m_isMicOn = false;
            m_userInterface->microphoneOff();
            m_micWrapper->stopStreamingMicrophoneData();
            
        } else {
            m_isMicOn = true;
            m_micWrapper->startStreamingMicrophoneData();
            m_userInterface->microphoneOn();
            
        }
    });	
}

void ControlActionManager::playbackControl() {
    m_executor.submit([this]() { m_client->buttonPressed(); });
}

void ControlActionManager::playBringupSound(int type) {
    AISDK_INFO(LX("ControlActionManager type")
                .d("value", type));
    m_type=type;

    m_executor.submit([this]() { m_client->bringupSound(m_type);});
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
