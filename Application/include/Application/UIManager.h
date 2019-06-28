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

#ifndef __UI_MANAGER_H_
#define __UI_MANAGER_H_

#include <DMInterface/VolumeObserverInterface.h>
#include <Utils/DialogRelay/DialogUXStateObserverInterface.h>
#include <Utils/NetworkStateObserverInterface.h>
#include <Utils/Threading/Executor.h>
#include "Application/mq_api.h"


namespace aisdk {
namespace application {

/**
 * A user interface manager status class.
 */
class UIManager 
	: public utils::dialogRelay::DialogUXStateObserverInterface
	, public utils::NetworkStateObserverInterface
	, public dmInterface::VolumeObserverInterface {
	
public:
	/// A alias @c DialogUXStateObserverInterface
	using DialogUXStateObserverInterface = utils::dialogRelay::DialogUXStateObserverInterface;
    /**
     * Constructor.
     */
	UIManager();

    ///use for ipc communication(key, led, event and so on);
    // int creatMsg(int argc, char* argv[]);
   // int creatMsg(e_LED_MODE mode, int status);
    int creatMsg(MqSndInfo mqSndInfo);

    int reveiceMsg(MqRecvInfo &mqRecvinfo);
	/// @name DialogUXStateObserverInterface methods
	/// @{
	void onDialogUXStateChanged(DialogUXState newState) override;
	/// @}
	
	/// @name NetworkStateObserverInterface methods
	/// @{
	void onNetworkStatusChanged(const Status newState) override;
	/// }

	/// @name VolumeObserverInterface methods.
	/// @{
	bool onVolumeChange(Type volumeType = Type::NLP_VOLUME_SET, int volume = 50) override;
	/// }
	
	/// Prints the Error Message for Wrong Input.
	void printErrorScreen();

	/// Prints the help option.
	void printHelpScreen();
    
	/// Prints the mic off info.
	void microphoneOff();

	/// Prints the mic off info.
	void microphoneOn();

	/// Set volume.
	void adjustVolume(dmInterface::VolumeObserverInterface::Type volumeType, int volume);
	
    ///use for read wake up audio dir and push audio list to deque.
    void readWakeupAudioDir(char *path, std::deque<std::string> &wakeUpAudioList);

    ///response wake up and aplay audio.
    int responseWakeUp(std::deque<std::string> wakeUpAudioList);

private:
	
	/// Prints the current state of nlp 
	void printState();
	
	/// The current dialog UX state of the SDK
    DialogUXState m_dialogState;

	/// The current dialog network state of the SDK
    Status m_connectionState;

	/// An internal executor thread pool that performs execution task sequentially.
    utils::threading::Executor m_executor;
};

}  // namespace application
}  // namespace aisdk

#endif  // __UI_MANAGER_H_

