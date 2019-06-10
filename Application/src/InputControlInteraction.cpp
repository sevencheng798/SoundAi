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

#include "Application/InputControlInteraction.h"

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

int InputControlInteraction::run() {
	// Display begin message.
	m_controlActionManager->begin();

	// TODO: Should to achieve listen event with block way in here.
	// An IPC should be implemented in the following code. The IPC should to listen button event.
	char ch;
	while(true) {
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
	}
}

}  // namespace application
}  // namespace aisdk