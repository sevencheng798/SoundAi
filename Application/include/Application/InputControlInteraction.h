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

#ifndef __INPUT_CONTROL_INTERACTION_H_
#define __INPUT_CONTROL_INTERACTION_H_

#include <memory>

#include "Application/ControlActionManager.h"

namespace aisdk {
namespace application {

/**
 * This class manages most of the user control interaction by taking in commands and notifying the AIClient and the
 * userInterface (the view) accordingly.
 */
class InputControlInteraction {
public:
    /**
     * Create constructor.
     */
    static std::unique_ptr<InputControlInteraction> create(
        std::shared_ptr<ControlActionManager> controlActionManager);

    /**
     * Begins the interaction between the Sample App and other processer. This should only be called at startup.
     */
     int run();
private:
	/**
     * Constructor.
     */
    InputControlInteraction(
        std::shared_ptr<ControlActionManager> controlActionManager);

    /// The control action interface manager.
    std::shared_ptr<ControlActionManager> m_controlActionManager;
};

}  // namespace application
}  // namespace aisdk

#endif  // __INPUT_CONTROL_INTERACTION_H_