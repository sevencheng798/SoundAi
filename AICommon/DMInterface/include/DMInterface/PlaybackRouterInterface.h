/*
 * Copyright 2018 its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifndef __PLAYBACK_ROUTER_INTERFACE_H_
#define __PLAYBACK_ROUTER_INTERFACE_H_
//#include <string>
//#include <vector>
//#include <memory>

namespace aisdk {
namespace dmInterface {

/**
 * The @c PlaybackRouterInterface used to get a playback button press and
 * send it it to the current handler.
 */
class PlaybackRouterInterface {
public:
    /**
     * Destructor
     */
    virtual ~PlaybackRouterInterface() = default;

    /**
     * This method can be called by the client when a Button is pressed on a physical button or on the GUI.
     */
    virtual void buttonPressedPlayback() = 0;

	/**
     * This method can be called by the client when a Button is pressed on a physical button or on the GUI.
     */
	virtual void buttonPressedNext() {};
	/**
     * This method can be called by the client when a Button is pressed on a physical button or on the GUI.
     */
	virtual void buttonPressedPrevious() {};
};

}  // namespace dmInterface
} //namespace aisdk
#endif  // __PLAYBACK_ROUTER_INTERFACE_H_
