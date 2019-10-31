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

#ifndef __APPLICATIONXX_CLIENT_H_
#define __APPLICATIONXX_CLIENT_H_


#include <Utils/MediaPlayer/MediaPlayerObserverInterface.h>
#include <Utils/Attachment/AttachmentManagerInterface.h>
#include <Utils/Channel/AudioTrackManagerInterface.h>
#include <Utils/Channel/ChannelObserverInterface.h>
#include <Utils/Threading/Executor.h>
#include <ASR/GenericAutomaticSpeechRecognizer.h>
#include <Utils/BringUp/BringUpEventType.h>


namespace aisdk {
namespace modules {
namespace bringup {
/**
 * A @c RetryTimer holds information needed to compute a retry time for a thread
 * when waiting on events.
 */
class Bringup
    : public utils::mediaPlayer::MediaPlayerObserverInterface
    , public utils::channel::ChannelObserverInterface
    , public std::enable_shared_from_this<Bringup> {
public:
    /**
     * Constructor.
     *
     * @param retryTable The table with entries for retry times.
     */
     static std::shared_ptr<Bringup> create(
        std::shared_ptr<utils::attachment::AttachmentManagerInterface> attachmentDocker,
        std::shared_ptr<asr::GenericAutomaticSpeechRecognizer> asrEngine,
        std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> mediaPlayer,
        std::shared_ptr<utils::channel::AudioTrackManagerInterface> trackManager);

    /// @name ChannelObserverInterface method.
    /// @{
    void onTrackChanged(utils::channel::FocusState newTrace) override;
    /// @}
    
    /// @name MediaPlayerObserverInterface method.
    /// @{
    void onPlaybackStarted(SourceId id) override;
    
    void onPlaybackFinished(SourceId id) override;

    void executePlaybackFinished() ;    

    void onPlaybackError(SourceId id, const utils::mediaPlayer::ErrorType& type, std::string error) override;
    
    void onPlaybackStopped(SourceId id) override;
     /// @}

     void init();

    bool start(utils::bringup::eventType type,  std::string ttsTxt);
     
     ~Bringup();

private:
    Bringup(
        std::shared_ptr<utils::attachment::AttachmentManagerInterface> attachmentDocker,
        std::shared_ptr<asr::GenericAutomaticSpeechRecognizer> asrEngine,
        std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> mediaPlayer,
        std::shared_ptr<utils::channel::AudioTrackManagerInterface> trackManager);

 
    std::shared_ptr<utils::attachment::AttachmentManagerInterface> m_attachmentDocker;
    std::shared_ptr<asr::GenericAutomaticSpeechRecognizer> m_asrEngine;
    std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> m_bringupPlayer;
    std::shared_ptr<utils::channel::AudioTrackManagerInterface> m_trackManager;

    SourceId m_currentSourceId;
    utils::bringup::eventType m_status;
    int m_playFlag;
    utils::channel::FocusState m_Tracep;

    std::string m_ttsTxt;
    
    enum PLAY_FLAG{
        IDLE = 0,
        PLAY_START_FLAG,
        PLAY_FINISHED_FLAG,
        PLAY_ERROR_FLAG,
        PLAY_STOP_FLAG
    };

    ///
    const char* CreateRandomUuid(char *uuid);

    ///
    void inOpenFile(const char *filePath);

    ///add for play the ttstxt from alarmack and ipc;
    void playttsTxtItem(std::string ttsTxt);

    utils::threading::Executor  m_executor;
};

}
}  // namespace modules
}  // namespace aisdk

#endif  // __APPLICATION_CLIENT_H_

