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

#ifndef __APPLICATION_AICLIENT_H_
#define __APPLICATION_AICLIENT_H_

#include <unordered_set>
#include <vector>

#include <Utils/DeviceInfo.h>
#include <Utils/MediaPlayer/MediaPlayerInterface.h>
#include <Utils/DialogRelay/DialogUXStateObserverInterface.h>
#include <Utils/DialogRelay/DialogUXStateRelay.h>
#include <Utils/NetworkStateObserverInterface.h>
/// Stream buffer .
#include <Utils/SharedBuffer/SharedBuffer.h>
#include <DMInterface/DomainSequencerInterface.h>
#include <DMInterface/PlaybackRouterInterface.h>
#include <DMInterface/AutomaticSpeechRecognizerUIDObserverInterface.h>

#include "AudioTrackManager/AudioTrackManager.h"

#include <ASR/GenericAutomaticSpeechRecognizer.h>
#include <ASR/ASRRefreshConfiguration.h>
#include "SpeechSynthesizer/SpeechSynthesizer.h"
#include "ResourcesPlayer/ResourcesPlayer.h"
#include "AlarmsPlayer/AlarmsPlayer.h"
#include "VolumeManager/VolumeManager.h"
#include "Bringup.h"


namespace aisdk {
namespace application {

/**
 * This class is used to instantiate each interactive component
 */
class AIClient:public utils::NetworkStateObserverInterface {
public:
	/**
	 * Creates and initializes a default AI client. To connect the client to soundai server, users should make a call to
	 * connect() after creation.
	 */
	static std::unique_ptr<AIClient> createNew(
		std::shared_ptr<utils::DeviceInfo> deviceInfo,
		std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> chatMediaPlayer,
        std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> resourceMediaPlayer,      
    	/// To-Do wx
    	/// ...
    	/// ...
		std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> streamMediaPlayer,
        std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> alarmMediaPlayer,
		std::unordered_set<std::shared_ptr<utils::dialogRelay::DialogUXStateObserverInterface>>
        	dialogStateObservers,
        std::unordered_set<std::shared_ptr<dmInterface::AutomaticSpeechRecognizerUIDObserverInterface>>
			asrRefreshUid);

	bool notifyOfWakeWord(
		std::shared_ptr<utils::sharedbuffer::SharedBuffer> stream,
		utils::sharedbuffer::SharedBuffer::Index beginIndex,
		utils::sharedbuffer::SharedBuffer::Index endIndex);
    /**     
     * Adds an observer to be notified of NLP dialog related UX state.
     *
     * @param observer The observer to add.
     */
     void addDialogStateObserver(
     	std::shared_ptr<utils::dialogRelay::DialogUXStateObserverInterface> observer);
	
	/**
	 * Removes an observer to be notified of NLP dialog related UX state.
	 *
	 * @param observer The observer to remove.
	 */
	 void removeDialogStateObserver(
	 	std::shared_ptr<utils::dialogRelay::DialogUXStateObserverInterface> observer);

	/**
	 * Start service to allow the client to initiate connect sai sdk service. 
	 */
	void connect();

	/**
	 * Stop service to allow the client to initiate disconnect sai sdk.
	 */
	void disconnect();
	
	std::shared_ptr<domain::volumeManager::VolumeManager>& getVolumeManager() {
		return m_volumeManager;
	}
	/**
	 * This method can be called by the client when a Button is pressed on a physical button or on the GUI.
	 */
	void buttonPressed();

	void bringupSound(utils::bringup::eventType type, std::string ttsTxt);

    /// @name NetworkStateObserverInterface methods
	/// @{
	void onNetworkStatusChanged(const Status newState) override;
	/// }

    std::shared_ptr<domain::alarmsPlayer::AlarmsPlayer>& getAlarmPlayer();

	/// Get bringup instance.
	std::shared_ptr<modules::bringup::Bringup>& getBringUpPlayer();
	
    /**
     * Destructor.
     */
    ~AIClient();

private:
    /**
     * Initializes the SDK and "glues" all the components together.
     */
	bool initialize(
		std::shared_ptr<utils::DeviceInfo> deviceInfo,
		std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> chatMediaPlayer,
		std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> resourceMediaPlayer,	
    	/// To-Do wx
    	/// ...
    	/// ...
		std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> streamMediaPlayer,
		std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> alarmMediaPlayer,
		std::unordered_set<std::shared_ptr<utils::dialogRelay::DialogUXStateObserverInterface>>
			dialogStateObservers,
		std::unordered_set<std::shared_ptr<dmInterface::AutomaticSpeechRecognizerUIDObserverInterface>>
			asrRefreshUid);

	/// The AI dialog UX State.
	std::shared_ptr<utils::dialogRelay::DialogUXStateRelay> m_dialogUXStateRelay;

	/// The UID refresh .
	std::shared_ptr<asr::ASRRefreshConfiguration> m_asrRefreshConfig;
	
	/// The domain directive sequence.
	std::shared_ptr<dmInterface::DomainSequencerInterface> m_domainSequencer;

	/// The SoundAi client engine.
	std::shared_ptr<asr::GenericAutomaticSpeechRecognizer> m_asrEngine;

	/// The AudioTrack manager for audio channel player.
	std::shared_ptr<atm::AudioTrackManager> m_audioTrackManager;

	/// A unsortedset contain mediaplayer keyed by @c Type.
	std::unordered_set<std::shared_ptr<dmInterface::PlaybackRouterInterface>> m_playbackRouter;
	
	/// The speech synthesizer.
	std::shared_ptr<domain::speechSynthesizer::SpeechSynthesizer> m_speechSynthesizer;

    /// The resources player.
	std::shared_ptr<domain::resourcesPlayer::ResourcesPlayer> m_resourcesPlayer;

    /// To-Do wx
    std::shared_ptr<domain::alarmsPlayer::AlarmsPlayer> m_alarmsPlayer;

    std::shared_ptr<modules::bringup::Bringup> m_bringupPlayer;
	
	/// The volume manager.
	std::shared_ptr<domain::volumeManager::VolumeManager> m_volumeManager;
	/// ...
	/// ...
};

}  // namespace application
}  // namespace aisdk

#endif  // __APPLICATION_AICLIENT_H_

