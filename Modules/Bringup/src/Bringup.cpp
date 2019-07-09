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
#include "string.h"
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

void Bringup::inOpenFile(const char *filePath)
{
    in->open(filePath, std::ifstream::in);
    if(!in->is_open())
    {
        AISDK_ERROR(LX("onTrackChanged").d("reason", "notFileBeOpened"));
        //return;
    }
    else
    {
        m_currentSourceId = m_bringupPlayer->setSource(in, false);
        AISDK_INFO(LX("onTrackChanged").d("m_currentSourceId", m_currentSourceId));
    }
}

void Bringup::onTrackChanged(utils::channel::FocusState newTrace) { 
    AISDK_INFO(LX("onTrackChanged").d("FocusState", newTrace));
    m_Tracep = newTrace;
   switch(newTrace) {
        case utils::channel::FocusState::FOREGROUND:
        {
            switch(m_status){
            case utils::bringup::eventType::BRINGUP_RESTART_CONFIG:
                inOpenFile("/cfg/sai_config/restartconfig.mp3");
                //in->open("/cfg/sai_config/restartconfig.mp3", std::ifstream::in);
            break;
            case utils::bringup::eventType::BRINGUP_START_BIND:
                inOpenFile("/cfg/sai_config/startbind.mp3");
                //in->open("/cfg/sai_config/startbind.mp3", std::ifstream::in);
            break;
            case utils::bringup::eventType::BRINGUP_BIND_COMPLETE:
                inOpenFile("/cfg/sai_config/bind_ok.mp3");
                //in->open("/cfg/sai_config/bind_ok.mp3", std::ifstream::in);
            break;
            case utils::bringup::eventType::BRINGUP_GMJK_START:
                inOpenFile("/cfg/sai_config/gmjkstart.mp3");
                //in->open("/cfg/sai_config/gmjkstart.mp3", std::ifstream::in);
            break;
            case utils::bringup::eventType::MICROPHONE_OFF:
                inOpenFile("/cfg/sai_config/mic_close.mp3");
                //in->open("/cfg/sai_config/mic_close.mp3", std::ifstream::in);
            break;
             case utils::bringup::eventType::MICROPHONE_ON:
                inOpenFile("/cfg/sai_config/mic_open.mp3");
                //in->open("/cfg/sai_config/mic_open.mp3", std::ifstream::in);
            break;
             case utils::bringup::eventType::NET_DISCONNECTED:
                inOpenFile("/cfg/sai_config/net_connecting.mp3");
                //in->open("/cfg/sai_config/net_connecting.mp3", std::ifstream::in);
             break;
             case utils::bringup::eventType::BRINGUP_UPGRADE_START:
                inOpenFile("/cfg/sai_config/upgrade_start.mp3");
                //in->open("/cfg/sai_config/upgrade_start.mp3", std::ifstream::in);
             break;
             case utils::bringup::eventType::BRINGUP_PULSE_SCORE:
             {
                #if 1
                char contentId[37];
                //char currentContent[1024] = "文本播放成功.";
                CreateRandomUuid(contentId);
                auto writer = m_attachmentDocker->createWriter(contentId);
                auto reader = m_attachmentDocker->createReader(contentId, utils::sharedbuffer::ReaderPolicy::BLOCKING);
                AISDK_DEBUG(LX("start").d("PULSESCORE:", m_ttsTxt));
                m_asrEngine->acquireTextToSpeech(m_ttsTxt, std::move(writer));

                utils::AudioFormat format {
                    .encoding = aisdk::utils::AudioFormat::Encoding::LPCM,
                    .endianness = aisdk::utils::AudioFormat::Endianness::LITTLE,
                    .sampleRateHz = 16000,
                    .sampleSizeInBits = 16,
                    .numChannels = 1,
                    .dataSigned = true };

                m_currentSourceId = m_bringupPlayer->setSource(std::move(reader), &format);
                
                #endif  
             }
             break;
            case utils::bringup::eventType::ALARM_ACK:
             {
                char contentId[37];
                CreateRandomUuid(contentId);
                auto writer = m_attachmentDocker->createWriter(contentId);
                auto reader = m_attachmentDocker->createReader(contentId, utils::sharedbuffer::ReaderPolicy::BLOCKING);
                m_asrEngine->acquireTextToSpeech(m_ttsTxt, std::move(writer));

                utils::AudioFormat format {
                    .encoding = aisdk::utils::AudioFormat::Encoding::LPCM,
                    .endianness = aisdk::utils::AudioFormat::Endianness::LITTLE,
                    .sampleRateHz = 16000,
                    .sampleSizeInBits = 16,
                    .numChannels = 1,
                    .dataSigned = true };

                m_currentSourceId = m_bringupPlayer->setSource(std::move(reader), &format);
             }
             break;
            default:
            break; 

            //if(!in->is_open()) {
            //    AISDK_ERROR(LX("onTrackChanged").d("reason", "notFileBeOpened"));
            //    return;
            //}
                    
            }            
                        

            //m_currentSourceId = m_bringupPlayer->setSource(in, false);
            //AISDK_INFO(LX("onTrackChanged").d("m_currentSourceId", m_currentSourceId));
      
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

/* Create random UUID */
/* @param uuid[37] - buffer to be filled with the uuid string */
const char* Bringup::CreateRandomUuid(char *uuid)
{
    const char *c = "89ab";
    char *p = uuid;
    int n;
    for( n = 0; n < 16; ++n ) {
        int b = rand()%255;
        switch(n)
        {
            case 6:
                sprintf(p, "4%x", b%15 );
            break;
            case 8:
                sprintf(p, "%c%x", c[rand()%strlen(c)], b%15 );
            break;
            default:
                sprintf(p, "%02x", b);
            break;
        }
        p += 2;
        switch(n) {
            case 3:
            case 5:
            case 7:
            case 9:
                *p++ = '-';
                break;
        }
    }
    *p = 0;
    return uuid;
}

void Bringup::init() {
    m_bringupPlayer->setObserver(shared_from_this());
}


bool Bringup::start(utils::bringup::eventType type, std::string ttsTxt) {
    m_status = type;
    m_ttsTxt = ttsTxt;
    AISDK_INFO(LX("start").d("m_currentSourceId", m_currentSourceId)
                          .d("m_playFlag", m_playFlag)
                          .d("m_Tracep", m_Tracep)
                          .d("ttsTxt", ttsTxt)
                          .d("m_ttsTxt", m_ttsTxt));
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

