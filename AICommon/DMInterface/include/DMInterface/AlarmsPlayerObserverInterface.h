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

#ifndef __ALARMSPLAYER_OBSERVER_INTERFACE_H_
#define __ALARMSPLAYER_OBSERVER_INTERFACE_H_
#include <iostream>

namespace aisdk {
namespace dmInterface {

/**
 * Interface for observing a AlarmsPlayer.
 */
class AlarmsPlayerObserverInterface {
public:
    /**
     * This is an enum class used to indicate the state of the @c AlarmsPlayer.
     */
    enum class AlarmsPlayerState {
        /// In this state, the @c AlarmsPlayer is playing back the speech.
        PLAYING,

        /// In this state, the @c AlarmsPlayer is idle and not playing speech.
        FINISHED,

        /// In this state, the @c AlarmsPlayer is gaining the channel trace while still not playing anything
        GAINING_FOCUS,
       // IDLE,

        /// In this state, the @c AlarmsPlayer is losing the channel trace but not yet considered @c FINISHED
        LOSING_FOCUS
        //PAUSE
    };

    /**
     * Destructor.
     */
    virtual ~AlarmsPlayerObserverInterface() = default;

    /**
     * Notification that the @c ResourcesPlayer state has changed. Callback functions must return as soon as possible.
     * @param state The new state of the @c ResourcesPlayer.
     */
    virtual void onStateChanged(AlarmsPlayerState state) = 0;
	
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
    const AlarmsPlayerObserverInterface::AlarmsPlayerState state) {
    switch (state) {
        case AlarmsPlayerObserverInterface::AlarmsPlayerState::PLAYING:
            stream << "PLAYING";
            break;
        case AlarmsPlayerObserverInterface::AlarmsPlayerState::FINISHED:
            stream << "FINISHED";
            break;
        case AlarmsPlayerObserverInterface::AlarmsPlayerState::GAINING_FOCUS:
            stream << "GAINING_FOCUS";
            break;
        case AlarmsPlayerObserverInterface::AlarmsPlayerState::LOSING_FOCUS:
            stream << "LOSING_FOCUS";
            break;
    }
    return stream;
}

}	//namespace dmInterface
}	//namespace aisdk

#endif 	//__ALARMSPLAYER_OBSERVER_INTERFACE_H_