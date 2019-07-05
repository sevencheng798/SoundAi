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

#ifndef __RESOURCES_PLAYER_H_
#define __RESOURCES_PLAYER_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <deque>

#include <Utils/Channel/ChannelObserverInterface.h>
#include <Utils/Channel/AudioTrackManagerInterface.h>
#include <Utils/DialogRelay/DialogUXStateObserverInterface.h>
#include <Utils/DialogRelay/DialogUXStateRelay.h>
#include <Utils/MediaPlayer/MediaPlayerInterface.h>
#include <Utils/MediaPlayer/MediaPlayerObserverInterface.h>
#include <Utils/SafeShutdown.h>
#include <DMInterface/PlaybackRouterInterface.h>
#include <DMInterface/ResourcesPlayerObserverInterface.h>
#include <NLP/DomainProxy.h>
#include <Utils/cJSON.h>
#include <Utils/DeviceInfo.h>
#include "http.h"

namespace aisdk {
namespace domain {
namespace resourcesPlayer {

/*
 * This class implements the ResourcesPlayer for real-time conversation domains.
 * The domain include (music/story/fm/so on)
 */
 //public nlp::DomainProxy,
class ResourcesPlayer
		: public nlp::DomainProxy
		, public utils::SafeShutdown
		, public utils::mediaPlayer::MediaPlayerObserverInterface
		, public dmInterface::PlaybackRouterInterface
		, public std::enable_shared_from_this<ResourcesPlayer> {
public:


    /**
     * Create a new @c ResourcesPlayer instance.
     *
     * @param mediaPlayer The instance of the @c MediaPlayerInterface used to play audio.
     * @param trackManager The instance of the @c AudioTrackManagerInterface used to acquire track of a channel.
     * @param dialogUXStateRelay The instance of the @c DialogUXStateRelay to use to notify user device interactive state.
     *
     * @return Returns a new @c ResourcesPlayer, or @c nullptr if the operation failed.
     */
    static std::shared_ptr<ResourcesPlayer> create(
        std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> mediaPlayer,
        std::shared_ptr<utils::channel::AudioTrackManagerInterface> trackManager,
        std::shared_ptr<utils::dialogRelay::DialogUXStateRelay> dialogUXStateRelay);

	/// @name DomainProxy method.
	/// @{
	/// DomainHandlerInterface we need to definition the base class pure-virturl functions.
	void onDeregistered() override;
	
	void preHandleDirective(std::shared_ptr<DirectiveInfo> info) override;

	void handleDirective(std::shared_ptr<DirectiveInfo> info) override;

	void cancelDirective(std::shared_ptr<DirectiveInfo> info) override;

	/// @}
	
	/// @name ChannelObserverInterface method.
	/// @{
	void onTrackChanged(utils::channel::FocusState newTrace) override;
	/// @}
	
	/// @name MediaPlayerObserverInterface method.
	/// @{
	void onPlaybackStarted(SourceId id) override;
	
	void onPlaybackFinished(SourceId id) override;
	
	void onPlaybackError(SourceId id, const utils::mediaPlayer::ErrorType& type, std::string error) override;

    void onPlaybackPaused(SourceId id) override;

    void onPlaybackResumed(SourceId id) override;
    
	void onPlaybackStopped(SourceId id) override;
	 /// @}
     


	/*
	 * Add an observer to ResourcesPlayer.
	 * 
	 * @param observer The observer to add.
	 */
	void addObserver(std::shared_ptr<dmInterface::ResourcesPlayerObserverInterface> observer);

	/// Remove an observer from the ResourcesPlayer.
	/// @param observer The ovserver to remove.
	void removeObserver(std::shared_ptr<dmInterface::ResourcesPlayerObserverInterface> observer);

	/// Get the name of the execution DomainHandler. 
	std::unordered_set<std::string> getHandlerName() const override;

    /// @name PlaybackRouterInterface method.
	/// @{
    /// Stop playing speech audio.
    void buttonPressedPlayback() override;

    ///get kugou param;
    void setKuGouParam(const char *aiuiUid, const char *appId, const char *appKey, const char *clientDeviceId);
protected:

	/// @name SafeShutdown method.
	/// @{
	void doShutdown() override;
	/// @}
	
private:

    /**
     * This class has all the data that is needed to process @c Chat(url_tts) directives.
     */
    class ResourcesDirectiveInfo {
    public:
		
        /**
         * Constructor.
         *
         * @param directiveInfo The @c DirectiveInfo.
         */
		ResourcesDirectiveInfo(std::shared_ptr<DirectiveInfo> directiveInfo);

		/// Release Chat specific resources.
        void clear();

        /// The Domain directive @c NLPDomain that is passed during preHandle.
        std::shared_ptr<nlp::NLPDomain> directive;

        /// @c DomainHandlerResultInterface.
        std::shared_ptr<dmInterface::DomainHandlerResultInterface> result;
		
		/// Identifies the location of audio content.  the value will be a remote http/https location.
		std::string url;

        /// A flag to indicate if the domain directive complete message has to be sent to the @c DomainSequencer.
        bool sendCompletedMessage;
    };

	/**
	 * Constructor
	 *
	 * @param mediaPlayer The instance of the @c MediaPlayerInterface used to play audio.
	 * @param trackManager The instance of the @c FocusManagerInterface used to acquire focus of a channel.
	 */
	ResourcesPlayer(
		std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> mediaPlayer,
		std::shared_ptr<utils::channel::AudioTrackManagerInterface> trackManager);

    /**
     * Initializes the @c ResourcesPlayer.
     * Adds the @c ResourcesPlayer as an observer of the speech player.
     */
    void init();


    ///
    void AnalysisNlpDataForResourcesPlayer(cJSON          * datain , std::deque<std::string> &audiourllist );

    ///
    void AnalysisAudioIdForResourcesPlayer(std::shared_ptr<DirectiveInfo> info , std::deque<std::string> &audioidlist );
    
    ///
    void AnalysisNlpDataForPlayControl(std::shared_ptr<DirectiveInfo> info, std::string &operation );

    ///
    void AnalysisNlpDataForVolume(std::shared_ptr<DirectiveInfo> info, std::string &operation, int &volumeValue );
    
    ///
    void responsePlayControl(std::shared_ptr<DirectiveInfo> info, std::string operation);

    
    /**
     * Pre-handle a ResourcesPlayer.Chat directive (on the @c m_executor threadpool) to parse own keys and values.
     *
     * @param info The directive to pre-handle and the associated data.
     */
    void executePreHandleAfterValidation(std::shared_ptr<DirectiveInfo> info);

    /**
     * Handle a ResourcesPlayer.Chat directive (on the @c m_executor threadpool).  This starts a request for
     * the foreground trace focus.
     *
     * @param info The directive to handle and the associated data.
     */
    void executeHandleAfterValidation(std::shared_ptr<ResourcesDirectiveInfo> info);

    /**
     * Pre-handle a ResourcesPlayer.Chat directive (on the @c m_executor thread). This starts a request parse.
     *
     * @param info The directive to preHandle
     */
    void executePreHandle(std::shared_ptr<DirectiveInfo> info);

    /**
     * Handle a ResourcesPlayer.Chat directive (on the @c m_executor threadpool).  This starts a request for
     * the foreground trace focus.
     *
     * @param info The directive to handle and the result object with which to communicate the result.
     */
    void executeHandle(std::shared_ptr<DirectiveInfo> info);

    void handlePlayDirective(std::shared_ptr<DirectiveInfo> info);

    ///
    void executePlay(std::shared_ptr<DirectiveInfo> info) ;

    /**
     * Cancel execution of a ResourcesPlayer.Chat directive (on the @c m_executor threadpool).
     *
     * @param info The directive to cancel.
     */
    void executeCancel(std::shared_ptr<DirectiveInfo> info);

    /**
     * Execute a change of state (on the @c m_executor threadpool). If the @c m_desiredState is @c PLAYING, playing the
     * audio of the current directive is started. If the @c m_desiredState is @c FINISHED this method triggers
     * termination of playing the audio.
     */
    void executeStateChange();


    ///
    void executeTrackChanged(utils::channel::FocusState newTrace);
    /**
     * Handle (on the @c m_executor threadpool) notification that speech playback has started.
     */
    void executePlaybackStarted();


    void executePlaybackStopped();
    /**
     * Handle (on the @c m_executor threadpool) notification that speech playback has finished.
     */
    void executePlaybackFinished();


    void executePlaybackPaused();


    void executePlaybackResumed();


    /**
     * Handle (on the @c m_executor threadpool) notification that speech playback encountered an error.
     *
     * @param error Text describing the nature of the error.
     */
    ///
    void playNextItem();

    void playResourceItem(std::string ResourceItem );

    void playKuGouResourceItemID(std::string ResourceItemID );
    
    void executePlaybackError(const utils::mediaPlayer::ErrorType& type, std::string error);

    /**
     * Start playing speech audio.
     */
    void startPlaying();

    /**
     * Stop playing speech audio.
     */
    void stopPlaying();

	/**
     * Set the current state of the @c ResourcesPlayer.
     * @c m_mutex must be acquired before calling this function. All observers will be notified of a new state
     *
     * @param newState The new state of the @c ResourcesPlayer.
     */
    void setCurrentStateLocked(
    	dmInterface::ResourcesPlayerObserverInterface::ResourcesPlayerState newState);
		
    /**
     * Set the desired state the @c ResourcesPlayer needs to transition to based on the @c newTrace.
     * @c m_mutex must be acquired before calling this function.
     *
     * @param newTrace The new track focus of the @c ResourcesPlayer.
     */
    void setDesiredStateLocked(utils::channel::FocusState newTrace);

	/**
     * Reset @c m_currentInfo, cleaning up any @c ResourcesPlayer resources and removing from DomainProxy's
     * map of active @c NLPDomain.
     *
     * @param info The @c DirectiveInfo instance to make current (defaults to @c nullptr).
     */
	void resetCurrentInfo(std::shared_ptr<ResourcesDirectiveInfo> info = nullptr);

    /**
     * Send the handling completed notification to @c domainRouter and clean up the resources of @c m_currentInfo.
     */
	void setHandlingCompleted();

    /**
     * Report a failure to handle the @c NLPDomain.
     *
     * @param info The @c NLPDomain that encountered the error and ancillary information.
     * @param message The error message to include in the ExceptionEncountered message.
     */
	void reportExceptionFailed(
		std::shared_ptr<ResourcesDirectiveInfo> info,
        const std::string& message);

    /**
     * Release the @c FOREGROUND trace focus (if we have it).
     */
    void releaseForegroundTrace();

    /**
     * Reset the @c m_mediaSourceId once the @c MediaPlayer finishes with playback or
     * when @c MediaPlayer returns an error.
     */
    void resetMediaSourceId();

    /**
     * Verify a pointer to a well formed @c ResourcesDirectiveInfo.
     *
     * @param caller Name of the method making the call, for logging.
     * @param info The @c DirectiveInfo to test.
     * @param checkResult Check if the @c DomainHandlerResultInterface is not a @c nullptr in the @c DirectiveInfo.
     * @return A @c ResourcesDirectiveInfo if it is well formed, otherwise @c nullptr.
     */
    std::shared_ptr<ResourcesDirectiveInfo> validateInfo(
        const std::string& caller,
        std::shared_ptr<DirectiveInfo> info,
        bool checkResult = true);

    /**
     * Find the @c ResourcesDirectiveInfo instance (if any) for the specified @c messageId.
     *
     * @param messageId The messageId value to find @c ResourcesDirectiveInfo instance.
     * @return The @c ResourcesDirectiveInfo instance for @c messageId.
     */
    std::shared_ptr<ResourcesDirectiveInfo> getResourcesDirectiveInfo(const std::string& messageId);

    /**
     * Checks if the @c messageId is already present in the map. If its not present, adds an entry to the map.
     *
     * @param messageId The @c messageId value to add to the @c m_chatDirectiveInfoMap
     * @param info The @c ResourcesDirectiveInfo corresponding to the @c messageId to add.
     * @return @c true if @c messageId to @c ResourcesDirectiveInfo mapping was added. @c false if entry already exists
     * for the @c messageId.
     */
    bool setResourcesDirectiveInfo(
        const std::string& messageId,
        std::shared_ptr<ResourcesPlayer::ResourcesDirectiveInfo> info);

    /**
     * Adds the @c ResourcesDirectiveInfo to the @c m_chatInfoQueue.
     *
     * @param info The @c ResourcesDirectiveInfo to add to the @c m_chatInfoQueue.
     */
    void addToDirectiveQueue(std::shared_ptr<ResourcesDirectiveInfo> info);

    /**
     * Removes the @c ResourcesDirectiveInfo corresponding to the @c messageId from the @c m_chatDirectiveInfoMap.
     *
     * @param messageId The @c messageId to remove.
     */
    void removeChatDirectiveInfo(const std::string& messageId);	

    /**
     * This function is called whenever the NLP UX dialog state of the system changes.
     *
     * @param newState The new dialog specific NLP UX state.
     */
    void executeOnDialogUXStateChanged(
        utils::dialogRelay::DialogUXStateObserverInterface::DialogUXState newState);

	/// The name of DomainHandler identifies which @c DomainHandlerInterface operates on.
	std::unordered_set<std::string> m_handlerName;
	
	/// MediaPlayerInterface instance to send tts audio to MediaPlayer interface and playback.
	std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> m_resourcesPlayer;

	/// AudioTrackManagerInterface instance to acqurie the channel.
	std::shared_ptr<utils::channel::AudioTrackManagerInterface> m_trackManager;

    /// Id to identify the specific source when making calls to MediaPlayerInterface.
    utils::mediaPlayer::MediaPlayerInterface::SourceId m_mediaSourceId;
	
    /// The set of @c SpeechSynthesizerObserverInterface instances to notify of state changes.
    std::unordered_set<std::shared_ptr<dmInterface::ResourcesPlayerObserverInterface>> m_observers;

    /**
     * The current state of the @c ResourcesPlayer. @c m_mutex must be acquired before reading or writing the
     * @c m_currentState.
     */
    dmInterface::ResourcesPlayerObserverInterface::ResourcesPlayerState m_currentState;

    /**
     * The state the @c ResourcesPlayer must transition to. @c m_mutex must be acquired before reading or writing
     * the @c m_desiredState.
     */
    dmInterface::ResourcesPlayerObserverInterface::ResourcesPlayerState m_desiredState;

    /// The current trace acquired by the @c ResourcesPlayer.
    utils::channel::FocusState m_currentFocus;

	/// @c ResourcesDirectiveInfo instance for the @c AVSDirective currently being handled.
	std::shared_ptr<ResourcesDirectiveInfo> m_currentInfo;

	/// Mutex to serialize access to m_currentState, m_desiredState, and m_waitOnStateChange.
	std::mutex m_mutex;
	std::mutex m_mutexStopRequest;

	/// A flag to keep track of if @c ResourcesPlayer has called @c Stop() already or not.
	bool m_isAlreadyStopping;

	/// Condition variable to wake @c onFocusChanged() once the state transition to desired state is complete.
	std::condition_variable m_waitOnStateChange;

    std::condition_variable m_waitOnStopChange;
    
    /// Map of message Id to @c ResourcesDirectiveInfo.
    std::unordered_map<std::string, std::shared_ptr<ResourcesDirectiveInfo>> m_chatDirectiveInfoMap;

    /**
     * Mutex to protect @c messageId to @c ResourcesDirectiveInfo mapping. This mutex must be acquired before accessing
     * or modifying the m_chatDirectiveInfoMap
     */
    std::mutex m_chatDirectiveInfoMutex;

    /// Queue which holds the directives to be processed.
    std::deque<std::shared_ptr<ResourcesDirectiveInfo>> m_chatInfoQueue;

    /// Serializes access to @c m_chatInfoQueue
    std::mutex m_chatInfoQueueMutex;

    /// Current operation state to playcontrol.
   // State m_currentOperationState;
    
    /// The attachment reader @c AttachmentReader to read speech audio('text to speech').
    std::unique_ptr<utils::attachment::AttachmentReader> m_attachmentReader;

    ///use for store kugou param;
    std::string m_aiuiUid;
    std::string m_appId;
    std::string m_appKey;
    std::string m_clientDeviceId;
    
	/// An internal thread pool which queues up operations from asynchronous API calls
	utils::threading::Executor m_executor;

};

}	// namespace resourcesPlayer
} 	//namespace domain
}	//namespace aisdk

#endif 	//__RESOURCES_PLAYER_H_

