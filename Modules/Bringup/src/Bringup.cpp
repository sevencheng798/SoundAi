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
#include <fstream>
#include <Utils/Logging/Logger.h>
#include <Utils/BringUp/BringUpEventType.h>
#include "Bringup.h"

/// String to identify log entries originating from this file.
static const std::string TAG("Bringup");
static auto in = std::make_shared<std::ifstream>();

/// Define output
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

namespace aisdk {
namespace modules {
namespace bringup {
/// The name of the @c AudioTrackManager channel used by the @c SpeechSynthesizer.
static const std::string CHANNEL_NAME = utils::channel::AudioTrackManagerInterface::DIALOG_CHANNEL_NAME;

/// The name of the @c SafeShutdown
static const std::string BRINGUP_NAME{"Bringup"};

std::shared_ptr<Bringup> Bringup::create(
   std::shared_ptr<utils::attachment::AttachmentManagerInterface> attachmentDocker,
   std::shared_ptr<asr::GenericAutomaticSpeechRecognizer> asrEngine,
   std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> mediaPlayer,
   std::shared_ptr<utils::channel::AudioTrackManagerInterface> trackManager){
    auto bringup = std::shared_ptr<Bringup>(new Bringup(attachmentDocker, asrEngine, mediaPlayer, trackManager));
    if(bringup) {
        bringup->init();
        return bringup;
    }

    return nullptr;
}

Bringup::Bringup(
    std::shared_ptr<utils::attachment::AttachmentManagerInterface> attachmentDocker,
    std::shared_ptr<asr::GenericAutomaticSpeechRecognizer> asrEngine,
    std::shared_ptr<utils::mediaPlayer::MediaPlayerInterface> mediaPlayer,
    std::shared_ptr<utils::channel::AudioTrackManagerInterface> trackManager):
    m_attachmentDocker{attachmentDocker},
    m_asrEngine{asrEngine},
    m_bringupPlayer{mediaPlayer},
    m_trackManager{trackManager},
        m_currentSourceId{utils::mediaPlayer::MediaPlayerInterface::ERROR},
    m_playFlag{IDLE} {

}

Bringup::~Bringup() { }

void Bringup::onTrackChanged(utils::channel::FocusState newTrace) { 
    AISDK_INFO(LX("onTrackChanged").d("FocusState", newTrace));
    m_Tracep = newTrace;
   switch(newTrace) {
        case utils::channel::FocusState::FOREGROUND:
        {
            switch(m_status){
            case utils::bringup::eventType::BRINGUP_RESTART_CONFIG:
                in->open("/cfg/sai_config/restartconfig.mp3", std::ifstream::in);
            break;
            case utils::bringup::eventType::BRINGUP_START_BIND:
                in->open("/cfg/sai_config/startbind.mp3", std::ifstream::in);
            break;
            case utils::bringup::eventType::BRINGUP_BIND_COMPLETE:
                in->open("/cfg/sai_config/bind_ok.mp3", std::ifstream::in);
            break;
            case utils::bringup::eventType::BRINGUP_GMJK_START:
                in->open("/cfg/sai_config/gmjkstart.mp3", std::ifstream::in);
            break;
            case utils::bringup::eventType::MICROPHONE_OFF:
                in->open("/cfg/sai_config/mic_close.mp3", std::ifstream::in);
            break;
             case utils::bringup::eventType::MICROPHONE_ON:
                in->open("/cfg/sai_config/mic_open.mp3", std::ifstream::in);
            break;
             case utils::bringup::eventType::NET_DISCONNECTED:
                in->open("/cfg/sai_config/net_connecting.mp3", std::ifstream::in);
             break;
             case utils::bringup::eventType::BRINGUP_UPGRADE_START:
                in->open("/cfg/sai_config/upgrade_start.mp3", std::ifstream::in);
             break;
            default:
            break; 

            if(!in->is_open()) {
                AISDK_ERROR(LX("onTrackChanged").d("reason", "notFileBeOpened"));
                return;
            }
                    
            }            
                        

            m_currentSourceId = m_bringupPlayer->setSource(in, false);
            AISDK_INFO(LX("onTrackChanged").d("m_currentSourceId", m_currentSourceId));
      
            m_bringupPlayer->play(m_currentSourceId);
        }
        break;
        case utils::channel::FocusState::BACKGROUND:
        case utils::channel::FocusState::NONE:
        AISDK_INFO(LX("onTrackChangedNone").d("m_currentSourceId", m_currentSourceId));
                m_bringupPlayer->stop(m_currentSourceId);

        break;
   }
}

void Bringup::onPlaybackStarted(SourceId id) {
    // no-op
    m_playFlag = PLAY_START_FLAG;
    AISDK_INFO(LX("|||| onPlaybackStarted").d("m_playFlag", m_playFlag));

}

void Bringup::onPlaybackFinished(SourceId id) {
    // no-op
    AISDK_INFO(LX("onPlaybackFinished").d("GM SourceId", id));
    m_playFlag = PLAY_FINISHED_FLAG;
    in->close();
    m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
}

void Bringup::onPlaybackError(SourceId id, const utils::mediaPlayer::ErrorType& type, std::string error) {
    // no-op
    AISDK_INFO(LX("onPlaybackError").d("GM SourceId", id));
    in->close();
    m_playFlag = PLAY_ERROR_FLAG; 

}

void Bringup::onPlaybackStopped(SourceId id) {
    // no-op
    AISDK_INFO(LX("onPlaybackStopped").d("GM SourceId", id));
    in->close();
    m_playFlag = PLAY_STOP_FLAG; 

}


void Bringup::init() {
    m_bringupPlayer->setObserver(shared_from_this());
}


bool Bringup::start(utils::bringup::eventType type) {
    m_status = type;

    AISDK_INFO(LX("start").d("m_currentSourceId", m_currentSourceId)
                          .d("m_playFlag", m_playFlag)
                          .d("m_Tracep", m_Tracep));
    if(m_Tracep != utils::channel::FocusState::NONE)
    {
        m_trackManager->releaseChannel(CHANNEL_NAME, shared_from_this());
    }
    if(!m_trackManager->acquireChannel(CHANNEL_NAME, shared_from_this(), BRINGUP_NAME)) {
        AISDK_INFO(LX("startFailed")
    		    .d("reason", "CouldNotAcquireChannel"));
    }

    return true;
}

}
}  // namespace modules
}  // namespace aisdk

