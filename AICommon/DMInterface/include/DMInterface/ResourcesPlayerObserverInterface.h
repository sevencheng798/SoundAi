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

#ifndef __RESOURCESPLAYER_OBSERVER_INTERFACE_H_
#define __RESOURCESPLAYER_OBSERVER_INTERFACE_H_
#include <iostream>

namespace aisdk {
namespace dmInterface {

/**
 * Interface for observing a ResourcesPlayer.
 */
class ResourcesPlayerObserverInterface {
public:
    /**
     * This is an enum class used to indicate the state of the @c ResourcesPlayer.
     */
    enum class ResourcesPlayerState {
        /// In this state, the @c ResourcesPlayer is playing back the speech.
        PLAYING,

        /// In this state, the @c ResourcesPlayer is idle and not playing speech.
        FINISHED,

        /// In this state, the @c ResourcesPlayer is gaining the channel trace while still not playing anything
        GAINING_FOCUS,

        /// In this state, the @c ResourcesPlayer is losing the channel trace but not yet considered @c FINISHED
        LOSING_FOCUS
    };

    /**
     * Destructor.
     */
    virtual ~ResourcesPlayerObserverInterface() = default;

    /**
     * Notification that the @c ResourcesPlayer state has changed. Callback functions must return as soon as possible.
     * @param state The new state of the @c ResourcesPlayer.
     */
    virtual void onStateChanged(ResourcesPlayerState state) = 0;
	
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
    const ResourcesPlayerObserverInterface::ResourcesPlayerState state) {
    switch (state) {
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::PLAYING:
            stream << "PLAYING";
            break;
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::FINISHED:
            stream << "FINISHED";
            break;
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::GAINING_FOCUS:
            stream << "GAINING_FOCUS";
            break;
        case ResourcesPlayerObserverInterface::ResourcesPlayerState::LOSING_FOCUS:
            stream << "LOSING_FOCUS";
            break;
    }
    return stream;
}

}	//namespace dmInterface
}	//namespace aisdk

#endif 	//__RESOURCESPLAYER_OBSERVER_INTERFACE_H_