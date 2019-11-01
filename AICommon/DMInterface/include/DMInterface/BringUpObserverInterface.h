/*
 * Copyright 2019 gm its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifndef __BRINGUP_OBSERVER_INTERFACE_H_
#define __BRINGUP_OBSERVER_INTERFACE_H_
#include <iostream>
#include <Utils/BringUp/BringUpEventType.h>

namespace aisdk {
namespace dmInterface {

/**
 * Interface for observing a BringUpPlayer.
 */
class BringUpObserverInterface {
public:
    /**
     * This is an enum class used to indicate the state of the @c BringUpPlayer.
     */
    enum class BringUpPlayerState {
        /// In this state, the @c BringUpPlayer is playing back the speech.
        PLAYING,

        /// In this state, the @c BringUpPlayer is idle and not playing speech.
        FINISHED,
    };

    /**
     * Destructor.
     */
    virtual ~BringUpObserverInterface() = default;

    /**
     * Notification that the @c BringUpPlayer state has changed. Callback functions must return as soon as possible.
     * @param state The new state of the @c BringUpPlayer.
     */
    virtual void onStateChanged(utils::bringup::eventType event, BringUpPlayerState state) = 0;
	
};

/**
 * Write a @c State value to an @c ostream as a string.
 *
 * @param stream The stream to write the value to.
 * @param state The state value to write to the @c ostream as a string.
 * @return The @c ostream that was passed in and written to.
 */
inline std::ostream& operator<<(
    std::ostream& stream,
    const BringUpObserverInterface::BringUpPlayerState state) {
    switch (state) {
        case BringUpObserverInterface::BringUpPlayerState::PLAYING:
            stream << "PLAYING";
            break;
        case BringUpObserverInterface::BringUpPlayerState::FINISHED:
            stream << "FINISHED";
            break;
    }
    return stream;
}

}	//namespace dmInterface
}	//namespace aisdk

#endif 	//__BRINGUP_OBSERVER_INTERFACE_H_