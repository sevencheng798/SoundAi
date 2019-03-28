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

static const std::string TAG{"UIManager"};
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

namespace aisdk {
namespace application {

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

void UIManager::printState() {
	switch(m_dialogState) {
		case DialogUXStateObserverInterface::DialogUXState::IDLE:
			AISDK_INFO(LX("NLP Client is currently IDLE!"));
			break;
		case DialogUXStateObserverInterface::DialogUXState::LISTENING:
			AISDK_INFO(LX("Listening ..."));
            AISDK_INFO(LX("response wake up and aplay wakeup_9.wav "));
            system("aplay /cfg/sai_config/wakeup/wakeup_9.wav");
            //+WAKE UP  ; LED  ;
            
			break;
		case DialogUXStateObserverInterface::DialogUXState::THINKING:
			AISDK_INFO(LX("Thinking ..."));
			break;
		case DialogUXStateObserverInterface::DialogUXState::SPEAKING:
			AISDK_INFO(LX("SPEAKING ..."));
			break;
		case DialogUXStateObserverInterface::DialogUXState::FINISHED:
			break;
	}
}

} // namespace application
} // namespace aisdk
